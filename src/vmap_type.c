#include "object.h"
#include "siphash.h"
#include "util.h"
#include "vmap.h"
#include "vstr.h"

#define SEED_SIZE 16

static vmap_type vt = {0};

static unsigned char seed[SEED_SIZE] = {0};

static uint64_t hash_fn(const void* key) {
    const vstr* k = key;
    const char* str = vstr_data(k);
    size_t len = vstr_len(k);
    return siphash((const uint8_t*)str, len, seed);
}

static int key_cmp(const void* a, const void* b) { return vstr_cmp(a, b); }

static void key_free(void* key) { vstr_free(key); }

static void value_free(void* value) { object_free(value); }

void init_vmap_type(void) {
    get_random_bytes(seed, SEED_SIZE);
    vt.key_size = sizeof(vstr);
    vt.value_size = sizeof(object);
    vt.hash = hash_fn;
    vt.key_cmp = key_cmp;
    vt.key_free = key_free;
    vt.value_free = value_free;
}

vmap_type* get_vmap_type(void) { return &vt; }
