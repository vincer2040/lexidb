cmake_minimum_required(VERSION 3.10)

project(lexidb VERSION 0.1)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_C_COMPILER "gcc")

add_compile_options(-std=c99 -Wall -Werror -pedantic -fstack-clash-protection -fstack-protector-all -fstack-protector-strong
    -Werror=format-security -Werror=implicit-function-declaration -pipe -g -O2)

add_library(objects src/objects.c src/vstring.c)
add_library(builder src/builder.c)
add_library(sock src/sock.c)
add_library(cmd src/cmd.c src/lexer.c src/parser.c)
add_library(de src/de.c)

add_executable(lexer_test tests/test_lexer.c)
add_executable(parser_test tests/test_parser.c)
add_executable(ht_test tests/test_ht_resize.c src/ht.c src/util.c src/sha256.c src/siphash.c)

add_executable(lexidb src/cluster.c src/log.c
    src/server.c src/sha256.c src/util.c
    src/siphash.c src/ht.c src/db.c)

add_executable(lexi-cli src/cli.c src/cli-util.c src/cli-cmd.c src/hilexi.c)


target_link_libraries(lexidb de objects builder sock cmd)
target_link_libraries(lexi-cli objects builder sock cmd)

target_link_libraries(lexer_test builder cmd)
target_link_libraries(parser_test builder cmd)

