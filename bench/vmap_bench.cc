#include <benchmark/benchmark.h>
extern "C" {
#include "../src/vmap.h"
#include "../src/vstr.h"
#include "../src/object.h"
#include "../src/util.h"
#include "../src/siphash.h"
#include "../src/murmur3.h"
#include "rand_keys.h"
}

#define SEED_SIZE 16
#define arr_size(arr) sizeof arr / sizeof arr[0]

vmap_type type;

unsigned char siphash_seed[SEED_SIZE];
uint32_t murmur_seed = 0;

uint64_t hash_shiphash(const void* key) {
    const char* k = vstr_data((const vstr*)key);
    size_t k_len = vstr_len((const vstr*)key);
    return siphash((const uint8_t*)k, k_len, siphash_seed);
}

uint64_t hash_murmur1(const void* key) {
    uint64_t hash[2] = {0};
    const char* k = vstr_data((const vstr*)key);
    size_t k_len = vstr_len((const vstr*)key);
    MurmurHash3_x64_128(k, k_len, murmur_seed, hash);
    return hash[0] ^ hash[1];
}

uint64_t hash_murmur2(const void* key) {
    uint64_t hash[2] = {0};
    const char* k = vstr_data((const vstr*)key);
    size_t k_len = vstr_len((const vstr*)key);
    MurmurHash3_x86_128(k, k_len, murmur_seed, hash);
    return hash[0] ^ hash[1];
}

int key_cmp(const void* a, const void* b) { return vstr_cmp((const vstr*)a, (const vstr*)b); }

void key_free(void* key) { vstr_free((vstr*)key); }

void value_free(void* value) { object_free((object*)value); }

void init(void) {
    size_t i;
    get_random_bytes(siphash_seed, SEED_SIZE);
    for (i = 0; i < SEED_SIZE; ++i) {
        murmur_seed += siphash_seed[i];
    }
    type.key_size = sizeof(vstr);
    type.value_size = sizeof(object);
    type.key_free = key_free;
    type.value_free = value_free;
    type.key_cmp = key_cmp;
    type.hash = hash_shiphash;
}

static void clobber() {
    asm volatile("" : : : "memory");
}

/* this just ensures that we get the same seed */
static void benchmark_init(benchmark::State& state) {
    init();
    for (auto _ : state) {
        clobber();
    }
}

static void bench_vmap_siphash(benchmark::State& state) {
    size_t i, len = 367001;
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        char* t = keys[i];
        vstr key = vstr_from(t);
        object value = {.type=OT_Int, .data = {.integer = (int64_t)i}};
        vmap_insert(&map, &key, &value);
    }

    vstr key = vstr_from(keys[170000]);

    for (auto _ : state) {
        const void* x = vmap_find(map, &key);
        clobber();
    }

    vmap_free(map);
}

static void bench_vmap_murmur1(benchmark::State& state) {
    type.hash = hash_murmur1;

    size_t i, len = 367001;
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        char* t = keys[i];
        vstr key = vstr_from(t);
        object value = {.type=OT_Int, .data = {.integer = (int64_t)i}};
        vmap_insert(&map, &key, &value);
    }

    vstr key = vstr_from(keys[170000]);

    for (auto _ : state) {
        const void* x = vmap_find(map, &key);
        clobber();
    }

    vmap_free(map);
}

static void bench_vmap_murmur2(benchmark::State& state) {
    type.hash = hash_murmur2;

    size_t i, len = 367001;
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        char* t = keys[i];
        vstr key = vstr_from(t);
        object value = {.type=OT_Int, .data = {.integer = (int64_t)i}};
        vmap_insert(&map, &key, &value);
    }

    vstr key = vstr_from(keys[170000]);

    for (auto _ : state) {
        const void* x = vmap_find(map, &key);
        clobber();
    }

    vmap_free(map);
}

static void bench_vmap_siphash_after_rehash(benchmark::State& state) {
    size_t i, len = 367002;
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        char* t = keys[i];
        vstr key = vstr_from(t);
        object value = {.type=OT_Int, .data = {.integer = (int64_t)i}};
        vmap_insert(&map, &key, &value);
    }

    vstr key = vstr_from(keys[170000]);

    for (auto _ : state) {
        const void* x = vmap_find(map, &key);
        clobber();
    }

    vmap_free(map);
}

static void bench_vmap_murmur1_after_rehash(benchmark::State& state) {
    type.hash = hash_murmur1;

    size_t i, len = 367002;
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        char* t = keys[i];
        vstr key = vstr_from(t);
        object value = {.type=OT_Int, .data = {.integer = (int64_t)i}};
        vmap_insert(&map, &key, &value);
    }

    vstr key = vstr_from(keys[170000]);

    for (auto _ : state) {
        const void* x = vmap_find(map, &key);
        clobber();
    }

    vmap_free(map);
}

static void bench_vmap_murmur2_after_rehash(benchmark::State& state) {
    type.hash = hash_murmur2;

    size_t i, len = 367002;
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        char* t = keys[i];
        vstr key = vstr_from(t);
        object value = {.type=OT_Int, .data = {.integer = (int64_t)i}};
        vmap_insert(&map, &key, &value);
    }

    vstr key = vstr_from(keys[170000]);

    for (auto _ : state) {
        const void* x = vmap_find(map, &key);
        clobber();
    }

    vmap_free(map);
}

/**/
BENCHMARK(benchmark_init);

BENCHMARK(bench_vmap_siphash);
BENCHMARK(bench_vmap_murmur1);
BENCHMARK(bench_vmap_murmur2);

BENCHMARK(bench_vmap_siphash_after_rehash);
BENCHMARK(bench_vmap_murmur1_after_rehash);
BENCHMARK(bench_vmap_murmur2_after_rehash);

BENCHMARK_MAIN();
