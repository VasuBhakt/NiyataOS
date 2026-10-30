include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(niyata_tests test_context.cpp test_cooperative.cpp test_clock_sleep.cpp test_preemption.cpp)
target_link_libraries(niyata_tests PRIVATE niyata GTest::gtest_main)
gtest_discover_tests(niyata_tests)
