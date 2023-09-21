# lexer test
add_executable(lexer_test lexer_test.c)

target_link_libraries(lexer_test PUBLIC builder cmd check pthread)

target_include_directories(lexer_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME lexer_test COMMAND lexer_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(lexer_test PROPERTIES TIMEOUT 30)

# parser test
add_executable(parser_test parser_test.c)

target_link_libraries(parser_test PUBLIC builder cmd check pthread)

target_include_directories(parser_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME parser_test COMMAND parser_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(parser_test PROPERTIES TIMEOUT 30)
