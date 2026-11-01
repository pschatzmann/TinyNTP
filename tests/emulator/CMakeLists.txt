cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(tinyntp-emulator-tests CXX)
endif()

# Tests of the library with the Arduino emulator, which provides the real
# Arduino API on the desktop. The emulator is downloaded from GitHub. Use
# -DTINYNTP_EMULATOR_TESTS=ON in tests/, or build this directory on its own:
#   cmake -S tests/emulator -B build-emulator && cmake --build build-emulator
#   ctest --test-dir build-emulator --output-on-failure
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

include(FetchContent)
FetchContent_Declare(
    arduino_emulator
    GIT_REPOSITORY https://github.com/pschatzmann/arduino-emulator.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(arduino_emulator)

add_executable(test_emulator test_emulator.cpp)
target_include_directories(test_emulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_emulator PRIVATE arduino_emulator)
add_test(NAME test_emulator COMMAND test_emulator)
