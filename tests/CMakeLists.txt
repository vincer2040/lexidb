include(FetchContent)

set(LIBCHECK_VERSION "0.15.2")

# Use FetchContent to download and include libcheck
FetchContent_Declare(
    libcheck
    URL https://github.com/libcheck/check/archive/${LIBCHECK_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP true  # Specify the option here
)

FetchContent_MakeAvailable(libcheck)

add_executable(
    parser_test
    parser_test.c
)

add_executable(
    vec_test
    vec_test.c
)

add_executable(
    vmap_test
    vmap_test.c
)

target_link_libraries(
    parser_test
    check
    pthread
    parser
    cmd
)

target_link_libraries(
    vec_test
    check
    pthread
    vec
)

target_link_libraries(
    vmap_test
    check
    pthread
    vmap
    vstr
    object
    util
    siphash
)

add_test(
    NAME parser_test
    COMMAND parser_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing
)

add_test(
    NAME vec_test
    COMMAND vec_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing
)

add_test(
    NAME vmap_test
    COMMAND vmap_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing
)

set_tests_properties(
    parser_test PROPERTIES TIMEOUT 30
    vec_test PROPERTIES TIMEOUT 30
    vmap_test PROPERTIES TIMEOUT 30
)
