cmake_minimum_required(VERSION 3.14)
project(ogon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library
add_library(ogon INTERFACE)
target_include_directories(ogon INTERFACE include)

# Examples
option(OGON_BUILD_EXAMPLES "Build examples" ON)
if(OGON_BUILD_EXAMPLES)
    add_executable(simple examples/simple.cpp)
    target_link_libraries(simple PRIVATE ogon)
endif()

# Tests
option(OGON_BUILD_TESTS "Build tests" ON)
if(OGON_BUILD_TESTS)
    enable_testing()
    add_executable(test_basic tests/test_basic.cpp)
    target_link_libraries(test_basic PRIVATE ogon)
    add_test(NAME test_basic COMMAND test_basic)
    add_executable(test_full_spec tests/test_full_spec.cpp)
    target_link_libraries(test_full_spec PRIVATE ogon)
    add_test(NAME test_full_spec COMMAND test_full_spec)
endif()