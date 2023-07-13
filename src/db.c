#include "ht.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void fcb(void* value) { free(value); }

int main(int argc, char** argv) {
    Ht* ht;

    void *t1, *t2;
    if (argc > 1) {
        int i;
        for (i = 1; i < argc; ++i) {
            printf("%s\n", argv[i]);
        }
    }

    ht = ht_new(32);
    if (ht == NULL) {
        return 1;
    }

    ht_insert(ht, ((uint8_t*)"dom"), 3, "is cool", 8, fcb);
    ht_insert(ht, ((uint8_t*)"madi"), 4, "is cool", 8, fcb);
    ht_insert(ht, ((uint8_t*)"mom"), 3, "is cool", 8, fcb);
    ht_insert(ht, ((uint8_t*)"vince"), 5, "is cool", 8, fcb);
    ht_insert(ht, ((uint8_t*)"dad"), 3, "is cool", 8, fcb);
    ht_insert(ht, ((uint8_t*)"frank"), 5, "is cool", 8, fcb);
    ht_insert(ht, ((uint8_t*)"lomein"), 7, "is cool", 8, fcb);
    ht_insert(ht, ((uint8_t*)"kitty"), 5, "is cool", 8, fcb);
    ht_insert(ht, ((uint8_t*)"boller"), 6, "is cool", 8, fcb);

    t1 = ht_get(ht, ((uint8_t*)"vince"), 5);
    printf("%s\n", ((char*)t1));
    ht_delete(ht, ((uint8_t*)"vince"), 5);
    t2 = ht_get(ht, ((uint8_t*)"vince"), 5);
    printf("%p\n", t2);

    ht_print(ht);

    ht_free(ht);
    return 0;
}
