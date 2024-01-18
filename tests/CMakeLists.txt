include(FetchContent)

set(LIBCHECK_VERSION "0.15.2")

# Use FetchContent to download and include libcheck
FetchContent_Declare(
    libcheck
    URL https://github.com/libcheck/check/archive/${LIBCHECK_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP true  # Specify the option here
)

FetchContent_MakeAvailable(libcheck)

# ht test
add_executable(ht_test ht_test.c)

target_link_libraries(ht_test PUBLIC check ht pthread)

target_include_directories(ht_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME ht_test COMMAND ht_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(ht_test PROPERTIES TIMEOUT 30)

# parser test
add_executable(parser_test parser_test.c)

target_link_libraries(parser_test PUBLIC check parser builder pthread)

target_include_directories(parser_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME parser_test COMMAND parser_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(parser_test PROPERTIES TIMEOUT 30)

# json test
add_executable(vjson_test vjson_test.c)

target_link_libraries(vjson_test PUBLIC check pthread)

target_include_directories(vjson_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME vjson_test COMMAND vjson_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(vjson_test PROPERTIES TIMEOUT 30)
