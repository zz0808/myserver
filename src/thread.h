#ifndef THREAD_H_
#define THREAD_H_

#include "noncopyable.h"
#include <thread>
#include <string>
#include <functional>
#include <memory>
#include <pthread.h>

namespace dserver {

class Thread : public Noncopyable {
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb, const std::string& thread_name = "unknown");
    ~Thread();
    void join();

private:
    static void* Run(void* args);

public:
    static Thread* GetThis();
    static void SetName(const std::string& name);
    static const std::string& GetName();
    static pid_t GetThreadId();

private:
    // 线程真实id, pthread_self获得的不同进程下线程的id可能是相同的
    pid_t thread_id_;
    std::string thread_name_;
    pthread_t thread_;
    std::function<void()> cb_;
};

} // namespace dserver 

#endif