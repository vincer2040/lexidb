cmake_minimum_required(VERSION 3.10)

project(lexidb VERSION 0.0.1)

include(CheckIncludeFile)

check_include_file("sys/epoll.h" HAVE_EPOLL)

configure_file(config.h.in "../src/config.h")

enable_testing()

add_subdirectory(tests)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_commands_help
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set(CMAKE_C_COMPILER "gcc")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -std=c99 -Wall -Werror -pedantic -fstack-clash-protection \
-fstack-protector-all -fstack-protector-strong -Werror=format-security -Werror=implicit-function-declaration -pipe -O2")

set(CMAKE_C_FLAGS_RELEASE "-std=c99 -Wall -Werror -pedantic -fstack-clash-protection -fstack-protector-all \
-fstack-protector-strong -Werror=format-security -Werror=implicit-function-declaration -pipe -O2 -s -DNDEBUG")

add_library(
    util
    src/util.c
    src/sha256.c
)

add_library(
    ht
    src/ht.c
    src/siphash.c
)

add_library(
    set
    src/set.c
    src/siphash.c
)

add_library(
    vstr
    src/vstr.c
)

add_library(
    vec
    src/vec.c
)

add_library(
    queue
    src/queue.c
)

add_library(
    object
    src/object.c
)

add_library(
    parser
    src/parser.c
)

add_library(
    networking
    src/networking.c
)

add_library(
    ev
    src/ev.c
)

add_library(
    builder
    src/builder.c
)

add_library(
    hilexi
    src/hilexi.c
)

add_library(
    clap
    src/clap.c
)

add_library(
    config_parser
    src/config_parser.c
)

add_executable(
    lexidb
    src/db.c
    src/server.c
    src/auth.c
    src/reply.c
)

add_executable(
    lexi-cli
    src/lexi-cli.c
    src/read_line.c
    src/line_parser.c
)

target_link_libraries(
    util
    vstr
)

target_link_libraries(
    ht
    util
)

target_link_libraries(
    set
    util
)

target_link_libraries(
    object
    vstr
    vec
    ht
)

target_link_libraries(
    parser
    object
)

target_link_libraries(
    hilexi
    parser
    object
    networking
    builder
)

target_link_libraries(
    clap
    vstr
    vec
    ht
)

target_link_libraries(
    config_parser
    ht
    vec
    vstr
    object
)

target_link_libraries(
    lexidb
    networking
    ht
    set
    queue
    vstr
    object
    ev
    vec
    builder
    parser
    clap
    config_parser
)

target_link_libraries(
    lexi-cli
    vstr
    object
    util
    hilexi
)

