include(FetchContent)

set(LIBCHECK_VERSION "0.15.2")

# Use FetchContent to download and include libcheck
FetchContent_Declare(
    libcheck
    URL https://github.com/libcheck/check/archive/${LIBCHECK_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP true  # Specify the option here
)

FetchContent_MakeAvailable(libcheck)

# lexer test
# add_executable(lexer_test lexer_test.c)
#
# target_link_libraries(lexer_test PUBLIC builder cmd check m pthread)
#
# target_include_directories(lexer_test PUBLIC "${PROJECT_BINARY_DIR}")
#
# add_test(NAME lexer_test COMMAND lexer_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
# set_tests_properties(lexer_test PROPERTIES TIMEOUT 30)
