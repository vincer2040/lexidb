include(FetchContent)

set(LIBCHECK_VERSION "0.15.2")

# Use FetchContent to download and include libcheck
FetchContent_Declare(
    libcheck
    URL https://github.com/libcheck/check/archive/${LIBCHECK_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP true  # Specify the option here
)

FetchContent_MakeAvailable(libcheck)

# parser does not require all of these to work, but they are
# needed to compile
add_executable(
    parser_test
    parser_test.c
    ../src/parser.c
    ../src/cmd.c
    ../src/networking.c
    ../src/object.c
    ../src/vstr.c
    ../src/db.c
    ../src/server.c
    ../src/connection.c
    ../src/vmap_type.c
    ../src/configure.c
    ../src/auth.c
)

add_executable(
    vec_test
    vec_test.c
)

add_executable(
    vmap_test
    vmap_test.c
)

add_executable(
    config_test
    config_test.c
    ../src/cmd.c
    ../src/networking.c
    ../src/object.c
    ../src/vstr.c
    ../src/db.c
    ../src/server.c
    ../src/connection.c
    ../src/vmap_type.c
    ../src/configure.c
    ../src/auth.c
    ../src/configure.c
    ../src/auth.c
)

add_executable(
    builder_test
    builder_test.c
)

# parser does not require all of these to work, but they are
# needed to compile
target_link_libraries(
    parser_test
    check
    pthread
    builder
    vec
    util
    vmap
    ev
    vnet
    siphash
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

target_link_libraries(
    config_test
    check
    pthread
    vec
    vstr
    util
    builder
    vmap
    ev
    vnet
    siphash
    parser
)

target_link_libraries(
    builder_test
    check
    pthread
    builder
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

add_test(
    NAME config_test
    COMMAND config_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing
)

add_test(
    NAME builder_test
    COMMAND builder_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing
)

set_tests_properties(
    parser_test PROPERTIES TIMEOUT 30
    vec_test PROPERTIES TIMEOUT 30
    vmap_test PROPERTIES TIMEOUT 30
    config_test PROPERTIES TIMEOUT 30
    builder_test PROPERTIES TIMEOUT 30
)
