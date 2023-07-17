cmake_minimum_required(VERSION 3.10)

project(lexidb VERSION 0.1)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_C_COMPILER "gcc")

add_compile_options(-std=c99 -Wall -Werror -pedantic -fstack-clash-protection -fstack-protector-all -fstack-protector-strong
    -Werror=format-security -Werror=implicit-function-declaration -pipe -g -O2)

add_executable(lexidb src/cmd.c src/parser.c src/lexer.c src/builder.c
    src/server.c src/de.c src/sock.c src/sha256.c src/util.c
    src/siphash.c src/ht.c src/db.c)
add_executable(lexer_test src/lexer.c tests/test_lexer.c)
add_executable(parser_test src/cmd.c src/parser.c src/lexer.c tests/test_parser.c)



