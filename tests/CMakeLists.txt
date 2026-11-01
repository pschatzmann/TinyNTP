cmake_minimum_required(VERSION 3.10)
project(tinyntp-tests CXX)

# Tests of the library on the desktop (Linux, macOS):
#   cmake -S tests -B build-tests && cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

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
tinyntp_test(test_loopback)

# The namespace: used automatically with ARDUINO, otherwise not
tinyntp_test(test_namespace_arduino)
target_compile_definitions(test_namespace_arduino PRIVATE ARDUINO=100)
tinyntp_test(test_namespace_plain)

# ...and this one must not compile without ARDUINO
add_executable(test_namespace_leak EXCLUDE_FROM_ALL test_namespace_leak.cpp)
target_include_directories(test_namespace_leak PRIVATE ${LIB_SRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME test_namespace_needs_arduino_or_using
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test_namespace_leak)
set_tests_properties(test_namespace_needs_arduino_or_using PROPERTIES WILL_FAIL TRUE)

# The command line tool with a device (a TinyNTPServer) that it talks to
add_executable(tinyntp ${CMAKE_CURRENT_SOURCE_DIR}/../tools/tinyntp/tinyntp.cpp)
target_include_directories(tinyntp PRIVATE ${LIB_SRC})
target_compile_options(tinyntp PRIVATE -Wall -Wextra)
add_executable(test_device test_device.cpp)
target_include_directories(test_device PRIVATE ${LIB_SRC})
target_compile_options(test_device PRIVATE -Wall -Wextra)
if(Python3_FOUND)
  add_test(NAME test_tool
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_tool.py
                   $<TARGET_FILE:tinyntp> $<TARGET_FILE:test_device>)
  set_tests_properties(test_tool PROPERTIES TIMEOUT 120)
else()
  message(STATUS "python3 not found: test_tool is not run")
endif()

# Tests with the real Arduino API of the Arduino emulator (downloaded from
# GitHub): cmake -S tests -B build-tests -DTINYNTP_EMULATOR_TESTS=ON
option(TINYNTP_EMULATOR_TESTS "Test with the Arduino emulator (needs a download)" OFF)
if(TINYNTP_EMULATOR_TESTS)
  add_subdirectory(emulator)
endif()
