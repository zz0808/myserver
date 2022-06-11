#ifndef IOMANAGER_H_
#define IOMANAGER_H_

#include "scheduler.h"
#include "timer.h"

namespace dserver {

class IOManager : public Scheduler, public TimerManager {
public:
    typedef std::shared_ptr<IOManager> ptr;
    
    enum Event {
        NONE = 0,
        READ = 1,
        WRITE = 2
    };

private:
    // socket fd 上下文，包括fd的值，fd上的事件，以及fd的读写事件上下文
    struct FdContext {
        std::mutex mtx;
        // 事件上下文类
        struct EventContext {
            // 执行事件回调的调度器
            Scheduler* scheduler = nullptr;
            // 事件回调协程
            Fiber::ptr f;
            // 事件回调函数
            std::function<void()> cb;
        };
        // 获取事件上下文
        EventContext& get_event_context(Event e);
        // 重置事件上下文
        void reset_event_context(EventContext& ctx);
        // 触发事件
        void triger_event(Event e);

        EventContext read;
        EventContext write;
        int fd = 0;
        // 该fd添加了哪些事件的回调函数，或者说该fd关心哪些事件
        Event events = NONE;
        std::mutex mtx;
    };

public:
    IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");
    ~IOManager();
    int add_event(int fd, Event e, std::function<void()> cb);
    int del_event(int fd, Event e);
    bool cancel_event(int fd, Event e);
    bool cancel_all(int fd);

    static IOManager* GetThis();

protected:
    void tickle() override;
    bool is_stopping() override;
    void idle() override;
    bool is_stopping(uint64_t timeout);
    // 当有定时器插入到头部时，要重新更新epoll_wait的超时时间，这里是唤醒idle协程以便于使用新的超时时间
    void on_timer_inserted_at_front() override;
    void context_resize(size_t size);

private:
    int epfd_ = 0;
    int tickle_fd[2];
    std::atomic<size_t> pending_event_count_{0};
    std::mutex mtx_;
    std::vector<FdContext*> fd_contexts_;
    
};

} // namespace dserver 

#endif