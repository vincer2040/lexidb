#ifndef __RESULT_H__

#define __RESULT_H__

typedef enum {
    Ok,
    Err,
} result_type;

#define result_t(val_type, err_type)                                           \
    typedef struct {                                                           \
        result_type type;                                                      \
        union {                                                                \
            val_type ok;                                                       \
            err_type err;                                                      \
        } data;                                                                \
    } result_##val_type

#define result(val_type) result_##val_type

#endif /* __RESULT_H__ */
