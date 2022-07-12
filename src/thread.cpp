#include "thread.h"

namespace server {

static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "default";
static server::Logger::ptr g_logger = LOG_NAME("system");

Thread::Thread(std::function<void()> cb, const std::string& name) 
    : cb_(cb)
    , name_(name) {
    int res = pthread_create(&thread_, nullptr, &Thread::run, this);
    if (res) {
        LOG_ERROR(g_logger) << "pthread_create thread fail, res=" << res
                                  << " name=" << name_;
        throw std::logic_error("pthread create");    
    }
    sem_.wait();
}

Thread::~Thread() {
    if (thread_) {
        pthread_detach(thread_);
    }
}

void Thread::join() {
    if (thread_) {
        int res = pthread_join(thread_, nullptr);
        if (res) {
            LOG_ERROR(g_logger)  << "pthread_join thread fail, res=" << res
                                 << " name=" << name_;
            throw std::logic_error("pthread join");      
        }
        thread_ = 0;
    }
}

Thread* Thread::GetThis() {
    return t_thread;
}

const std::string& Thread::GetName() {
    return t_thread_name;
}

void Thread::SetName(const std::string& name) {
    if (name.empty()) {
        return;
    }
    if (t_thread) {
        t_thread->name_ = name;
    }
    t_thread_name = name;
}

void* Thread::run(void* args) {
    Thread* thread = (Thread*)args;
    t_thread = thread;
    t_thread_name = thread->name_;
    thread->thread_id_ = server::GetThreadId();
    pthread_setname_np(pthread_self(), thread->name_.substr(0, 15).c_str());
    
    std::function<void()> cb;
    cb.swap(thread->cb_);
    thread->sem_.notify();
    cb();
    return 0;
}

} // namespace server 