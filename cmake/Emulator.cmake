# Fetches the Arduino emulator (https://github.com/pschatzmann/arduino-emulator)
# once and makes it available to the whole project: the examples, the command
# line tool and the tests. Include it with
#   include(<root of this library>/cmake/Emulator.cmake)
# in each CMakeLists.txt that needs it: it does its work only the first time.
#
# It provides
#   - the target arduino_emulator (the Arduino API for the desktop)
#   - the target TinyNTP (this library, header-only)
#   - the functions arduino_library() and arduino_sketch() of the emulator
include_guard(GLOBAL)

if(NOT TARGET arduino_emulator)
  include(FetchContent)
  FetchContent_Declare(
      arduino_emulator
      GIT_REPOSITORY https://github.com/pschatzmann/arduino-emulator.git
      GIT_TAG        main
  )
  FetchContent_MakeAvailable(arduino_emulator)
endif()
include(${arduino_emulator_SOURCE_DIR}/Arduino.cmake)

if(NOT TARGET TinyNTP)
  arduino_library(TinyNTP ${CMAKE_CURRENT_LIST_DIR}/..)
endif()
