cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ntp-wifi-server)
endif()

# Builds the sketch as a program for the desktop with the Arduino emulator,
# which is downloaded from GitHub:
#   cmake -S examples/ntp-wifi-server -B build-ntp-wifi-server
#   cmake --build build-ntp-wifi-server
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Emulator.cmake)

arduino_sketch(ntp-wifi-server ntp-wifi-server.ino LIBRARIES TinyNTP)
# The Arduino IDE adds Arduino.h to each .ino file
target_compile_options(ntp-wifi-server PRIVATE -include Arduino.h)
