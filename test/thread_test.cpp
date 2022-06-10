#include "../src/thread.h"
#include <iostream>
#include <vector>
#include <atomic>

using namespace dserver;

std::atomic<uint32_t> g_count{0};

void func() {
    std::cout << dserver::Thread::GetThreadId() << " "
        << dserver::Thread::GetName() << std::endl;

    for (size_t i = 0; i < 1000000; i++) {
        g_count++;
    }
}

int main(int argc, char const *argv[])
{
    std::vector<dserver::Thread*> thrds;
    for (size_t i = 0; i < 5; i++) {
        thrds.push_back(new Thread(&func, "thread " + std::to_string(i)));
        thrds.back()->join();
    }

    std::cout << "g_count : " << g_count << std::endl;

    return 0;
}
