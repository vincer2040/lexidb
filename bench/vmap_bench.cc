#include <benchmark/benchmark.h>
extern "C" {
#include "../src/vmap.h"
#include "../src/vstr.h"
#include "../src/object.h"
#include "../src/util.h"
#include "../src/siphash.h"
#include "../src/murmur3.h"
}

#define SEED_SIZE 16
#define arr_size(arr) sizeof arr / sizeof arr[0]

vmap_type type;

unsigned char siphash_seed[SEED_SIZE];
uint32_t murmur_seed = 0;

uint64_t hash_shiphash(const void* key) { return siphash((const uint8_t*)key, sizeof(vstr), siphash_seed); }

uint64_t hash_murmur(const void* key) {
    uint64_t hash[2] = {0};
    MurmurHash3_x64_128(key, sizeof(vstr), murmur_seed, hash);
    return hash[0];
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

static void bench_vmap_siphash(benchmark::State& state) {
    init();

    typedef struct {
        const char* key;
        int64_t value;
    } kv_test;

    kv_test tests[] = {
        {"foo", 1},     {"bar", 2},        {"baz", 3},        {"foobar", 4},
        {"foobaz", 5},  {"foobarbaz", 6},  {"foobazbar", 7},  {"barfoo", 8},
        {"barbaz", 9},  {"barfoobaz", 10}, {"barbazfoo", 11}, {"bazfoo", 12},
        {"bazbar", 13}, {"bazfoobar", 14}, {"bazbarfoo", 15}, {"abc", 16},
        {"xyz", 17},    {"cba", 18},       {"zyx", 19},       {"vmap", 20},
        {"lexidb", 21}, {"abcd", 22},
    };

    size_t i, len = arr_size(tests);
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        object value = {.type=OT_Int, .data = {.integer = t.value}};
        vmap_insert(&map, &key, &value);
    }

    vstr key = vstr_from("foo");

    for (auto _ : state) {
        const void* x = vmap_find(map, &key);
    }

    vmap_free(map);
}

static void bench_vmap_murmur(benchmark::State& state) {
    init();

    typedef struct {
        const char* key;
        int64_t value;
    } kv_test;

    kv_test tests[] = {
        {"foo", 1},     {"bar", 2},        {"baz", 3},        {"foobar", 4},
        {"foobaz", 5},  {"foobarbaz", 6},  {"foobazbar", 7},  {"barfoo", 8},
        {"barbaz", 9},  {"barfoobaz", 10}, {"barbazfoo", 11}, {"bazfoo", 12},
        {"bazbar", 13}, {"bazfoobar", 14}, {"bazbarfoo", 15}, {"abc", 16},
        {"xyz", 17},    {"cba", 18},       {"zyx", 19},       {"vmap", 20},
        {"lexidb", 21}, {"abcd", 22},
    };

    size_t i, len = arr_size(tests);
    type.hash = hash_murmur;
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        object value = {.type=OT_Int, .data = {.integer = t.value}};
        vmap_insert(&map, &key, &value);
    }

    vstr key = vstr_from("foo");

    for (auto _ : state) {
        const void* x = vmap_find(map, &key);
    }

    vmap_free(map);
}

BENCHMARK(bench_vmap_siphash);
BENCHMARK(bench_vmap_murmur);

BENCHMARK_MAIN();
