#ifndef MUTEX_H_
#define MUTEX_H_

#include "nocopyable.h"
#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include <list>

namespace server {

class Semaphore : public Nocopyable {
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();
    void wait();
    void notify();

private:
    sem_t sem_;
};

} // namespace server

#endif