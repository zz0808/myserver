#ifndef TIMER_H_
#define TIMER_H_

#include <memory>
#include <vector>
#include <set>
#include <functional>
#include <mutex>
#include <shared_mutex>

namespace dserver {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
friend TimerManager;
public:
    typedef std::shared_ptr<Timer> ptr;

    bool cancel();
    bool refresh();
    bool reset(uint64_t ms, bool from_now);
    ~Timer() {}

private:
    Timer(uint64_t ms, std::function<void()> cb, bool recur, TimerManager* manager);
    Timer(uint64_t next);

private:
    bool recurring_ = false;
    uint64_t ms_ = 0;
    uint64_t next_ = 0;
    std::function<void()> cb_;
    TimerManager* manager_;

    struct Comparator {
        bool operator()(const Timer::ptr lhs, const Timer::ptr rhs) const;
    };
};

class TimerManager {
friend Timer;
public:
    TimerManager();
    virtual ~TimerManager();
    Timer::ptr add_timer(uint64_t ms, std::function<void()> cb, bool recur = false);
    Timer::ptr add_condition_timer(uint64_t ms, std::function<void()> cb, 
                                   std::weak_ptr<void> conditon, bool recur = false);
    uint64_t get_next_timer();
    void list_all_expired_cbs(std::vector<std::function<void()>>& cbs);
    bool has_timer();

protected:  
    virtual void on_timer_inserted_at_front() = 0;
    void add_timer(Timer::ptr val, std::unique_lock<std::mutex>& lock);
    bool detect_clock_rollover(uint64_t now_ms);
private:
    std::mutex mtx_;
    std::set<Timer::ptr, Timer::Comparator> timers_;
    bool tickle_ = false;
    uint64_t previous_time = 0;
};

} // namespace dserver

#endif