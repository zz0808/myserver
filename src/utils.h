#ifndef UTILS_H_
#define UTILS_H_

#include <sys/types.h>
#include <stdint.h>
#include <sys/time.h>
#include <cxxabi.h> // for abi::__cxa_demangle()
#include <string>
#include <vector>
#include <iostream>

namespace server {

// 获取程序到现在的启动时间(毫秒)
uint64_t GetElapsedMS();

} // namespace server

#endif