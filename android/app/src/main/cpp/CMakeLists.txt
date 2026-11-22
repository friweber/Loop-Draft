cmake_minimum_required(VERSION 3.22.1)
project(loopdraft_android CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The audio code has to be optimized in every build type, or it cannot keep up in real time.
add_compile_options(-O2)

# The portable engine from the repository root.
set(LD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../engine ${CMAKE_BINARY_DIR}/loopdraft_engine)

# Oboe: Google's low-latency audio library, built from source and linked statically.
include(FetchContent)
FetchContent_Declare(oboe
  GIT_REPOSITORY https://github.com/google/oboe.git
  GIT_TAG 1.9.3
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(oboe)

add_library(loopdraft_jni SHARED native-lib.cpp)
target_link_libraries(loopdraft_jni PRIVATE loopdraft oboe log)
# Needed on phones that use 16 KB memory pages.
target_link_options(loopdraft_jni PRIVATE "-Wl,-z,max-page-size=16384")
