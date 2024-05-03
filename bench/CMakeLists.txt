include(../cmake/GoogleBenchmark.cmake)

add_executable(
    vmap_bench
    vmap_bench.cc
)

target_link_libraries(
    vmap_bench
    benchmark::benchmark
    vmap
    vstr
    object
    siphash
    util
    murmur3
)
