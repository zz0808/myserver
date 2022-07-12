#include "mutex.h"

namespace server {

Semaphore::Semaphore(uint32_t count) {
    if (sem_init(&sem_, 0, count)) {
        throw std::logic_error("sem_init");
    }
}

Semaphore::~Semaphore() {
    sem_destroy(&sem_);
}

void Semaphore::wait() {
    if (sem_wait(&sem_)) {
        throw std::logic_error("sem wait");
    }
}

void Semaphore::notify() {
    if (sem_post(&sem_)) {
        throw std::logic_error("sem notify");
    }
}

} // namespace server