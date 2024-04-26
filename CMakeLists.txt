cmake_minimum_required(VERSION 3.10)

project(lexidb VERSION 0.0.1)

include(CheckIncludeFile)

check_include_file("sys/epoll.h" HAVE_EPOLL)

configure_file(config.h.in "../src/config.h")

enable_testing()

add_subdirectory(tests)

set(CMAKE_C_COMPILER "gcc")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -std=c99 -Wall -Werror -pedantic -fstack-clash-protection \
-fstack-protector-all -fstack-protector-strong -Werror=format-security -Werror=implicit-function-declaration -pipe -O2")

set(CMAKE_C_FLAGS_RELEASE "-std=c99 -Wall -Werror -pedantic -fstack-clash-protection -fstack-protector-all \
-fstack-protector-strong -Werror=format-security -Werror=implicit-function-declaration -pipe -O2 -s -DNDEBUG")

add_library(
    vstr
    src/vstr.c
)

add_library(
    ev
    src/ev.c
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
    builder
    src/builder.c
)

add_library(
    vec
    src/vec.c
)

add_library(
    sha256
    src/sha256.c
)

add_library(
    siphash
    src/siphash.c
)

add_library(
    util
    src/util.c
)

add_library(
    vmap
    src/vmap.c
)

add_library(
    vnet
    src/vnet.c
)

add_executable(
    lexidb
    src/lexidb.c
    src/server.c
    src/configure.c
    src/vmap_type.c
    src/connection.c
    src/networking.c
    src/cmd.c
    src/db.c
)

target_link_libraries(
    lexidb
    vmap
    object
    siphash
    vstr
    vec
    util
    pthread
    ev
    vnet
    builder
    parser
)

target_link_libraries(
    builder
    vstr
)

target_link_libraries(
    object
    vstr
)

target_link_libraries(
    parser
    object
    m
)

target_link_libraries(
    util
    sha256
    vstr
)
