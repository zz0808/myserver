#ifndef THREAD_H_
#define THREAD_H_

#include "mutex.h"
#include "nocopyable.h"

namespace server {

class Thread : public Nocopyable {
public:
    typedef std::shared_ptr<Thread> ptr;

    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();
    pid_t GetId() const { return id_; }
    const std::string& getName() const { return name_; }
    void join();
    static Thread* GetThis();
    static const std::string& GetName();
    static void SetName(const std::string& name);

private:
    static void* run(void* args);
    
private:
    pid_t id_ = 0;
    pthread_t thread_ = 0;
    std::function<void()> cb_;
    std::string name_;
    Semaphore sem_;
};

} // namespace server

#endif
