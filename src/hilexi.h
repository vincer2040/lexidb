#ifndef __HI_LEXI_H__

#define __HI_LEXI_H__

#include <stddef.h>
#include <stdint.h>

typedef struct hilexi hilexi;

typedef enum {
    HLOT_Null,
    HLOT_Error,
    HLOT_Int,
    HLOT_Double,
    HLOT_String,
    HLOT_Array,
} hilexi_obj_type;

typedef struct hilexi_array hilexi_array;
typedef char* hilexi_string;

typedef struct {
    hilexi_obj_type type;
    union {
        int64_t integer;
        double dbl;
        hilexi_string string;
        hilexi_array* array;
    } data;
} hilexi_obj;

extern void* hilexi_nil;

hilexi* hilexi_new(const char* ip, uint16_t port);
int hilexi_has_error(hilexi* hl);
hilexi_string hilexi_get_error(hilexi* hl);
int hilexi_connnect(hilexi* hl);
hilexi_string hilexi_ping(hilexi* hl);
hilexi_string hilexi_set(hilexi* hl, const char* key, const char* value);
hilexi_string hilexi_get(hilexi* hl, const char* key);
hilexi_string hilexi_del(hilexi* hl, const char* key);
void hilexi_close(hilexi* hl);

void hilexi_obj_print(const hilexi_obj* obj);

size_t hilexi_array_length(const hilexi_array* arr);
const hilexi_obj* hilexi_array_get_at(const hilexi_array* arr, size_t idx);

size_t hilexi_string_len(const hilexi_string str);

void hilexi_obj_free(hilexi_obj* obj);
void hilexi_array_free(hilexi_array* arr);
void hilexi_string_free(hilexi_string str);

#endif /* __HI_LEXI_H__ */
