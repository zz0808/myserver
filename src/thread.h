#ifndef __THREAD_H__
#define __THREAD_H_

#include "nocopyable.h"
#include "mutex.h"
#include "log.h"
#include <functional>

namespace server {

class Thread : public Nocopyable {
public:
    typedef std::shared_ptr<Thread> ptr;

    Thread(std::function<void()> cb, const std::string& name = "default");
    ~Thread();
    pid_t getThreadId() const { return thread_id_; }
    const std::string& getThreadName() const { return name_; }
    void join();
    static Thread* GetThis();
    static const std::string& GetName();
    static void SetName(const std::string& name);

private:
    static void* run(void* args);

private:
    pid_t thread_id_ = -1;
    pthread_t thread_ = 0;
    std::function<void()> cb_;
    std::string name_;
    Semaphore sem_;
};

} // namespace server

#endif 