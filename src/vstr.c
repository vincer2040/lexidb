#include "vstr.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static vstr_lg vstr_make_lg(const char* data);
static vstr_lg vstr_lg_new_len(size_t len);
static vstr_lg vstr_make_lg_len(const char* data, size_t len);
static int vstr_sm_push_char(vstr_sm* sm, char c, uint8_t avail);
static int vstr_lg_push_char(vstr_lg* lg, char c);
static int vstr_lg_push_string(vstr_lg* lg, const char* str, size_t str_len);
static int vstr_sm_push_string(vstr_sm* sm, const char* str, size_t str_len,
                               uint8_t avail);
static int vstr_realloc_lg(vstr_lg* lg, size_t len, size_t cap);
static int vstr_realloc_lg_len(vstr_lg* lg, size_t len, size_t cap,
                               size_t new_len);

vstr vstr_new(void) {
    vstr s = {0};
    s.is_large = 0;
    s.small_avail = VSTR_MAX_SMALL_SIZE;
    return s;
}

vstr vstr_new_len(size_t len) {
    vstr s = {0};
    if (len > VSTR_MAX_SMALL_SIZE) {
        assert(len <= VSTR_MAX_LARGE_SIZE);
        s.str_data.lg = vstr_lg_new_len(len);
        s.is_large = 1;
        return s;
    }
    s.small_avail = VSTR_MAX_SMALL_SIZE;
    return s;
}

vstr vstr_from(const char* cstr) {
    vstr s = {0};
    size_t cstr_len = strlen(cstr);
    if (cstr_len > VSTR_MAX_SMALL_SIZE) {
        s.str_data.lg = vstr_make_lg_len(cstr, cstr_len);
        if (s.str_data.lg.cap == 0) {
            return s;
        }
        s.is_large = 1;
        return s;
    }
    memcpy(s.str_data.sm.data, cstr, cstr_len);
    s.small_avail = VSTR_MAX_SMALL_SIZE - cstr_len;
    return s;
}

vstr vstr_from_len(const char* cstr, size_t len) {
    vstr s = {0};
    if (len > VSTR_MAX_SMALL_SIZE) {
        s.str_data.lg = vstr_make_lg_len(cstr, len);
        if (s.str_data.lg.cap == 0) {
            return s;
        }
        s.is_large = 1;
        return s;
    }
    memcpy(s.str_data.sm.data, cstr, len);
    s.small_avail = VSTR_MAX_SMALL_SIZE - len;
    return s;
}

vstr vstr_format(const char* fmt, ...) {
    vstr s;
    int n;
    va_list ap;
    size_t size;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    assert(n != 0);

    size = ((size_t)n + 1);

    s = vstr_new_len(size);

    if (s.is_large) {
        va_start(ap, fmt);
        n = vsnprintf(s.str_data.lg.data, size, fmt, ap);
        va_end(ap);
        s.str_data.lg.len = size - 1;
    } else {
        va_start(ap, fmt);
        n = vsnprintf(s.str_data.sm.data, size, fmt, ap);
        va_end(ap);
        s.small_avail = VSTR_MAX_SMALL_SIZE - size + 1;
    }

    assert(n != -1);
    return s;
}

size_t vstr_len(const vstr* s) {
    if (s->is_large) {
        return s->str_data.lg.len;
    }
    return VSTR_MAX_SMALL_SIZE - s->small_avail;
}

const char* vstr_data(const vstr* s) {
    if (s->is_large) {
        return s->str_data.lg.data;
    }
    return s->str_data.sm.data;
}

int vstr_cmp(const vstr* a, const vstr* b) {
    size_t alen = vstr_len(a);
    size_t blen = vstr_len(b);
    const char* astr = vstr_data(a);
    const char* bstr = vstr_data(b);
    if (alen == blen) {
        return strncmp(astr, bstr, alen);
    }
    if (alen < blen) {
        int cmp = strncmp(astr, bstr, alen);
        if (cmp == 0) {
            return 1;
        }
        return cmp;
    } else {
        int cmp = strncmp(astr, bstr, blen);
        if (cmp == 0) {
            return -1;
        }
        return cmp;
    }
}

int vstr_push_char(vstr* s, char c) {
    int push_res;
    if (s->is_large) {
        return vstr_lg_push_char(&(s->str_data.lg), c);
    }
    push_res = vstr_sm_push_char(&(s->str_data.sm), c, s->small_avail);
    if (push_res == 0) {
        s->small_avail--;
        return 0;
    }
    s->str_data.lg = vstr_make_lg(s->str_data.sm.data);
    if (s->str_data.lg.cap == 0) {
        return -1;
    }
    s->is_large = 1;
    return vstr_lg_push_char(&(s->str_data.lg), c);
}

int vstr_push_string(vstr* s, const char* str) {
    int push_res;
    size_t old_len, str_len = strlen(str);
    if (s->is_large) {
        return vstr_lg_push_string(&(s->str_data.lg), str, str_len);
    }
    push_res =
        vstr_sm_push_string(&(s->str_data.sm), str, str_len, s->small_avail);
    if (push_res == 0) {
        s->small_avail -= str_len;
        return 0;
    }
    old_len = vstr_len(s);
    s->str_data.lg = vstr_make_lg_len(s->str_data.sm.data, old_len);
    s->is_large = 1;
    return vstr_lg_push_string(&(s->str_data.lg), str, str_len);
}

int vstr_push_string_len(vstr* s, const char* str, size_t str_len) {
    int push_res;
    size_t old_len;
    if (s->is_large) {
        return vstr_lg_push_string(&(s->str_data.lg), str, str_len);
    }
    push_res =
        vstr_sm_push_string(&(s->str_data.sm), str, str_len, s->small_avail);
    if (push_res == 0) {
        s->small_avail -= str_len;
        return 0;
    }
    old_len = vstr_len(s);
    s->str_data.lg = vstr_make_lg_len(s->str_data.sm.data, old_len);
    s->is_large = 1;
    return vstr_lg_push_string(&(s->str_data.lg), str, str_len);
}

void vstr_reset(vstr* s) {
    if (s->is_large) {
        free(s->str_data.lg.data);
        s->is_large = 0;
    } else {
        memset(s->str_data.sm.data, 0, VSTR_MAX_SMALL_SIZE);
    }
    s->small_avail = VSTR_MAX_SMALL_SIZE;
    return;
}

void vstr_free(vstr* s) {
    if (s->is_large) {
        free(s->str_data.lg.data);
    }
    s->is_large = 0;
    s->small_avail = VSTR_MAX_SMALL_SIZE;
}

static vstr_lg vstr_make_lg(const char* data) {
    vstr_lg lg = {0};
    lg.len = VSTR_MAX_SMALL_SIZE;
    lg.cap = VSTR_MAX_SMALL_SIZE + 2;
    lg.data = calloc(lg.cap, sizeof(char));
    if (lg.data == NULL) {
        lg.len = 0;
        lg.cap = 0;
        return lg;
    }
    memcpy(lg.data, data, VSTR_MAX_SMALL_SIZE);
    return lg;
}

static vstr_lg vstr_lg_new_len(size_t len) {
    vstr_lg lg = {0};
    lg.len = 0;
    lg.cap = len + 1;
    lg.data = calloc(len + 1, sizeof(char));
    assert(lg.data != NULL);
    return lg;
}

static vstr_lg vstr_make_lg_len(const char* data, size_t len) {
    vstr_lg lg = {0};
    if (len >= VSTR_MAX_LARGE_SIZE) {
        return lg;
    }
    lg.len = len;
    lg.cap = len + 1;
    lg.data = calloc(lg.cap, sizeof(char));
    if (lg.data == NULL) {
        lg.len = 0;
        lg.cap = 0;
        return lg;
    }
    memcpy(lg.data, data, len);
    return lg;
}

static int vstr_sm_push_char(vstr_sm* sm, char c, uint8_t avail) {
    uint8_t ins;
    if (avail <= 0) {
        return -1;
    }
    ins = VSTR_MAX_SMALL_SIZE - avail;
    sm->data[ins] = c;
    return 0;
}

static int vstr_sm_push_string(vstr_sm* sm, const char* str, size_t str_len,
                               uint8_t avail) {
    uint8_t ins;
    if (str_len > avail) {
        return -1;
    }
    ins = VSTR_MAX_SMALL_SIZE - avail;
    memcpy(sm->data + ins, str, str_len);
    return 0;
}

static int vstr_lg_push_char(vstr_lg* lg, char c) {
    size_t len = lg->len, cap = lg->cap;
    if (cap == 0) {
        return -1;
    }
    if (len == (cap - 1)) {
        int realloc_res = vstr_realloc_lg(lg, len, cap);
        if (realloc_res == -1) {
            return -1;
        }
    }
    if ((len + 1) > VSTR_MAX_LARGE_SIZE) {
        return -1;
    }
    lg->data[len] = c;
    lg->len++;
    return 0;
}

static int vstr_lg_push_string(vstr_lg* lg, const char* str, size_t str_len) {
    size_t len = lg->len, cap = lg->cap;
    if (cap == 0) {
        return -1;
    }
    if ((len + str_len) > VSTR_MAX_LARGE_SIZE) {
        return -1;
    }
    if ((len + str_len) > (cap - 1)) {
        int realloc_res = vstr_realloc_lg_len(lg, len, cap, str_len);
        if (realloc_res == -1) {
            return -1;
        }
    }
    memcpy(lg->data + len, str, str_len);
    lg->len += str_len;
    return 0;
}

static int vstr_realloc_lg(vstr_lg* lg, size_t len, size_t cap) {
    void* tmp;
    cap <<= 1;
    tmp = realloc(lg->data, cap);
    if (tmp == NULL) {
        return -1;
    }
    lg->data = tmp;
    memset(lg->data + len, 0, cap - len);
    lg->cap = cap;
    return 0;
}

static int vstr_realloc_lg_len(vstr_lg* lg, size_t len, size_t cap,
                               size_t new_len) {
    void* tmp;
    cap += new_len;
    tmp = realloc(lg->data, cap);
    if (tmp == NULL) {
        return -1;
    }
    lg->data = tmp;
    memset(lg->data + len, 0, cap - len);
    lg->cap = cap;
    return 0;
}
