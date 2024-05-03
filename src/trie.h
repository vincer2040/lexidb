#ifndef __TRIE_H__

#define __TRIE_H__

#include <stddef.h>
#include <stdint.h>

#define TRIE_OK 0
#define TRIE_OOM 1
#define TRIE_NO_KEY 2
#define TRIE_KEY_ALREADY_EXISTS 3

typedef struct trie trie;

trie* trie_new(void);

uint64_t trie_numel(const trie* t);

int trie_insert(trie* t, const char* str, size_t str_len);
int trie_search(const trie* trie, const char* str, size_t str_len);

int trie_set_score(trie* trie, const char* str, size_t str_len, uint32_t score);
uint32_t trie_get_score(trie* trie, const char* str, size_t str_len, int* res);
int trie_incr_score(trie* trie, const char* str, size_t str_len);
int trie_incr_by_score(trie* trie, const char* str, size_t str_len,
                       uint32_t incr_by);

void trie_free(trie* trie);

#endif /* __TRIE_H__ */
