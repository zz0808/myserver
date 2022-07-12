#ifndef __MUTEX_H__
#define __MUTEX_H__

#include "nocopyable.h"
#include <semaphore.h>
#include <stdint.h>
#include <thread>
#include <atomic>

namespace server {

// 信号量
class Semaphore : Nocopyable {
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();
    void wait();
    void notify();

private:
    sem_t sem_;
};

// 原子锁
class CASLock : Nocopyable {
public:
    CASLock() {
        mtx_.clear();
    }
    ~CASLock() {}
    void lock() {
        while (std::atomic_flag_test_and_set_explicit(&mtx_, std::memory_order_acquire));
    }
    void unlock() {
        std::atomic_flag_clear_explicit(&mtx_, std::memory_order_release);
    }

private:
    volatile std::atomic_flag mtx_;
};


} // namespace server

#endif