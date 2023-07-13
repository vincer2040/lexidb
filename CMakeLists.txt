cmake_minimum_required(VERSION 3.10)

project(lexidb VERSION 0.1)

set(CMAKE_C_COMPILER "clang")

add_compile_options(-std=c99)

set(CMAKE_C_FLAGS "-Wall -Werror -pedantic\
 -fstack-clash-protection -fstack-protector-all -fstack-protector-strong\
 -Werror=format-security -Werror=implicit-function-declaration\
 -g -O2 -pipe")

add_executable(lexidb src/sha256.c src/util.c src/siphash.c src/ht.c src/db.c)



