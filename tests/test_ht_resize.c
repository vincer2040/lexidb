#include "../src/ht.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define assert_mem_eq(a, b, size)                                              \
    {                                                                          \
        if (memcmp((a), (b), (size)) != 0) {                                   \
            fprintf(stderr, "%s:%d, %s != %s\n", __FILE__, __LINE__, (a),      \
                    (b));                                                      \
            abort();                                                           \
        }                                                                      \
    }

void fcb(void* ptr) {
    free(ptr);
}


int main(void) {
    Ht* ht = ht_new(2);
    uint8_t* key0 = ((uint8_t*)"vince");
    uint8_t* key1 = ((uint8_t*)"vince1");
    uint8_t* key2 = ((uint8_t*)"vince2");
    uint8_t* key3 = ((uint8_t*)"vince3");
    uint8_t* key4 = ((uint8_t*)"vince4");
    uint8_t* key5 = ((uint8_t*)"vince5");
    uint8_t* key6 = ((uint8_t*)"vince6");
    uint8_t* key7 = ((uint8_t*)"vince7");
    uint8_t* key8 = ((uint8_t*)"vince8");
    size_t key_len = strlen((char*)key0);
    size_t key1_len = strlen((char*)key1);
    char* value = "is cool";
    size_t value_size = strlen(value);

    ht_insert(ht, key0, key_len, value, value_size, fcb);
    ht_insert(ht, key1, key1_len, value, value_size, fcb);
    ht_insert(ht, key2, key1_len, value, value_size, fcb);
    ht_insert(ht, key3, key1_len, value, value_size, fcb);
    ht_insert(ht, key4, key1_len, value, value_size, fcb);
    ht_insert(ht, key5, key1_len, value, value_size, fcb);
    ht_insert(ht, key6, key1_len, value, value_size, fcb);
    ht_insert(ht, key7, key1_len, value, value_size, fcb);
    ht_insert(ht, key8, key1_len, value, value_size, fcb);

    ht_print(ht);

    ht_free(ht);
}
