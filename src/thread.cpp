#include "thread.h"

namespace server {

static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = nullptr;

Thread::Thread(std::function<void()> cb, const std::string& name) 
    : cb_(cb)
    , name_(name) {
    if (name.empty()) {
        name_ = "UNKNOWN";
    }
    int rt = pthread_create(&thread_, nullptr, &Thread::run, this);
    if (rt) {
        throw std::logic_error("pthread_create error");
    }
    sem_.wait();
}

Thread::~Thread() {
    if (thread_) {
        pthread_detach(thread_);
    }
}

void Thread::join() {
    int rt = pthread_join(thread_, nullptr);
    if (rt) {
        throw std::logic_error("pthread_join error");
    }
    thread_ = 0;
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

} // namespace server