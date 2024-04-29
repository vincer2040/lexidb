#ifndef __AUTH_H__

#define __AUTH_H__

#include "server.h"
#include "vstr.h"
#include <stddef.h>

vstr hash_password(const char* p, size_t len);
int time_safe_compare(const char* a, const char* b, size_t len);
user* authenicate_user(const vstr* username, const vstr* password);

#endif /* __AUTH_H__ */
