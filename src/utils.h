#include <sys/types.h>
#include <stdint.h>
#include <sys/time.h>
#include <cxxabi.h> // for abi::__cxa_demangle()
#include <string>
#include <vector>
#include <iostream>

namespace dserver {

// 获取当前启动的毫秒数，参考clock_gettime(2)，使用CLOCK_MONOTONIC_RAW
uint64_t GetElapsedMS();

} // namespace dserver 