cmake_minimum_required(VERSION 3.16)
project(niyata VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NIYATA_BUILD_TESTS "Build unit tests" ON)
option(NIYATA_BUILD_EXAMPLES "Build examples" ON)

# Core library. Add src/*.cpp here as the scheduler grows.
add_library(niyata
    src/scheduler.cpp
    port/host/context_ucontext.cpp
)
target_include_directories(niyata PUBLIC include)
target_compile_options(niyata PRIVATE -Wall -Wextra -Wpedantic)

if(NIYATA_BUILD_EXAMPLES)
    add_executable(hello_tasks examples/hello_tasks.cpp)
    target_link_libraries(hello_tasks PRIVATE niyata)
endif()

if(NIYATA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
