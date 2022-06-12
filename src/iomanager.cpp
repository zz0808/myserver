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
    std::shared_lock<std::shared_mutex> read_lock(mtx_);

    if ((int)fd_contexts_.size() > fd) {
        fd_ctx = fd_contexts_[fd];
        read_lock.unlock();
    } else {
        read_lock.unlock();
        std::unique_lock<std::shared_mutex> write_lock(mtx_);
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

bool IOManager::del_event(int fd, Event e) {
    std::shared_lock<std::shared_mutex> read_lock(mtx_);
    if ((int)fd_contexts_.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = fd_contexts_[fd];
    read_lock.unlock();
    
    std::lock_guard<std::mutex> locker(fd_ctx->mtx);
    if (!(fd_ctx->events & e)) {
        return false;
    }

    // 清除指定的事件，表示不关心这个事件了，如果清除之后结果为0，则从epoll_wait中删除该文件描述符
    Event new_events = (Event)(fd_ctx->events & ~e);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int res = epoll_ctl(epfd_, op, fd_ctx->fd, &epevent);
    if (!res) {
        return false;
    }

    // 待执行事件数减1
    pending_event_count_--;
    // 重置该fd对应的event事件上下文
    fd_ctx->events = new_events;
    FdContext::EventContext& event_ctx = fd_ctx->get_event_context(e);
    fd_ctx->reset_event_context(event_ctx);

    return true;
}

bool IOManager::cancel_event(int fd, Event e) {
    // 找到fd对应的FdContext
    std::shared_lock<std::shared_mutex> read_lock(mtx_);
    if ((int)fd_contexts_.size() <= fd) {
        return false;
    }
    FdContext* fd_ctx = fd_contexts_[fd];
    read_lock.unlock();

    std::lock_guard<std::mutex> locker(fd_ctx->mtx);
    if (!(fd_ctx->events & e)) {
        return false;
    }

    // 删除事件
    Event new_events = (Event)(fd_ctx->events & ~e);
    int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

    epoll_event epevent;
    epevent.events = EPOLLET | new_events;
    epevent.data.ptr = fd_ctx;

    int res = epoll_ctl(epfd_, op, fd_ctx->fd, &epevent);
    if (!res) {
        return false;
    }
    fd_ctx->triger_event(e);
    pending_event_count_--;

    return true;   
}

bool IOManager::cancel_all(int fd) {
    std::shared_lock<std::shared_mutex> read_lock(mtx_);
    if ((int)(fd_contexts_.size() <= fd)) {
        return false;
    }
    FdContext* fd_ctx = fd_contexts_[fd];
    read_lock.unlock();

    std::lock_guard<std::mutex> locker(fd_ctx->mtx);
    if (!fd_ctx->events) {
        return false;
    }
    // 删除全部事件
    int op = EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events = 0;
    epevent.data.ptr = fd_ctx;

    int res = epoll_ctl(epfd_, op, fd_ctx->fd, &epevent);
    if (!res) {
        return false;
    }

    // 触发全部已注册的事件
    if (fd_ctx->events & READ) {
        fd_ctx->triger_event(READ);
        pending_event_count_--;
    } 
    if (fd_ctx->events & WRITE) {
        fd_ctx->triger_event(WRITE);
        pending_event_count_--;
    }
    assert(fd_ctx->events == 0);

    return true;
}

IOManager* IOManager::GetThis() {
    return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

/**
 * 通知调度协程、也就是Scheduler::run()从idle中退出
 * Scheduler::run()每次从idle协程中退出之后，都会重新把任务队列里的所有任务执行完了再重新进入idle
 * 如果没有调度线程处理于idle状态，那也就没必要发通知了
 */
void IOManager::tickle() {
    std::cout << "tickle" << std::endl;
    if (!has_idle_threads()) {
        return;
    }
    
    int res = write(tickle_fd[1], "T", 1);
    assert(res == 1);
}

bool IOManager::is_stopping() {
    uint64_t timeout = 0;
    return is_stopping(timeout);
}

/**
 * 调度器无调度任务时会阻塞idle协程上，对IO调度器而言，idle状态应该关注两件事，一是有没有新的调度任务，对应Schduler::schedule()，
 * 如果有新的调度任务，那应该立即退出idle状态，并执行对应的任务；二是关注当前注册的所有IO事件有没有触发，如果有触发，那么应该执行
 * IO事件对应的回调函数
 */
void IOManager::idle() {
    std::cout << "idle" << std::endl;
    // 一次epoll_wait最多检测256个就绪事件，如果就绪事件超过了这个数，那么会在下轮epoll_wati继续处理
    const uint64_t MAX_EVNETS = 256;

    epoll_event *events = new epoll_event[MAX_EVNETS]();
    std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *ptr) {
        delete[] ptr;
    });
    while (true) {
        // 获取下一个定时器的超时时间，顺便判断调度器是否停止
        uint64_t next_timeout = 0;
        if(is_stopping(next_timeout)) {
            std::cout << "name=" << GetSchedulerName() << "idle stopping exit";
            break;
        }
        // 阻塞在epoll_wait上，等待事件发生或定时器超时
        int res = 0;
        do {
            static const int MAX_TIMEOUT = 5000;
            if (next_timeout > 0) {
                next_timeout = std::min((int)next_timeout, MAX_TIMEOUT);
            } else {
                next_timeout = MAX_TIMEOUT;
            }
            res = epoll_wait(epfd_, events, MAX_EVNETS, (int)next_timeout);
            if(res < 0 && errno == EINTR) {
                continue;
            } else {
                break;
            }
        } while (true);

        // 收集所有已超时的定时器，执行回调函数
        std::vector<std::function<void()>> cbs;
        list_all_expired_cbs(cbs);
        if (!cbs.empty()) {
            for (const auto& cb : cbs) {
                schedule(cb);
            }
            cbs.clear();
        }

        // 遍历所有发生的事件，根据epoll_event的私有指针找到对应的FdContext，进行事件处理
        for (uint32_t i = 0; i < res; i++) {
            epoll_event& event = events[i];
            if (event.data.fd == tickle_fd[0]) {
                // ticklefd[0]用于通知协程调度，这时只需要把管道里的内容读完即可
                uint8_t dummy[256];
                while(read(tickle_fd[0], dummy, sizeof(dummy)) > 0);
                continue;
            }
            FdContext* fd_ctx = (FdContext*)event.data.ptr;
            std::lock_guard<std::mutex> locker(fd_ctx->mtx);
            /**
             * EPOLLERR: 出错，比如写读端已经关闭的pipe
             * EPOLLHUP: 套接字对端关闭
             * 出现这两种事件，应该同时触发fd的读和写事件，否则有可能出现注册的事件永远执行不到的情况
             */ 
            if (event.events & (EPOLLERR | EPOLLHUP)) {
                event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
            }
            int real_events = NONE;
            if (event.events & EPOLLIN) {
                real_events |= READ;
            }
            if (event.events & EPOLLOUT) {
                real_events |= WRITE;
            }

            if ((fd_ctx->events & real_events) == NONE) {
                continue;
            }

            // 剔除已经发生的事件，将剩下的事件重新加入epoll_wait
            int left_events = (fd_ctx->events & ~real_events);
            int op = (left_events ? EPOLL_CTL_ADD : EPOLL_CTL_DEL);
            event.events = EPOLLET | left_events;

            int res2 = epoll_ctl(epfd_, op, fd_ctx->fd, &event);
            if (res2) {
                continue;
            }
            // 处理已经发生的事件，也就是让调度器调度指定的函数或协程
            if (real_events & READ) {
                fd_ctx->triger_event(READ);
                pending_event_count_--;
            }
            if (real_events & WRITE) {
                fd_ctx->triger_event(WRITE);
                pending_event_count_--;
            }
        }
        /**
         * 一旦处理完所有的事件，idle协程yield，这样可以让调度协程(Scheduler::run)重新检查是否有新任务要调度
         * 上面triggerEvent实际也只是把对应的fiber重新加入调度，要执行的话还要等idle协程退出
         */ 
        Fiber::ptr cur = Fiber::GetThis();
        auto raw_ptr   = cur.get();
        cur.reset();

        raw_ptr->yield();
    }
}

bool IOManager::is_stopping(uint64_t timeout) {
    // 对于IOManager而言，必须等所有待调度的IO事件都执行完了才可以退出
    // 增加定时器功能后，还应该保证没有剩余的定时器待触发
    timeout = get_next_timer();
    return timeout == ~0ull && pending_event_count_ == 0 && Scheduler::is_stopping();
}

void IOManager::on_timer_inserted_at_front() {
    tickle();
}

void IOManager::context_resize(size_t size) {
    fd_contexts_.resize(size);

    for (size_t i = 0; i < fd_contexts_.size(); ++i) {
        if (!fd_contexts_[i]) {
            fd_contexts_[i] = new FdContext;
            fd_contexts_[i]->fd = i;
        }
    }
}
} // namespace dserver 