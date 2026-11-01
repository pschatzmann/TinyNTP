cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(tinyntp CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Command line NTP client and server for the desktop (Linux, macOS) based on the
# TinyNTP library. The Arduino emulator, which is downloaded from GitHub,
# provides the Arduino API (UDP, millis()).
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Emulator.cmake)

add_executable(tinyntp main.cpp tinyntp.cpp zones.cpp)
target_link_libraries(tinyntp PRIVATE TinyNTP arduino_emulator)
# (the warnings about the headers of the emulator are switched off)
target_compile_options(tinyntp PRIVATE -Wall -Wno-sign-compare -Wno-overloaded-virtual)

install(TARGETS tinyntp RUNTIME DESTINATION bin)
