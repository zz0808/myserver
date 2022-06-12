#include "fiber.h"
#include "scheduler.h"
#include <queue>
#include <vector>
#include <atomic>
#include <mutex>
#include <assert.h>
#include <iostream>

namespace dserver {

static std::atomic<uint64_t> s_cur_id{0};
static std::atomic<uint64_t> s_total_id{0};
static thread_local Fiber* t_fiber = nullptr;
static thread_local Fiber::ptr t_thread_fiber = nullptr;

Fiber::Fiber(std::function<void()> cb, uint32_t stack_sz, bool run_in_scheduler ) 
            : id_(s_cur_id++)
            , cb_(cb)
            , run_in_scheduler_(run_in_scheduler) {
    ++s_total_id;
    stack_size_ = stack_sz;
    stack_ = malloc(stack_size_);

    if (getcontext(&ctx_)) {
        std::cout << "getcontext" << std::endl;
        assert(0);
    }
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size_;

    makecontext(&ctx_, &(Fiber::MainFunc), 0);

    std::cout << "Fiber::Fiber() id = " << id_ << std::endl;
}

Fiber::~Fiber() {
    std::cout << "Fiber::~Fiber() id = " << id_ << std::endl;
    --s_total_id;

    if (stack_ == nullptr) { // 主协程
        assert(state_ == RUNNING);
        assert(cb_ == nullptr);
        Fiber* f = t_fiber;
        if (f == this) {
            SetThis(nullptr);
        }
    } else { // 子协程
        assert(state_ == TERM);
        free(stack_);
    }
}

void Fiber::yield() {
    assert(state_ == RUNNING || state_ == TERM);
    SetThis(t_thread_fiber.get());
    if (state_ == RUNNING) {
        state_ = READY;
    }

    if (run_in_scheduler_) {

    } else {
        if (swapcontext(&ctx_, &(t_thread_fiber->ctx_))) {
            std::cout << "swapcontext" << std::endl;
        }
    }
}

void Fiber::resume() {
    assert(state_ == READY);
    SetThis(this);
    state_ = RUNNING;

    if (run_in_scheduler_) {

    } else {
        if (swapcontext(&(t_thread_fiber->ctx_), &ctx_)) {
            std::cout << "swapcontext" << std::endl;
        }
    }
}

void Fiber::reset(std::function<void()> cb) {
    assert(stack_);
    assert(state_ == TERM);
    cb_ = cb;
    if (getcontext(&ctx_)) {
        std::cout << "getcontext" << std::endl;
    }
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size_;

    makecontext(&ctx_, &Fiber::MainFunc, 0);
    state_ = READY;
}

void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

Fiber::ptr Fiber::GetThis() {
    if (t_fiber) {
        return t_fiber->shared_from_this();
    }
    Fiber::ptr main_f(new Fiber);
    assert(t_fiber == main_f.get());
    t_thread_fiber = main_f;
    return t_fiber->shared_from_this();
}

void Fiber::MainFunc() {
    Fiber::ptr f = GetThis();
    assert(f);
    f->cb_();
    f->cb_ = nullptr;
    f->state_ = TERM;
    
    auto ptr = f.get();
    f.reset();
    ptr->yield();
}

uint64_t Fiber::GetFiberId() {
    if (t_fiber) {
        return t_fiber->id_;
    }
    return 0;
}

Fiber::Fiber() {
    SetThis(this);
    state_ = RUNNING;
    if (getcontext(&ctx_)) {
        std::cout << "getcontext" << std::endl;
        assert(0);
    }
    ++s_total_id;
    id_ = s_cur_id++;
}

} // namespace dserver