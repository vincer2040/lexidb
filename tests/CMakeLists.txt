# lexer test
add_executable(lexer_test lexer_test.c)

target_link_libraries(lexer_test PUBLIC builder cmd check m pthread)

target_include_directories(lexer_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME lexer_test COMMAND lexer_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(lexer_test PROPERTIES TIMEOUT 30)

# parser test
add_executable(parser_test parser_test.c)

target_link_libraries(parser_test PUBLIC builder cmd check m pthread)

target_include_directories(parser_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME parser_test COMMAND parser_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(parser_test PROPERTIES TIMEOUT 30)

# cmd test
add_executable(cmd_test cmd_test.c)

target_link_libraries(cmd_test PUBLIC builder cmd check m pthread)

target_include_directories(cmd_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME cmd_test COMMAND cmd_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(cmd_test PROPERTIES TIMEOUT 30)

# vec test
add_executable(vec_iter_test vec_iter_test.c)

target_link_libraries(vec_iter_test PUBLIC vec check m pthread)
target_include_directories(vec_iter_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME vec_iter_test COMMAND vec_iter_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(vec_iter_test PROPERTIES TIMEOUT 30)

