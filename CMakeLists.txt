cmake_minimum_required(VERSION 3.10)
project(TinyNTP)

# The library: header-only
add_library(TinyNTP INTERFACE)
add_library(TinyNTPClient ALIAS TinyNTP)  # the former name

target_include_directories(TinyNTP INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# The examples and the command line tool run on the desktop with the Arduino
# emulator, which is downloaded from GitHub once for the whole project
# (cmake/Emulator.cmake)
option(TINYNTP_BUILD_EXAMPLES "Build the examples for the desktop" ON)
option(TINYNTP_BUILD_TOOLS "Build the command line tool tinyntp" ON)
option(TINYNTP_BUILD_TESTS "Build the tests (run them with ctest)" OFF)

if(TINYNTP_BUILD_EXAMPLES OR TINYNTP_BUILD_TOOLS)
  include(cmake/Emulator.cmake)
endif()
if(TINYNTP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(TINYNTP_BUILD_TOOLS)
  add_subdirectory(tools/tinyntp)
endif()
if(TINYNTP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
