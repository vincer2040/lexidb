cmake_minimum_required(VERSION 3.10)

project(lexidb VERSION 0.1)

enable_testing()

add_subdirectory(tests)

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

add_executable(
    lexidb
    src/db.c
)

target_link_libraries(
    ht
    util
)
