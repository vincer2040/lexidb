#ifndef __REPLY_H__

#define __REPLY_H__

#include <stddef.h>

#define err_reply_t(name)                                                      \
    typedef struct {                                                           \
        const char* str;                                                       \
        size_t str_len;                                                        \
    } err_reply_##name

#define err_reply_init(name, str, str_len)                                     \
    err_reply_##name err_##name = {str, str_len}

#define err_reply_def(name) extern err_reply_##name err_##name

err_reply_t(unauthed);
err_reply_def(unauthed);

err_reply_t(invalid_command);
err_reply_def(invalid_command);

err_reply_t(bad_auth);
err_reply_def(bad_auth);

err_reply_t(invalid_key);
err_reply_def(invalid_key);

err_reply_t(oom);
err_reply_def(oom);

err_reply_t(dbrange);
err_reply_def(dbrange);

#endif /* __REPLY_H__ */
