cmake_minimum_required(VERSION 3.20)

project(
    signalpp
    VERSION 0.1.0
    DESCRIPTION "A lightweight C++ signal/slot library"
    LANGUAGES CXX
)

option(SIGNALPP_BUILD_TESTS "Build signalpp tests" "${PROJECT_IS_TOP_LEVEL}")

add_library(signalpp INTERFACE)
add_library(signalpp::signalpp ALIAS signalpp)

target_compile_features(signalpp INTERFACE cxx_std_20)

target_include_directories(
    signalpp
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

if(SIGNALPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()