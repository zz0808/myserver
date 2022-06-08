#include "thread.h"
#include <sys/prctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>

namespace dserver {

static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "unknown";

Thread::Thread(std::function<void()> cb, const std::string& thread_name)
    : thread_id_(-1), thread_(0), thread_name_(thread_name), cb_(cb) {
    int res = pthread_create(&thread_, nullptr, &Thread::Run, this);
    if (res) {
        // TOOD
        assert(0);
    }
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
            // TODO
            assert(0);
        }
        thread_ = 0;
    }
}

Thread* Thread::GetThis() {
    return t_thread;
}

void Thread::SetName(const std::string& name) {
    if (name.empty()) {
        return;
    }
    if (t_thread) {
        t_thread->thread_name_ = name;
    }
    t_thread_name = name;
    prctl(PR_SET_NAME, name.c_str());

}

const std::string& Thread::GetName() {
    return t_thread_name;
}

pid_t Thread::GetThreadId() {
    if (t_thread) {
        return t_thread->thread_id_;
    }
    return syscall(SYS_gettid);
}

void* Thread::Run(void* args) {
    Thread* tmp = (Thread*)args;
    t_thread = tmp;
    tmp->thread_id_ = syscall(SYS_gettid);
    t_thread_name = tmp->thread_name_;
    prctl(PR_SET_NAME, tmp->thread_name_.c_str());
    std::function<void()> cb;
    cb.swap(tmp->cb_);
    cb();
    return 0;
}

} // namespace dserver