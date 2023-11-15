#ifndef __HI_LEXI_H__

#define __HI_LEXI_H__

#include "builder.h"
#include "objects.h"
#include "vec.h"
#include "vstring.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int sfd;
    uint32_t addr;
    uint16_t port;
    uint16_t flags; // first bit is used to check if client is connected
    uint8_t* read_buf;
    size_t read_pos;
    size_t read_cap;
    uint8_t* write_buf;
    size_t write_pos;
    size_t write_len;
    Builder builder;
} HiLexi;

typedef enum {
    HL_IO,
    HL_NO_MEM,
    HL_INV_DATA,
} HiLexiErr;

typedef enum {
    HL_ERR,
    HL_ARR,
    HL_BULK_STRING,
    HL_SIMPLE_STRING,
    HL_INT,
    HL_INV,
} HiLexiDataType;

typedef enum {
    HL_INVSS,
    HL_PONG,
    HL_NONE,
    HL_OK,
} HiLexiSimpleString;

typedef union {
    HiLexiErr err;
    Vec* arr;
    vstr string;
    int64_t integer;
    HiLexiSimpleString simple;
} HiLexiDataD;

typedef struct {
    HiLexiDataType type;
    HiLexiDataD val;
} HiLexiData;

/* set up */
HiLexi* hilexi_new(const char* addr, uint16_t port);
int hilexi_connect(HiLexi* l);

/* ping */
HiLexiData hilexi_ping(HiLexi* l);

/* regular */
HiLexiData hilexi_set(HiLexi* l, const char* key, size_t key_len,
                      const char* value, size_t val_len);
HiLexiData hilexi_set_int(HiLexi* l, const char* key, size_t key_len,
                          int64_t val);
HiLexiData hilexi_get(HiLexi* l, const char* key, size_t key_len);
HiLexiData hilexi_del(HiLexi* l, const char* key, size_t key_len);
HiLexiData hilexi_keys(HiLexi* l);
HiLexiData hilexi_values(HiLexi* l);
HiLexiData hilexi_entries(HiLexi* l);
HiLexiData hilexi_push(HiLexi* l, const char* val, size_t val_len);
HiLexiData hilexi_push_int(HiLexi* l, int64_t val);
HiLexiData hilexi_pop(HiLexi* l);
HiLexiData hilexi_enque(HiLexi* l, const char* val, size_t val_len);
HiLexiData hilexi_enque_int(HiLexi* l, int64_t val);
HiLexiData hilexi_deque(HiLexi* l);

/* clusters */
HiLexiData hilexi_cluster_new(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len);
HiLexiData hilexi_cluster_drop(HiLexi* l, const char* cluster_name,
                               size_t cluster_name_len);
HiLexiData hilexi_cluster_set(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len, const char* key,
                              size_t key_len, const char* val, size_t val_len);
HiLexiData hilexi_cluster_set_int(HiLexi* l, const char* cluster_name,
                                  size_t cluster_name_len, const char* key,
                                  size_t key_len, int64_t val);
HiLexiData hilexi_cluster_get(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len, const char* key,
                              size_t key_len);
HiLexiData hilexi_cluster_del(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len, const char* key,
                              size_t key_len);
HiLexiData hilexi_cluster_push(HiLexi* l, const char* cluster_name,
                               size_t cluster_name_len, const char* val,
                               size_t val_len);
HiLexiData hilexi_cluster_push_int(HiLexi* l, const char* cluster_name,
                                   size_t cluster_name_len, int64_t val);
HiLexiData hilexi_cluster_pop(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len);
HiLexiData hilexi_cluster_keys(HiLexi* l, const char* cluster_name,
                               size_t cluster_name_len);
HiLexiData hilexi_cluster_values(HiLexi* l, const char* cluster_name,
                                 size_t cluster_name_len);
HiLexiData hilexi_cluster_entries(HiLexi* l, const char* cluster_name,
                                  size_t cluster_name_len);

/* teardown */
void hilexi_disconnect(HiLexi* l);
void hilexi_destory(HiLexi* l);
void hilexi_data_free(HiLexiData* data);

#endif
