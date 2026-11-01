cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ntp-wifi)
endif()

# Builds the sketch as a program for the desktop with the Arduino emulator,
# which is downloaded from GitHub:
#   cmake -S examples/ntp-wifi -B build-ntp-wifi
#   cmake --build build-ntp-wifi
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Emulator.cmake)

arduino_sketch(ntp-wifi ntp-wifi.ino LIBRARIES TinyNTP)
# The Arduino IDE adds Arduino.h to each .ino file
target_compile_options(ntp-wifi PRIVATE -include Arduino.h)
