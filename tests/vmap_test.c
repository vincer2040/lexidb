#include "../src/object.h"
#include "../src/siphash.h"
#include "../src/util.h"
#include "../src/vmap.h"
#include "../src/vstr.h"
#include <check.h>
#include <stdlib.h>

#define SEED_SIZE 16

#define arr_size(arr) sizeof arr / sizeof arr[0]

vmap_type type;

unsigned char seed[SEED_SIZE];

uint64_t hash(const void* key) { return siphash(key, sizeof(vstr), seed); }

uint64_t determinitistic_hash(const void* key) {
    static uint64_t cur = 0;
    ((void)key);
    return cur++;
}

int key_cmp(const void* a, const void* b) { return vstr_cmp(a, b); }

void key_free(void* key) { vstr_free(key); }

void value_free(void* value) { object_free(value); }

void init(void) {
    get_random_bytes(seed, SEED_SIZE);
    type.key_size = sizeof(vstr);
    type.value_size = sizeof(object);
    type.key_free = key_free;
    type.value_free = value_free;
    type.key_cmp = key_cmp;
    type.hash = hash;
}

typedef struct {
    const char* key;
    int64_t value;
} kv_test;

START_TEST(test_it_works) {
    kv_test tests[] = {
        {"foo", 1},     {"bar", 2},        {"baz", 3},        {"foobar", 4},
        {"foobaz", 5},  {"foobarbaz", 6},  {"foobazbar", 7},  {"barfoo", 8},
        {"barbaz", 9},  {"barfoobaz", 10}, {"barbazfoo", 11}, {"bazfoo", 12},
        {"bazbar", 13}, {"bazfoobar", 14}, {"bazbarfoo", 15}, {"abc", 16},
        {"xyz", 17},    {"cba", 18},       {"zyx", 19},       {"vmap", 20},
        {"lexidb", 21}, {"abcd", 22},      {"dcba", 23},
    };
    size_t i, len = arr_size(tests);
    vmap* map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        object value = {OT_Int, .data = {.integer = t.value}};
        const object* to = vmap_find(map, &key);
        ck_assert_ptr_null(to);
        int res = vmap_insert(&map, &key, &value);
        ck_assert_int_eq(res, VMAP_OK);
        res = vmap_has(map, &key);
        ck_assert_int_eq(res, 1);
    }
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        const object* res = vmap_find(map, &key);
        int64_t got;
        ck_assert_ptr_nonnull(res);
        got = res->data.integer;
        ck_assert_int_eq(got, t.value);
        vstr_free(&key);
    }
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        int res = vmap_delete(&map, &key);
        ck_assert_int_eq(res, VMAP_OK);
        vstr_free(&key);
        res = vmap_has(map, &key);
        ck_assert_int_eq(res, 0);
    }
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        int res = vmap_delete(&map, &key);
        ck_assert_int_eq(res, VMAP_NO_KEY);
        vstr_free(&key);
    }
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        const object* res = vmap_find(map, &key);
        ck_assert_ptr_null(res);
        int x = vmap_has(map, &key);
        ck_assert_int_eq(x, 0);
        vstr_free(&key);
    }
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        object value = {OT_Int, .data = {.integer = t.value}};
        int res = vmap_insert(&map, &key, &value);
        ck_assert_int_eq(res, VMAP_OK);
        res = vmap_has(map, &key);
        ck_assert_int_eq(res, 1);
    }
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        const object* res = vmap_find(map, &key);
        int64_t got;
        ck_assert_ptr_nonnull(res);
        got = res->data.integer;
        ck_assert_int_eq(got, t.value);
        int x = vmap_has(map, &key);
        ck_assert_int_eq(x, 1);
        vstr_free(&key);
    }
    vmap_free(map);
}
END_TEST

START_TEST(test_vmap_iter) {
    kv_test tests[] = {
        {"foo", 1},     {"bar", 2},        {"baz", 3},        {"foobar", 4},
        {"foobaz", 5},  {"foobarbaz", 6},  {"foobazbar", 7},  {"barfoo", 8},
        {"barbaz", 9},  {"barfoobaz", 10}, {"barbazfoo", 11}, {"bazfoo", 12},
        {"bazbar", 13}, {"bazfoobar", 14}, {"bazbarfoo", 15}, {"abc", 16},
        {"xyz", 17},    {"cba", 18},       {"zyx", 19},       {"vmap", 20},
        {"lexidb", 21}, {"abcd", 22},      {"dcba", 23},
    };
    size_t i, len = arr_size(tests);
    vmap* map;
    vmap_iter iter;
    type.hash = determinitistic_hash;
    map = vmap_new(&type);
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        vstr key = vstr_from(t.key);
        ck_assert_int_eq(vmap_insert(&map, &key, &t.value), VMAP_OK);
    }
    iter = vmap_iter_new(map);
    for (i = 0; i < len; ++i) {
        kv_test t = tests[i];
        const vmap_entry* cur = iter.cur;
        ck_assert_ptr_nonnull(cur);
        const vstr* key = vmap_entry_get_key(cur);
        const int* value = vmap_entry_get_value(map, cur);
        ck_assert_str_eq(t.key, vstr_data(key));
        ck_assert_int_eq(*value, t.value);
        vmap_iter_next(&iter);
    }

    vmap_free(map);
}
END_TEST

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("vmap");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_it_works);
    tcase_add_test(tc_core, test_vmap_iter);
    suite_add_tcase(s, tc_core);
    return s;
}

int main(void) {
    int num_failed;
    Suite* s;
    SRunner* sr;
    init();
    s = suite();
    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (num_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
