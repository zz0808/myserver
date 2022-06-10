#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "thread.h"
#include "fiber.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <list>
#include <string>
#include <memory>

namespace dserver {

class Scheduler {
public:
    typedef std::shared_ptr<Scheduler> ptr;

    Scheduler(uint32_t threads = 1, bool use_caller = false, const std::string& name = "scheduler");
    virtual ~Scheduler();
    const std::string GetSchedulerName() const { return name_; }
    void start();
    void stop();

    static Scheduler* GetThis();
    // 获取当前线程的主协程
    static Fiber* GetMainFiber();

    template<typename T>
    void schedule(T t) {
        bool need_tickle = false;
        {
            std::lock_guard<std::mutex> locker(mtx_);
            need_tickle = schedule_no_lock(t);
        }
        if (need_tickle) {
            tickle();
        }
    }

    template<typename Iterator>
    void schedule(Iterator begin, Iterator end) {
        bool need_tickle = false;
        {
            std::lock_guard<std::mutex> locker(mtx_);
            while (begin != end) {
                need_tickle |= schedule_no_lock(*begin);
                begin++;
            }
        }
        if (need_tickle) {
            tickle();
        }
    }


protected:
    virtual void tickle();
    void run();
    virtual void idle();
    virtual bool is_stopping();
    // 设置当前的协程调度器
    void SetThis();
    bool has_idle_threads() { return idle_threads_ > 0; }

private:
    struct Task {
        Fiber::ptr f;
        std::function<void()> cb;

        Task(Fiber::ptr fiber) {
            f = fiber;
        }
        Task(std::function<void()> func) {
            cb = func;
        }
        Task() { }
        void reset() {
            f = nullptr;
            cb = nullptr;
        }
    };

    template<typename T>
    bool schedule_no_lock(T t) {
        bool need_tickle = tasks_.empty();
        Task task(t);
        if (task.f || task.cb) {
            tasks_.push_back(task);
        }
        return need_tickle;
    }

private:
    std::string name_;
    std::mutex mtx_;
    std::vector<Thread::ptr> threads_;
    std::list<Task> tasks_;
    uint32_t threads_num_ = 0;
    std::atomic<uint32_t> active_threads_ = {0};
    std::atomic<uint32_t> idle_threads_ = {0};
    uint32_t root_thread_id_ = 0;
    // 是否use caller
    bool use_caller_ = false;
    // use caller时，调度器所在线程的主协程
    Fiber::ptr root_fiber_;
    // 是否正在停止
    bool stopping_ = false;
};

} // namespace dserver 


#endif 