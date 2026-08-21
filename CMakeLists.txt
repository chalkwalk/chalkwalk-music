# chalkwalk-music -- music theory, JUCE-free and dependency-free.
#
# Written to work BOTH nested inside a parent project and standalone, so that
# `cmake -B build && ctest` works from this directory with nothing else on the
# machine. That is not a convenience: a library that only builds inside its
# parent has not actually been extracted, and the standalone build is the test
# of the boundary.

cmake_minimum_required(VERSION 3.22)
project(chalkwalk_music VERSION 0.1.0 LANGUAGES CXX)

# STATIC since Notation and Harmony arrived. The header-only note this replaces
# said "nothing about the consumer-facing usage changes when it does", and
# nothing did: `chalkwalk::music` is still what a consumer links.
#
# Everything else here is still header-only and stays that way. These two are
# compiled because they are large and because parsing a chord name is not
# something every translation unit should pay for.
add_library(chalkwalk_music STATIC
    src/Notation.cpp
    src/Harmony.cpp)
add_library(chalkwalk::music ALIAS chalkwalk_music)

target_include_directories(chalkwalk_music
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

target_compile_features(chalkwalk_music PUBLIC cxx_std_17)
set_target_properties(chalkwalk_music PROPERTIES POSITION_INDEPENDENT_CODE ON)

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
