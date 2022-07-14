#include "fiber.h"

namespace server {

static Logger::ptr g_logger = LOG_NAME("system");

static std::atomic<uint64_t> s_fiber_id{0};
static std::atomic<uint64_t> s_fiber_count{0};

static thread_local Fiber* t_fiber = nullptr;
static thread_local Fiber::ptr t_thread_fiber = nullptr;

Fiber::Fiber() {
    SetThis(this);
    state_ = RUNNING;
    if (getcontext(&ctx_)) {
        // TODO
    }
    s_fiber_count++;
    fiber_id_ = s_fiber_id++;
}

Fiber::Fiber(std::function<void()> cb, uint32_t stack_size, bool run_in_fiber)
    : fiber_id_(s_fiber_id++)
    , cb_(cb)
    , run_in_fiber_(run_in_fiber) {
    s_fiber_count++;
    stack_size_ = stack_size >= 0 ? stack_size : 1024 * 1024;
    stack_ = malloc(stack_size_);

    if (getcontext(&ctx_)) {
        // TODO
    }
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size_;
    makecontext(&ctx_, &Fiber::MainFunc, 0);

    LOG_DEBUG(g_logger) << "Fiber::Fiber() id = " << fiber_id_;
}

Fiber::~Fiber() {
    LOG_DEBUG(g_logger) << "Fiber::~Fiber() id = " << fiber_id_;
    --s_fiber_count;
    if (stack_) {
        assert(state_ == TERM);
        free(stack_);
    } else {
        assert(state_ == RUNNING);
        Fiber* cur = t_fiber;
        if (cur == this) {
            SetThis(nullptr);
        }
    }
}

void Fiber::reset(std::function<void()> cb) {
    assert(stack_ != nullptr);
    assert(state_ == TERM);
    cb_ = cb;
    if (getcontext(&ctx_)) {
        // TODO
    }
    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stack_;
    ctx_.uc_stack.ss_size = stack_size_;

    makecontext(&ctx_, &Fiber::MainFunc, 0);
    state_ = READY;
}

void Fiber::resume() {
    assert(state_ == READY);
    SetThis(this);
    state_ = RUNNING;

    if (run_in_fiber_) {
        // if (swapcontext(&(Fiber::)))
    } else {
        if (swapcontext(&(t_thread_fiber->ctx_), &ctx_)) {
            // TODO
        }
    }
}

void Fiber::yield() {
    assert(state_ == RUNNING || state_ == TERM);
    SetThis(t_thread_fiber.get());
    if (state_ == RUNNING) {
        state_ = READY;
    }
    if (run_in_fiber_) {

    } else {
        if (swapcontext(&ctx_, &(t_thread_fiber->ctx_))) {

        }
    }
}

void Fiber::SetThis(Fiber* f) {
    t_fiber = f;
}

Fiber::ptr Fiber::GetThis() {
    if (t_fiber) {
        return t_fiber->shared_from_this();
    }
    Fiber::ptr main_fiber(new Fiber);
    assert(main_fiber.get() == t_fiber);
    t_thread_fiber = main_fiber;
    return t_thread_fiber->shared_from_this();
}

uint64_t Fiber::TotalFibers() {
    // TODO
}

void Fiber::MainFunc() {
    Fiber::ptr fiber = GetThis();
    assert(fiber);
    fiber->cb_();
    fiber->cb_ = nullptr;
    fiber->state_ = TERM;

    auto raw_ptr = fiber.get();
    fiber.reset();
    raw_ptr->yield();
}

uint64_t Fiber::GetFiberId() {
    if (t_fiber) {
        return t_fiber->getFiberId();
    }
    return 0;
}

} // namespace server
