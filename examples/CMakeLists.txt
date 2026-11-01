cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(tinyntp-examples)
endif()

# Builds the examples as programs for the desktop with the Arduino emulator
# (downloaded from GitHub). The RTC example is not included: it needs the
# hardware of a board.
#   cmake -S examples -B build-examples && cmake --build build-examples
# Each example can also be built on its own: see the CMakeLists.txt in its
# directory.
add_subdirectory(ntp-wifi)
add_subdirectory(ntp-wifi-timezone)
add_subdirectory(ntp-wifi-server)
