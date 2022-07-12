#include "../src/thread.h"
#include <vector>

using namespace server;

Logger::ptr g_logger = LOG_ROOT();

int count = 0;
std::mutex mtx;

void func1(void *arg) {
    LOG_INFO(g_logger) << "name:" << server::Thread::GetName()
        << " this.name:" << server::Thread::GetThis()->getThreadName()
        << " thread name:" << server::GetThreadName()
        << " id:" << server::GetThreadId()
        << " this.id:" << server::Thread::GetThis()->getThreadId();
    LOG_INFO(g_logger) << "arg: " << *(int*)arg;
    for(int i = 0; i < 10000; i++) {
        std::lock_guard<std::mutex> locker(mtx);
        ++count;
    }
}

int main(int argc, char const *argv[]) {

    std::vector<server::Thread::ptr> thrs;
    int arg = 123456;
    for(int i = 0; i < 3; i++) {
        // 带参数的线程用std::bind进行参数绑定
        server::Thread::ptr thr(new server::Thread(std::bind(func1, &arg), "thread_" + std::to_string(i)));
        thrs.push_back(thr);
    }

    for(int i = 0; i < 3; i++) {
        thrs[i]->join();
    }
    
    LOG_INFO(g_logger) << "count = " << count;
    return 0;
}
