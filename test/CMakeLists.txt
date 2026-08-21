# Catch2, guarded so a parent that already provides it is reused rather than
# fetching a second copy.
if(NOT TARGET Catch2::Catch2WithMain)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1)
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(chalkwalk_music_tests
    EuclideanTests.cpp
    HarmonyTests.cpp
    NotationTests.cpp
    ScaleTests.cpp
    MetricGridTests.cpp
    AccentTests.cpp
    DensityTests.cpp
    MetricSelectTests.cpp
    TuningTests.cpp
    NoteStrengthTests.cpp
    MelodyTests.cpp
    DurationTests.cpp)

target_link_libraries(chalkwalk_music_tests
    PRIVATE chalkwalk::music Catch2::Catch2WithMain)

# MSVC does not speak GCC's warning flags, and passing them is a hard error
# rather than a warning -- so an unguarded list breaks the Windows build at the
# command line, before a single file is compiled.
if(MSVC)
    target_compile_options(chalkwalk_music_tests PRIVATE /W4)
else()
    target_compile_options(chalkwalk_music_tests
        PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

add_test(NAME chalkwalk_music COMMAND chalkwalk_music_tests)
