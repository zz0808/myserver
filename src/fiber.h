#ifndef FIBER_H_
#define FIBER_H_
 
// 每个线程第一个创建的协程为主协程，负责子协程的创建，调度和回收       
#include <memory>
#include <functional>
#include <ucontext.h>

namespace dserver {

class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State {
        READY = 0,
        RUNNING = 1,
        TERM
    };
    Fiber(std::function<void()> cb, uint32_t stack_sz = 128 * 1024, bool run_in_scheduler = false);
    ~Fiber();
    void yield();
    void resume();
    void reset(std::function<void()> cb);
    uint64_t GetId() { return id_; }
    State GetState() { return state_; }

    static void SetThis(Fiber* f);
    static Fiber::ptr GetThis();
    static void MainFunc();
    // 获取当前运行协程id
    static uint64_t GetFiberId();

private:
    Fiber();

private:
    uint64_t id_ = 0;
    uint32_t stack_size_ = 0;
    State state_ = READY;
    ucontext_t ctx_;
    void* stack_ = nullptr;
    std::function<void()> cb_;
    bool run_in_scheduler_ = false;
};

} // namespace dserver 

#endif