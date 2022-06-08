#ifndef FIBER_H_
#define FIBER_H_
 
// 每个线程第一个创建的协程为主协程，负责子协程的创建，调度和回收       
#include <memory>
#include <functional>
#include <ucontext.h>

namespace dserver {

static uint32_t stack_size = 128 * 1024; // 128K

class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    typedef std::shared_ptr<Fiber> ptr;

    enum State {
        READY = 0,
        RUNNING = 1,
        TERM = 2
    };

    Fiber(std::function<void()> cb, uint32_t stack_sz = 128 * 1024, bool run_in_scheduler = false);
    ~Fiber();
    void yield();
    void resume();
    uint64_t GetId() const { return id_; }
    void reset(std::function<void()> cb);
    State getState() const { return state_; }
    uint64_t total_fibers() const;

    static Fiber* GetThis();
    static void SetThis(Fiber* f);
    static void MainFunc();


private:
    Fiber();

private:
    uint64_t id_;
    State state_;
    ucontext_t ctx_;
    void* stack_;
    uint32_t stack_size_;
    std::function<void()> cb_;
    // 该协程是否参与调度
    bool run_in_scheduler_;
};

} // namespace dserver 

#endif