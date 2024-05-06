include(../cmake/GoogleTest.cmake)

add_executable(
    vmap_test
    vmap_test.cc
    debug.cc
)

target_link_libraries(
    vmap_test
    GTest::gtest_main
    GTest::gmock_main
)

include(GoogleTest)

gtest_discover_tests(vmap_test)
