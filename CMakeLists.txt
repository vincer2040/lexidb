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

add_executable(
    lexidb
    src/lexidb.c
)
