cmake_minimum_required(VERSION 3.10)

project(lexidb VERSION 0.1)

enable_testing()

add_subdirectory(tests)

set(CMAKE_C_COMPILER "gcc")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -std=c99 -Wall -Werror -pedantic -fstack-clash-protection \
-fstack-protector-all -fstack-protector-strong -Werror=format-security -Werror=implicit-function-declaration -pipe -O2")

set(CMAKE_C_FLAGS_RELEASE "-std=c99 -Wall -Werror -pedantic -fstack-clash-protection -fstack-protector-all \
-fstack-protector-strong -Werror=format-security -Werror=implicit-function-declaration -pipe -O2 -s -DNDEBUG")

add_library(vec src/vec.c)

add_library(builder src/builder.c)

add_library(de src/de.c)

add_library(queue src/queue.c)

add_library(sock src/sock.c)

add_library(config src/config.c)

add_library(
    objects
    src/objects.c
    src/vstring.c
)

add_library(
    cmd
    src/cmd.c
    src/lexer.c
    src/parser.c
)

add_library(
    ht
    src/ht.c
    src/sha256.c
    src/util.c
    src/siphash.c
)

add_library(
    hilexi
    src/hilexi.c
    src/hilexi_parser.c
    src/builder.c
)

add_executable(
    lexidb
    src/cluster.c
    src/log.c
    src/server.c
    src/db.c
    src/replication.c
    src/stats.c
)

add_executable(
    lexi-cli
    src/cli.c
    src/cli-util.c
    src/cli-cmd.c
)

target_link_libraries(
    hilexi
    sock
)

target_link_libraries(
    config
    objects
    ht
)

target_link_libraries(
    lexidb
    ht
    de
    objects
    builder
    sock
    cmd
    vec
    queue
    config
)

target_link_libraries(
    lexi-cli
    objects
    builder
    sock
    cmd
    vec
    config
    hilexi
)

