#include "fiber.h"
#include <queue>
#include <vector>
#include <atomic>
#include <mutex>
#include <assert.h>
#include <iostream>
#include "lock.h"
#include "scheduler.h"

namespace dserver {

static thread_local Fiber* t_fiber = nullptr;
static thread_local std::shared_ptr<Fiber> t_thread_fiber = nullptr;
static std::atomic<uint64_t> cur_id{0};
static std::atomic<uint64_t> total_id{0};

Fiber::Fiber() {
    SetThis(this);
    state_ = RUNNING;
    if (getcontext(&ctx_)) {
        std::cout << "getcontext" << std::endl;
    }
    id_ = ++cur_id;
    total_id++;
}

Fiber::Fiber(std::function<void()> cb, uint32_t stack_sz, bool run_in_scheduler) 
    : stack_size_(stack_sz), cb_(cb), run_in_scheduler_(run_in_scheduler) {
    SetThis(this);
    state_ = READY;
    stack_ = malloc(stack_sz);
    if (getcontext(&ctx_)) {
        std::cout << "getcontext" << std::endl;
    }
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size;
    makecontext(&ctx_, Fiber::MainFunc, 0);

    id_ = ++cur_id;
    total_id++;
}

Fiber::~Fiber() {
    if (stack_) {
        assert(state_ == TERM);
        free(stack_);
        if (cb_) {
            cb_ = nullptr;
        }
    }else {
        assert(state_ == RUNNING);
        if (t_fiber == this) {
            SetThis(this);
        }
    }
    total_id--;
    std::cout << "~Fiber " << id_ << std::endl;   
}

void Fiber::yield() {
    // 在 RUNNING 和 TERM状态可以进行yield
    assert(state_ != READY);
    if (state_ == RUNNING) {
        state_ = READY;
    }
    SetThis(t_thread_fiber.get());
    // 如果该协程参与调度，则应切换到调度协程
    if (run_in_scheduler_) {
        // TODO
        // if (swapcontext(&ctx_, ))
    } else {
        if (swapcontext(&ctx_, &(t_thread_fiber->ctx_))) {
           std::cout << "swapcontext fail " << id_ << std::endl;    
        }
    }
     std::cout << "yield " << id_ << id_ << std::endl;    
}

void Fiber::resume() {
    assert(state_ == READY);
    SetThis(this);
    state_ = RUNNING;
    
    if (run_in_scheduler_) {
        // TODO
    } else {
        if (swapcontext(&(t_thread_fiber->ctx_), &ctx_)) {
           std::cout << "swapcontext fail " << id_ << std::endl;    
        }
    }
}

void Fiber::reset(std::function<void()> cb) {
    assert(stack_);
    assert(state_ == TERM);
    cb_ = cb;
    if (getcontext(&ctx_)) {
        std::cout << "getcontext fail " << id_ << std::endl;  
    }
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size;

    makecontext(&ctx_, Fiber::MainFunc, 0);
    state_ = READY;
}

uint64_t Fiber::total_fibers() const {
    return total_id;
}

Fiber* Fiber::GetThis() {
    if (t_fiber != nullptr) {
        return t_fiber;
    }
    Fiber::ptr new_fiber(new Fiber);
    // t_fiber = new_fiber.get();
    t_thread_fiber = new_fiber;
    return t_fiber;
}

void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

void Fiber::MainFunc() {
    Fiber* f = GetThis();
    assert(f);
    std::function<void()> cb;
    cb = f->cb_;
    cb();
    f->cb_ = nullptr;
    f->state_ = TERM;
    f->yield();
}

} // namespace dserver