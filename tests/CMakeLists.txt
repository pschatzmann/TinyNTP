cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(tinyntp-tests CXX)
  enable_testing()
endif()

# Tests of the library on the desktop (Linux, macOS):
#   cmake -S tests -B build-tests && cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure
# or with -DTINYNTP_BUILD_TESTS=ON in the main CMakeLists.txt.
#
# The tests of the first group need nothing but a compiler: they use a
# simulated environment (mock.h). The second group uses the Arduino emulator,
# which is downloaded from GitHub; switch it off with
# -DTINYNTP_EMULATOR_TESTS=OFF.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter)

# A test with a simulated environment (millis(), UDP): fast and deterministic
function(tinyntp_test name)
  add_executable(${name} ${name}.cpp)
  target_include_directories(${name} PRIVATE ${LIB_SRC} ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_options(${name} PRIVATE -Wall -Wextra)
  target_compile_definitions(${name} PRIVATE TINYNTP_SRC_DIR="${LIB_SRC}")
  target_link_libraries(${name} PRIVATE Threads::Threads)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

tinyntp_test(test_sha256)
tinyntp_test(test_timesetmessage)
tinyntp_test(test_timezone)
tinyntp_test(test_defines)
tinyntp_test(test_timesources)
tinyntp_test(test_client)
tinyntp_test(test_server)
tinyntp_test(test_timesetting)
tinyntp_test(test_gps)
tinyntp_test(test_zonetable)

# The namespace: TinyNTP.h uses it, unless NO_TINY_NTP_NAMESPACE is defined
tinyntp_test(test_namespace_default)
tinyntp_test(test_namespace_opt_out)

# ...and without the namespace the plain names must not compile
add_executable(test_namespace_leak EXCLUDE_FROM_ALL test_namespace_leak.cpp)
target_include_directories(test_namespace_leak PRIVATE ${LIB_SRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME test_namespace_opt_out_needs_qualified_names
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test_namespace_leak)
set_tests_properties(test_namespace_opt_out_needs_qualified_names PROPERTIES WILL_FAIL TRUE)

# --- Tests with the Arduino emulator: the real Arduino API on the desktop -----
option(TINYNTP_EMULATOR_TESTS "Tests with the Arduino emulator (downloaded from GitHub)" ON)
if(TINYNTP_EMULATOR_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Emulator.cmake)

  # Client and server over real sockets with the Arduino classes
  add_executable(test_emulator test_emulator.cpp)
  target_include_directories(test_emulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(test_emulator PRIVATE TinyNTP arduino_emulator Threads::Threads)
  target_compile_options(test_emulator PRIVATE -Wno-sign-compare -Wno-overloaded-virtual)
  add_test(NAME test_emulator COMMAND test_emulator)

  # The command line tool, and a device (a TinyNTPServer) that it talks to
  if(NOT TARGET tinyntp)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tools/tinyntp tinyntp)
  endif()
  add_executable(test_device device_main.cpp test_device.cpp)
  target_link_libraries(test_device PRIVATE TinyNTP arduino_emulator)
  target_compile_options(test_device PRIVATE -Wno-sign-compare -Wno-overloaded-virtual)
  if(Python3_FOUND)
    add_test(NAME test_tool
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_tool.py
                     $<TARGET_FILE:tinyntp> $<TARGET_FILE:test_device>)
    set_tests_properties(test_tool PROPERTIES TIMEOUT 120)
  else()
    message(STATUS "python3 not found: test_tool is not run")
  endif()

  # The examples must compile (they are built for the desktop)
  if(NOT TARGET ntp-wifi)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../examples examples)
  endif()
endif()
