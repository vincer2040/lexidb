#include "reply.h"

err_reply_init(unauthed, "EUNAUTHED", 9);
err_reply_init(invalid_command, "EINVCMD", 7);
err_reply_init(bad_auth, "EBADAUTH", 8);
err_reply_init(invalid_key, "EINVKEY", 7);
err_reply_init(oom, "EOOM", 4);
