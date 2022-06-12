#include "timer.h"
#include "utils.h"

namespace dserver {

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    std::shared_ptr<void> tmp = weak_cond.lock();
    if(tmp) {
        cb();
    }
}

bool Timer::Comparator::operator()(const Timer::ptr lhs, const Timer::ptr rhs) const {
    if (lhs == nullptr && rhs == nullptr) {
        return true;
    }
    if (lhs == nullptr) {
        return true;
    }
    if (rhs == nullptr) {
        return false;
    }
    if (lhs->next_ < rhs->next_) {
        return true;
    }
    if (lhs->next_ > rhs->next_) {
        return false;
    }
    return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recur, TimerManager* manager)
            : recurring_(recur)
            , ms_(ms)
            , cb_(cb)
            , manager_(manager) {
    next_ = ms_ + dserver::GetElapsedMS();
}

Timer::Timer(uint64_t next) : next_(next) {}

bool Timer::cancel() {
    std::unique_lock<std::shared_mutex> write_lock(manager_->mtx_);
    if (cb_) {
        cb_ = nullptr;
        auto it = manager_->timers_.find(shared_from_this());
        manager_->timers_.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    std::unique_lock<std::shared_mutex> write_lock(manager_->mtx_);
    if (cb_ == nullptr) {
        return false;
    }
    auto it = manager_->timers_.find(shared_from_this());
    if (it == manager_->timers_.end()) {
        return false;
    }
    manager_->timers_.erase(it);
    next_ = ms_ + dserver::GetElapsedMS();
    manager_->timers_.insert(shared_from_this());
    return true;
}

bool Timer::reset(uint64_t ms, bool from_now) {
    // 不用修改
    if (ms == ms_ && from_now == false) {
        return true;
    }
    std::unique_lock<std::shared_mutex> write_lock(manager_->mtx_);
    if (cb_ == nullptr) {
        return false;
    }
    auto it = manager_->timers_.find(shared_from_this());
    if (it == manager_->timers_.end()) {
        return false;
    }
    manager_->timers_.erase(it);
    uint64_t last_exe_time = next_ - ms_;
    ms_ = ms;
    next_ = (from_now ? dserver::GetElapsedMS() : last_exe_time) + ms_;
    manager_->timers_.insert(shared_from_this());

    return true;
}

TimerManager::TimerManager() {
    previous_time = dserver::GetElapsedMS();
}

TimerManager::~TimerManager() { }

Timer::ptr TimerManager::add_timer(uint64_t ms, std::function<void()> cb, bool recur) {
    Timer::ptr timer(new Timer(ms, cb, recur, this));
    std::unique_lock<std::shared_mutex> write_lock(mtx_);
    add_timer(timer, write_lock);
    return timer;
}

Timer::ptr TimerManager::add_condition_timer(uint64_t ms, std::function<void()> cb, 
                                std::weak_ptr<void> conditon, bool recur) {
    return add_timer(ms, std::bind(&OnTimer, conditon, cb), recur);
}

uint64_t TimerManager::get_next_timer() {
    std::shared_lock<std::shared_mutex> read_lock(mtx_);
    tickle_ = false;
    if (timers_.empty()) {
        return 0;
    }
    const Timer::ptr& timer = *timers_.begin();
    uint64_t now_ms = dserver::GetElapsedMS();

    if(now_ms >= timer->next_) {
        return 0;
    } else {
        return timer->next_ - now_ms;
    }
}

void TimerManager::list_all_expired_cbs(std::vector<std::function<void()>>& cbs) {
    uint64_t now_ms = dserver::GetElapsedMS();
    std::vector<Timer::ptr> expired;
    {
        std::shared_lock<std::shared_mutex> read_lock(mtx_);
        if(timers_.empty()) {
            return;
        }
    }
    std::unique_lock<std::shared_mutex> write_lock(mtx_);
    if(timers_.empty()) {
        return;
    }
    bool rollover = false;
    if(detect_clock_rollover(now_ms)) {
        // 使用clock_gettime(CLOCK_MONOTONIC_RAW)，应该不可能出现时间回退的问题
        rollover = true;
    }
    if(!rollover && ((*timers_.begin())->next_ > now_ms)) {
        return;
    }

    Timer::ptr now_timer(new Timer(now_ms));
    auto it = rollover ? timers_.end() : timers_.lower_bound(now_timer);
    while(it != timers_.end() && (*it)->next_ == now_ms) {
        ++it;
    }
    expired.insert(expired.begin(), timers_.begin(), it);
    timers_.erase(timers_.begin(), it);
    cbs.reserve(expired.size());

    for(auto& timer : expired) {
        cbs.push_back(timer->cb_);
        if(timer->recurring_) {
            timer->next_ = now_ms + timer->ms_;
            timers_.insert(timer);
        } else {
            timer->cb_ = nullptr;
        }
    }
}

bool TimerManager::has_timer() {
    std::shared_lock<std::shared_mutex> read_lock(mtx_);
    return !timers_.empty();
}

void TimerManager::add_timer(Timer::ptr val, std::unique_lock<std::shared_mutex>& lock) {
    auto it = timers_.insert(val).first;
    bool at_front = (it == timers_.begin()) && !tickle_;
    if(at_front) {
        tickle_ = true;
    }
    lock.unlock();

    if(at_front) {
        on_timer_inserted_at_front();
    }
}

bool TimerManager::detect_clock_rollover(uint64_t now_ms) {
    bool rollover = false;
    if(now_ms < previous_time &&
            now_ms < (previous_time - 60 * 60 * 1000)) {
        rollover = true;
    }
    previous_time = now_ms;
    return rollover;
}

} // namespace derver 