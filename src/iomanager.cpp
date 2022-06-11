#include "iomanager.h"
#include <iostream>
#include <assert.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

namespace dserver {

IOManager::FdContext::EventContext& IOManager::FdContext::get_event_context(Event e) {
    switch (e) {
    case IOManager::READ:
        return read;
    case IOManager::WRITE:
        return write;
    default:
        std::cout << "getcontex" << std::endl;
        assert(0);
    }
    throw std::invalid_argument("getContext invalid event");
}

void IOManager::FdContext::reset_event_context(EventContext& ctx) {
    ctx.scheduler = nullptr;
    ctx.f.reset();
    ctx.cb = nullptr;
}

void IOManager::FdContext::triger_event(Event e) {
    assert(events & e); // 待触发的事件应注册过
    events = (Event)(events & ~e);
    // 调度对应的协程
    EventContext& ctx = get_event_context(e);
    if (ctx.cb) {
        ctx.scheduler->schedule(ctx.cb);
    } else if (ctx.f) {
        ctx.scheduler->schedule(ctx.f);
    }
    reset_event_context(ctx);
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
    : Scheduler(threads, use_caller, name) {
    epfd_ = epoll_create(5000);
    assert(epfd_ > 0);
    
    int res = pipe(tickle_fd);
    assert(!res);

    epoll_event event;
    memset(&event, 0, sizeof(epoll_event));
    event.events = EPOLLIN | EPOLLOUT;
    event.data.fd = tickle_fd[0];

    // 非阻塞
    res = fcntl(tickle_fd[0], F_SETFL, O_NONBLOCK);
    assert(!res);

    res = epoll_ctl(epfd_, EPOLL_CTL_ADD, tickle_fd[0], &event);
    assert(!res);

    context_resize(32);

    start();
}

IOManager::~IOManager() {
    stop();
    close(epfd_);
    close(tickle_fd[0]);
    close(tickle_fd[1]);

    for (uint32_t i = 0; i < fd_contexts_.size(); i++) {
        if (!fd_contexts_[i]) {
            fd_contexts_[i] = new FdContext;
            fd_contexts_[i]->fd = i;
        }
    }
}

int IOManager::add_event(int fd, Event e, std::function<void()> cb) {
    FdContext* fd_ctx = nullptr;
    std::shared_lock<std::mutex> read_lock(mtx_);

    if ((int)fd_contexts_.size() > fd) {
        fd_ctx = fd_contexts_[fd];
        read_lock.unlock();
    } else {
        read_lock.unlock();
        std::unique_lock<std::mutex> write_lock(mtx_);
        context_resize(fd * 1.5);
        fd_ctx = fd_contexts_[fd];
    }

    // 同一个fd不允许重复添加相同的事件
    std::lock_guard<std::mutex> locker(fd_ctx->mtx);
    assert(!(fd_ctx->events & e));

    // 将新的事件加入epoll_wait，使用epoll_event的私有指针存储FdContext的位置
    int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events = EPOLLET | fd_ctx->events | e;
    epevent.data.ptr = fd_ctx;

    int res = epoll_ctl(epfd_, op, fd_ctx->fd, &epevent);
    if (!res) {
        std::cout << "add_event fail" << std::endl;
        return -1;
    }
    // 待执行IO事件数加1
    pending_event_count_++;
    // 找到这个fd的event事件对应的EventContext，对其中的scheduler, cb, fiber进行赋值
    fd_ctx->events = (Event)(fd_ctx->events | e);
    FdContext::EventContext &event_ctx = fd_ctx->get_event_context(e);

    // 赋值scheduler和回调函数，如果回调函数为空，则把当前协程当成回调执行体
    event_ctx.scheduler = Scheduler::GetThis();
    if (cb) {
        event_ctx.cb.swap(cb);
    } else {
        event_ctx.f = Fiber::GetThis();
        assert(event_ctx.f->GetState() == Fiber::RUNNING);
    }
    
    return 0;
}





} // namespace dserver 