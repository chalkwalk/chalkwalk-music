# Catch2, guarded so a parent that already provides it is reused rather than
# fetching a second copy.
if(NOT TARGET Catch2::Catch2WithMain)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1)
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(chalkwalk_music_tests EuclideanTests.cpp)

target_link_libraries(chalkwalk_music_tests
    PRIVATE chalkwalk::music Catch2::Catch2WithMain)

target_compile_options(chalkwalk_music_tests
    PRIVATE -Wall -Wextra -Wpedantic -Wshadow)

add_test(NAME chalkwalk_music COMMAND chalkwalk_music_tests)
