cmake_minimum_required(VERSION 3.2)

project(test)

set(CMAKE_CXX_FLAGS "-std=c++17 -ggdb")

# include_directories()

add_executable(test src/thread.cpp
                    src/fiber.cpp
                    src/hook.cpp
                    src/scheduler.cpp
                    src/timer.cpp
                    src/iomanager.cpp
                    src/utils.cpp
                    test/iomanager_test.cpp)

target_link_libraries(test pthread)