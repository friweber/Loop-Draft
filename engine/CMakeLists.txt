cmake_minimum_required(VERSION 3.16)
project(loopdraft_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(LD_SANITIZE "Build with address and undefined-behaviour sanitizers" OFF)
option(LD_BUILD_TESTS "Build desktop tests and tools" ON)

add_library(loopdraft STATIC
  src/project.cpp src/voices.cpp src/engine.cpp src/export.cpp src/wav.cpp src/session.cpp)
target_include_directories(loopdraft PUBLIC include PRIVATE third_party)
target_compile_options(loopdraft PRIVATE -Wall -Wextra)
set_source_files_properties(src/wav.cpp PROPERTIES COMPILE_OPTIONS "-w")   # third-party header

option(LD_TSAN "Build with the thread sanitizer" OFF)
if(LD_TSAN)
  add_compile_options(-fsanitize=thread -g -O1)
  add_link_options(-fsanitize=thread)
endif()

if(LD_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

if(LD_BUILD_TESTS)
  find_package(Threads REQUIRED)
  add_executable(ld_tests tests/test_main.cpp)
  target_link_libraries(ld_tests PRIVATE loopdraft Threads::Threads)
  target_compile_options(ld_tests PRIVATE -Wall -Wextra)
  target_compile_definitions(ld_tests PRIVATE LD_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
  add_executable(ld_render tools/ld_render.cpp)
  target_link_libraries(ld_render PRIVATE loopdraft)
  enable_testing()
  add_test(NAME engine COMMAND ld_tests)
endif()
