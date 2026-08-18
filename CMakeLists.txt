# chalkwalk-music -- music theory, JUCE-free and dependency-free.
#
# Written to work BOTH nested inside a parent project and standalone, so that
# `cmake -B build && ctest` works from this directory with nothing else on the
# machine. That is not a convenience: a library that only builds inside its
# parent has not actually been extracted, and the standalone build is the test
# of the boundary. See ../../../ECOSYSTEM.md.

cmake_minimum_required(VERSION 3.22)
project(chalkwalk_music VERSION 0.1.0 LANGUAGES CXX)

# INTERFACE while everything here is header-only. This becomes a STATIC library
# the moment the first .cpp arrives; nothing about the consumer-facing usage
# (`chalkwalk::music`) changes when it does.
add_library(chalkwalk_music INTERFACE)
add_library(chalkwalk::music ALIAS chalkwalk_music)

target_include_directories(chalkwalk_music
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(chalkwalk_music INTERFACE cxx_std_17)

# Tests only when built on its own, or when a parent explicitly asks. A parent
# that pulls this in for the headers should not have to build Catch2.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CHALKWALK_MUSIC_TESTS_DEFAULT ON)
else()
    set(CHALKWALK_MUSIC_TESTS_DEFAULT OFF)
endif()
option(CHALKWALK_MUSIC_TESTS "Build chalkwalk-music tests"
       ${CHALKWALK_MUSIC_TESTS_DEFAULT})

if(CHALKWALK_MUSIC_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
