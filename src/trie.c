#include "trie.h"
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define TRIE_NODE_INTIAL_CAP 2

#define TRIE_NODE_MAX_CAP 255

#if 1
#include <stdio.h>
#define trie_dbg(...)                                                          \
    do {                                                                       \
        printf("%s:%d ", __FILE__, __LINE__);                                  \
        printf(__VA_ARGS__);                                                   \
        fflush(stdout);                                                        \
    } while (0)
#else
#define trie_dbg(...)
#endif

typedef struct trie_node {
    char ch;
    uint8_t is_end;
    uint8_t len;
    uint8_t cap;
    uint32_t score;
    struct trie_node* children[];
} trie_node;

struct trie {
    uint64_t numel;
    uint64_t numnodes;
    trie_node* root;
};

static trie_node* trie_find(const trie* trie, const char* str, size_t str_len);
static int trie_node_push_node(trie_node** par, const trie_node* new_node);
static int trie_insert_new(trie* trie, trie_node* par, const char* str,
                           size_t str_len, uint32_t score);
static trie_node* trie_node_new(void);
static trie_node* find_child(trie_node* par, char ch);
static void trie_free_walk(trie_node* node);

trie* trie_new(void) {
    trie* res = calloc(1, sizeof *res);
    if (res == NULL) {
        return NULL;
    }
    res->numel = 0;
    res->root = trie_node_new();
    if (res->root == NULL) {
        free(res);
        return NULL;
    }
    return res;
}

uint64_t trie_numel(const trie* t) { return t->numel; }

int trie_insert(trie* trie, const char* str, size_t str_len) {
    size_t i;
    trie_node* cur = trie->root;
    int res;
    for (i = 0; i < str_len; ++i) {
        char ch = str[i];
        trie_node* child;
        child = find_child(cur, ch);
        if (child != NULL) {
            cur = child;
            continue;
        }
        res = trie_insert_new(trie, cur, str + i, str_len - i, 0);
        if (res == TRIE_OK) {
            trie->numel++;
        }
        return res;
    }
    return TRIE_KEY_ALREADY_EXISTS;
}

int trie_insert_with_score(trie* trie, const char* str, size_t str_len,
                           uint32_t score) {
    size_t i;
    trie_node* cur = trie->root;
    int res;
    for (i = 0; i < str_len; ++i) {
        char ch = str[i];
        trie_node* child;
        child = find_child(cur, ch);
        if (child != NULL) {
            cur = child;
            continue;
        }
        res = trie_insert_new(trie, cur, str + i, str_len - i, score);
        if (res == TRIE_OK) {
            trie->numel++;
        }
        return res;
    }
    return TRIE_KEY_ALREADY_EXISTS;
}

int trie_search(const trie* trie, const char* str, size_t str_len) {
    trie_node* node = trie_find(trie, str, str_len);
    if (node == NULL) {
        return TRIE_NO_KEY;
    }
    return TRIE_OK;
}

int trie_set_score(trie* trie, const char* str, size_t str_len,
                   uint32_t score) {
    trie_node* node = trie_find(trie, str, str_len);
    if (node == NULL) {
        return TRIE_NO_KEY;
    }
    node->score = score;
    return TRIE_OK;
}

int trie_incr_score(trie* trie, const char* str, size_t str_len) {
    trie_node* node = trie_find(trie, str, str_len);
    if (node == NULL) {
        return TRIE_NO_KEY;
    }
    node->score++;
    return TRIE_OK;
}

int trie_incr_by_score(trie* trie, const char* str, size_t str_len,
                       uint32_t incr_by) {
    trie_node* node = trie_find(trie, str, str_len);
    if (node == NULL) {
        return TRIE_NO_KEY;
    }
    node->score += incr_by;
    return TRIE_OK;
}

uint32_t trie_get_score(trie* trie, const char* str, size_t str_len, int* res) {
    trie_node* node = trie_find(trie, str, str_len);
    if (node == NULL) {
        *res = TRIE_NO_KEY;
        return 0;
    }
    *res = TRIE_OK;
    return node->score;
}

static trie_node* trie_find(const trie* trie, const char* str, size_t str_len) {
    size_t i;
    trie_node* cur = trie->root;
    for (i = 0; i < str_len; ++i) {
        char ch = str[i];
        trie_node* child = find_child(cur, ch);
        if (child == NULL) {
            return NULL;
        }
        cur = child;
    }
    if (!cur->is_end) {
        return NULL;
    }
    return cur;
}

void trie_free(trie* trie) {
    trie_free_walk(trie->root);
    free(trie);
}

static void trie_free_walk(trie_node* node) {
    uint8_t i, len = node->len;
    for (i = 0; i < len; ++i) {
        trie_node* next = node->children[i];
        trie_free_walk(next);
    }
    free(node);
}

static int trie_insert_new(trie* trie, trie_node* par, const char* str,
                           size_t str_len, uint32_t score) {
    size_t i;
    trie_node* cur = par;
    for (i = 0; i < str_len; ++i) {
        char ch = str[i];
        trie_node* new_node = trie_node_new();
        if (new_node == NULL) {
            return TRIE_OOM;
        }
        new_node->ch = ch;
        if (trie_node_push_node(&cur, new_node) != TRIE_OK) {
            return TRIE_OOM;
        }
        trie->numnodes++;
        cur = new_node;
    }
    cur->is_end = 1;
    cur->score = score;
    return TRIE_OK;
}

static int trie_node_push_node(trie_node** par, const trie_node* new_node) {
    size_t len = (*par)->len, cap = (*par)->cap;
    if (len == cap) {
        void* tmp;
        cap <<= 1;
        if (cap > TRIE_NODE_MAX_CAP) {
            return TRIE_OOM;
        }
        tmp = realloc(*par, (sizeof **par) + (cap * sizeof(trie_node*)));
        if (tmp == NULL) {
            return TRIE_OOM;
        }
        *par = tmp;
        (*par)->cap = cap;
    }
    memcpy(&(*par)->children[len], &new_node, sizeof(trie_node*));
    (*par)->len++;
    return TRIE_OK;
}

static trie_node* find_child(trie_node* par, char ch) {
    size_t i, len = par->len;
    for (i = 0; i < len; ++i) {
        trie_node* child = par->children[i];
        if (child->ch == ch) {
            return child;
        }
    }
    return NULL;
}

static trie_node* trie_node_new(void) {
    trie_node* node;
    size_t needed =
        (sizeof *node) + (TRIE_NODE_INTIAL_CAP * sizeof(trie_node*));
    node = malloc(needed);
    if (node == NULL) {
        return NULL;
    }
    memset(node, 0, needed);
    node->cap = TRIE_NODE_INTIAL_CAP;
    return node;
}
