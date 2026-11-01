cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ntp-wifi-timezone)
endif()

# Builds the sketch as a program for the desktop with the Arduino emulator,
# which is downloaded from GitHub:
#   cmake -S examples/ntp-wifi-timezone -B build-ntp-wifi-timezone
#   cmake --build build-ntp-wifi-timezone
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Emulator.cmake)

arduino_sketch(ntp-wifi-timezone ntp-wifi-timezone.ino LIBRARIES TinyNTP)
# The Arduino IDE adds Arduino.h to each .ino file
target_compile_options(ntp-wifi-timezone PRIVATE -include Arduino.h)
