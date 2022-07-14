#ifndef __FIBER_H__
#define __FIBER_H__

#include "log.h"
#include <memory>
#include <functional>
#include <ucontext.h>
#include <atomic>
#include <assert.h>

namespace server {

class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State {
        READY = 0,
        RUNNING = 1,
        TERM = 2
    };

private:
    Fiber();

public:
    Fiber(std::function<void()> cb, uint32_t stack_size = 0, bool run_in_fiber = false);
    ~Fiber();
    void reset(std::function<void()> cb);
    void resume();
    void yield();
    uint64_t getFiberId() const { return fiber_id_; }
    State getState() const { return state_; }

    static void SetThis(Fiber* f);
    static Fiber::ptr GetThis();
    static uint64_t TotalFibers();
    static void MainFunc();
    static uint64_t GetFiberId();

private:
    uint64_t fiber_id_ = 0;
    uint32_t stack_size_ = 0;
    State state_ = READY;
    ucontext_t ctx_;
    void* stack_ = nullptr;
    std::function<void()> cb_;
    bool run_in_fiber_ = false;
};

} // namespace server

#endif