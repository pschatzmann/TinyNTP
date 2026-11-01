cmake_minimum_required(VERSION 3.5)
project(tinyntp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Command line NTP client and server (Linux, macOS) based on the TinyNTP library
add_executable(tinyntp tinyntp.cpp)
target_include_directories(tinyntp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_compile_options(tinyntp PRIVATE -Wall)

install(TARGETS tinyntp RUNTIME DESTINATION bin)
