cmake_minimum_required(VERSION 3.14)
project(pktpy-hi C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for our code only
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(PROJECT_WARNING_FLAGS -Wall -Wextra -pedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O2)
    endif()
endif()

# Include directories
# pocketpy-2.1.6 is marked as SYSTEM to suppress warnings from its headers
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/pocketpy-2.1.6)

# pocketpy as object library (third-party, no warnings)
add_library(pocketpy OBJECT ${CMAKE_SOURCE_DIR}/pocketpy-2.1.6/pocketpy.c)
target_compile_options(pocketpy PRIVATE -w)

# Examples (C)
add_executable(basic_usage examples/basic_usage.c $<TARGET_OBJECTS:pocketpy>)
target_compile_options(basic_usage PRIVATE ${PROJECT_WARNING_FLAGS})
if(NOT MSVC)
    target_link_libraries(basic_usage PRIVATE m)
endif()

# Examples (C++)
add_executable(basic_usage_cpp examples/basic_usage.cpp $<TARGET_OBJECTS:pocketpy>)
target_compile_options(basic_usage_cpp PRIVATE ${PROJECT_WARNING_FLAGS})
if(NOT MSVC)
    target_link_libraries(basic_usage_cpp PRIVATE m)
endif()

# Enable testing
enable_testing()

# Test directory
set(TEST_DIR ${CMAKE_SOURCE_DIR}/tests)

# Helper function to add a test
function(add_ph_test name)
    add_executable(${name} ${TEST_DIR}/${name}.c $<TARGET_OBJECTS:pocketpy>)
    target_compile_options(${name} PRIVATE ${PROJECT_WARNING_FLAGS})
    if(NOT MSVC)
        target_link_libraries(${name} PRIVATE m)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Helper function to add a C++ test
function(add_ph_test_cpp name)
    add_executable(${name} ${TEST_DIR}/${name}.cpp $<TARGET_OBJECTS:pocketpy>)
    target_compile_options(${name} PRIVATE ${PROJECT_WARNING_FLAGS})
    if(NOT MSVC)
        target_link_libraries(${name} PRIVATE m)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Test executables (C)
add_ph_test(test_scope)
add_ph_test(test_exec)
add_ph_test(test_values)
add_ph_test(test_calls)
add_ph_test(test_binding)
add_ph_test(test_extraction)
add_ph_test(test_lists)
add_ph_test(test_interop)
add_ph_test(test_registers)
add_ph_test(test_debug)

# Test executables (C++)
add_ph_test_cpp(test_cpp_wrapper)

# Custom target to run tests with verbose output
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS
        test_scope
        test_exec
        test_values
        test_calls
        test_binding
        test_extraction
        test_lists
        test_interop
        test_registers
        test_debug
        test_cpp_wrapper
)
