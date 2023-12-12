include(FetchContent)

set(LIBCHECK_VERSION "0.15.2")

# Use FetchContent to download and include libcheck
FetchContent_Declare(
    libcheck
    URL https://github.com/libcheck/check/archive/${LIBCHECK_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP true  # Specify the option here
)

FetchContent_MakeAvailable(libcheck)

# ht test
add_executable(ht_test ht_test.c)

target_link_libraries(ht_test PUBLIC check ht pthread)

target_include_directories(ht_test PUBLIC "${PROJECT_BINARY_DIR}")

add_test(NAME ht_test COMMAND ht_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Testing)
set_tests_properties(ht_test PROPERTIES TIMEOUT 30)
