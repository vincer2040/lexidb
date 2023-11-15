#ifndef __HI_LEXI_H__

#define __HI_LEXI_H__

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
    size_t unpack_pos;
    uint8_t* write_buf;
    size_t write_pos;
    size_t write_len;
} HiLexi;

typedef enum { HI_OK, HI_ERR } HiLexiResultT;

typedef enum {
    HL_ARR,
    HL_BULK_STRING,
    HL_SIMPLE_STRING,
    HL_INT,
    HL_ERR,
    HL_INV
} HiLexiDataType;

typedef enum { HL_INVSS, HL_PONG, HL_NONE, HL_OK } HiLexiSimpleString;

typedef union {
    Vec* arr;
    vstr string; // used for both HL_BULK_STRING and HL_ERR
    int64_t integer;
    HiLexiSimpleString simple;
} HiLexiDataD;

typedef struct {
    HiLexiDataType type;
    HiLexiDataD val;
} HiLexiData;

typedef struct {
    HiLexiDataType type;
    HiLexiData data;
} HiLexiResult;

/* set up */
HiLexi* hilexi_new(const char* addr, uint16_t port);
int hilexi_connect(HiLexi* l);

/* simple commands */
int hilexi_ping(HiLexi* l);

/* ht commands */
int hilexi_set(HiLexi* l, uint8_t* key, size_t key_len, char* value,
               size_t value_len);
int hilexi_set_int(HiLexi* l, uint8_t* key, size_t key_len, int64_t value);
int hilexi_get(HiLexi* l, uint8_t* key, size_t key_len);
int hilexi_del(HiLexi* l, uint8_t* key, size_t key_len);
int hilexi_keys(HiLexi* l);
int hilexi_values(HiLexi* l);
int hilexi_entries(HiLexi* l);

/* vec commands */
int hilexi_push(HiLexi* l, char* value, size_t value_len);
int hilexi_push_int(HiLexi* l, int64_t value);
int hilexi_pop(HiLexi* l);

/* queue commands */
int hilexi_enque(HiLexi* l, char* value, size_t value_len);
int hilexi_enque_int(HiLexi* l, int64_t value);
int hilexi_deque(HiLexi* l);

/* cluster commands */
int hilexi_cluster_new(HiLexi* l, uint8_t* name, size_t name_len);
int hilexi_cluster_set(HiLexi* l, uint8_t* name, size_t name_len, uint8_t* key,
                       size_t key_len, char* value, size_t value_len);
int hilexi_cluster_set_int(HiLexi* l, uint8_t* name, size_t name_len,
                           uint8_t* key, size_t key_len, int64_t value);
int hilexi_cluster_get(HiLexi* l, uint8_t* name, size_t name_len, uint8_t* key,
                       size_t key_len);
int hilexi_cluster_del(HiLexi* l, uint8_t* name, size_t name_len, uint8_t* key,
                       size_t key_len);
int hilexi_cluster_push(HiLexi* l, uint8_t* name, size_t name_len, char* value,
                        size_t value_len);
int hilexi_cluster_push_int(HiLexi* l, uint8_t* name, size_t name_len,
                            int64_t value);
int hilexi_cluster_pop(HiLexi* l, uint8_t* name, size_t name_len);
int hilexi_cluster_drop(HiLexi* l, uint8_t* name, size_t name_len);
int hilexi_cluster_keys(HiLexi* l, uint8_t* name, size_t name_len);
int hilexi_cluster_values(HiLexi* l, uint8_t* name, size_t name_len);
int hilexi_cluster_entries(HiLexi* l, uint8_t* name, size_t name_len);
int hilexi_stats_cycles(HiLexi* l);

/* teardown */
void hilexi_disconnect(HiLexi* l);
void hilexi_destory(HiLexi* l);

#endif
