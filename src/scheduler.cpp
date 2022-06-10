#include "scheduler.h"
#include <assert.h>
#include "hook.h"
#include <functional>
#include <sstream>
#include <iostream>

namespace dserver {

static thread_local Scheduler* t_scheduler = nullptr;
static thread_local Fiber* t_schedule_fiber = nullptr;

Scheduler::Scheduler(uint32_t threads, bool use_caller, const std::string& name) {
    assert(threads > 0);
    use_caller_ = use_caller;
    name_ = name;

    if (use_caller_) {
        --threads;
        Fiber::GetThis();
        assert(GetThis() == nullptr); // 规定每个线程最多有一个调度器
        t_scheduler = this;

        // 调度协程
        root_fiber_.reset(new Fiber(std::bind(&Scheduler::run, this), 0, false));
        Thread::SetName(name_);
        t_schedule_fiber = root_fiber_.get();
        root_thread_id_ = Thread::GetThreadId();
    } 
    threads_num_ = threads;
}

Scheduler::~Scheduler() {
    std::cout << "Scheduler::~Scheduler()" << std::endl;
    assert(stopping_);
    if (GetThis() == this) {
        t_scheduler = nullptr;
    }
}

void Scheduler::start() {
    std::cout << "start" << std::endl;
    std::lock_guard<std::mutex> locker(mtx_);
    if (stopping_) {
        std::cout << "Scheduler is stopped" << std::endl;
        return;
    }
    assert(threads_.empty());
    threads_.resize(threads_num_);
    for (uint32_t i = 0; i < threads_num_; i++) {
        threads_[i].reset(new Thread(std::bind(&Scheduler::run, this), 
                         name_ + "_" + std::to_string(i)));
    }
}

void Scheduler::stop() {
    std::cout << "stop" << std::endl;
    if (is_stopping()) {
        return;
    }
    stopping_ = true;
    if (use_caller_) {
        assert(GetThis() == this);
    } else {
        assert(GetThis() != this);
    }
    for (uint32_t i = 0; i < threads_num_; i++) {
        tickle();
    }
    if (root_fiber_) {
        tickle();
    }
    if (root_fiber_) {
        root_fiber_->resume();
        std::cout << "root_fiber end" << std::endl;
    }
    std::vector<Thread::ptr> thrds;
    {
        std::lock_guard<std::mutex> locker(mtx_);
        thrds.swap(threads_);
    }
    for (auto& t : thrds) {
        t->join();
    }
}

Scheduler* Scheduler::GetThis() {
    return t_scheduler;
}

Fiber* Scheduler::GetMainFiber() {
    return t_schedule_fiber;
}

void Scheduler::tickle() {
    std::cout << "tickle" << std::endl;
}

void Scheduler::run() {
    std::cout << "run" << std::endl;
    // set_hook_enable(true);
    SetThis();
    if (Thread::GetThreadId() != root_thread_id_) {
        t_schedule_fiber = Fiber::GetThis().get();
    }
    Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
    Fiber::ptr cb_fiber;
    
    Task task;
    
    while (true) {
        task.reset();
        bool tickle_me = false; // 是否tickle其他线程进行任务调度
        {
            std::lock_guard<std::mutex> locker(mtx_);
            auto it = tasks_.begin();
            // 遍历所有调度任务
            while (it != tasks_.end()) {
                // 找到一个未指定线程，或是指定了当前线程的任务
                assert(it->f || it->cb);
                if(it->f && it->f->GetState() == Fiber::RUNNING) {
                    ++it;
                    continue;
                }
                
                // 当前调度线程找到一个任务，准备开始调度，将其从任务队列中剔除，活动线程数加1
                task = *it;
                tasks_.erase(it++);
                ++active_threads_;
                break;
            }
            // 当前线程拿完一个任务后，发现任务队列还有剩余，那么tickle一下其他线程
            tickle_me |= (it != tasks_.end());
        } 
        if (tickle_me) {
            tickle();
        }    
        if (task.f) {
            // resume协程，resume返回时，协程要么执行完了，要么半路yield了，总之这个任务就算完成了，活跃线程数减一
            task.f->resume();
            --active_threads_;
            task.reset();
        } else if (task.cb) {
            if (cb_fiber) {
                cb_fiber->reset(task.cb);
            } else {
                cb_fiber.reset(new Fiber(task.cb));
            }
            task.reset();
            cb_fiber->resume();
            --active_threads_;
            cb_fiber.reset();
        } else {
            // 进到这个分支情况一定是任务队列空了，调度idle协程即可
            if (idle_fiber->GetState() == Fiber::TERM) {
                // 如果调度器没有调度任务，那么idle协程会不停地resume/yield，不会结束，如果idle协程结束了，那一定是调度器停止了
                std::cout << "idle fiber term" << std::endl;
                break;
            }
            ++idle_threads_;
            idle_fiber->resume();
            --idle_threads_;
        }  
    }
}

void Scheduler::idle() {
    std::cout << "idle" << std::endl;
    while (!is_stopping()) {
        Fiber::GetThis()->yield();
    }
}

bool Scheduler::is_stopping() {
    std::lock_guard<std::mutex> locker(mtx_);
    return stopping_ && tasks_.empty() && active_threads_ == 0;
}

void Scheduler::SetThis() {
    t_scheduler = this;
}

} // namespace dserver 
