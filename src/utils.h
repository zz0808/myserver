#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/types.h>
#include <stdint.h>
#include <sys/time.h>
#include <cxxabi.h> 
#include <string>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <signal.h> 
#include <sys/syscall.h>
#include <sys/stat.h>
#include <execinfo.h>
#include <cxxabi.h>  
#include <algorithm> 

namespace server {

uint64_t GetElapsedMS();
pid_t GetThreadId();
uint64_t GetFiberId();
std::string GetThreadName();
std::string GetThreadName();
void SetThreadName(const std::string &name);

} // namespace server

#endif