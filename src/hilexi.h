#ifndef __HI_LEXI_H__

#define __HI_LEXI_H__

#include "objects.h"
#include <stdint.h>
#include <stddef.h>

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
} HiLexi;

typedef enum {
    OK,
    ERR
} HiLexiResultT;

typedef enum {
    HL_ARR,
    HL_BULK_STRING,
    HL_SIMPLE_STRING,
    HL_INT,
    HL_ERR,
    HL_INV
} HiLexiDataType;

typedef enum {
    HL_INVSS,
    HL_PONG,
    HL_NONE,
    HL_OK
} HiLexiSimpleString;

struct HiLexiData;

typedef union {
    struct HiLexiData* arr;
    String* string; // used for both HL_BULK_STRING and HL_ERR
    int64_t integer;
    HiLexiSimpleString simple;
} HiLexiDataD;

typedef struct HiLexiData {
    HiLexiDataType type;
    HiLexiDataD val;
} HiLexiData;

typedef struct {
    HiLexiDataType type;
    HiLexiData data;
} HiLexiResult;

HiLexi* hilexi_new(char* addr, uint16_t port);
int hilexi_connect(HiLexi* l);
int hilexi_ping(HiLexi* l);
int hilexi_set(HiLexi* l, uint8_t* key, size_t key_len, char* value, size_t value_len);
int hilexi_set_int(HiLexi* l, uint8_t* key, size_t key_len, int64_t value);
int hilexi_get(HiLexi* l, uint8_t* key, size_t key_len);
int hilexi_del(HiLexi* l, uint8_t* key, size_t key_len);
void hilexi_disconnect(HiLexi* l);
void hilexi_destory(HiLexi* l);

#endif
