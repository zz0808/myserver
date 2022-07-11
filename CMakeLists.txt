cmake_minimum_required(VERSION 3.2)

project(test)

set(CMAKE_CXX_FLAGS "-std=c++17 -ggdb")

# include_directories(src)

add_executable(test src/log.cpp
                    src/utils.cpp
                    src/singleton.h
                    test/test_log.cpp)

target_link_libraries(test pthread)