#ifndef __VEC_H__

#define __VEC_H__

#include <stddef.h>
#include <sys/types.h>
#include "util.h"

/**
 * @brief a vector representation
 *
 * This structure represents a dynamically sized array. Data added to the
 * vector is stored in the same memory allocation as seen by the use
 * of the flexible array member
 *
 * Available operations:
 *      - push (vec_push)
 *      - pop (vec_pop)
 *      - get at (vec_get_at)
 *      - remove at (vec_remove_at)
 *      - linear find (vec_find)
 *      - binary search (vec_binary_search)
 *      - bubble sort (vec_bubble_sort)
 *      - insertion sort (vec_insertion_sort)
 *      - quick sort (vec_quick_sort)
 */
typedef struct {
    size_t len; /* the number of items in the vector. Also used as an insertion
                   point */
    size_t cap; /* the number of slots available in the vector. cap * data_size
                   = the amount allocated for data */
    size_t data_size;     /* the size of a single element in the vector */
    unsigned char data[]; /* FAM of the data stored in the vector */
} vec;


/**
 * @brief allocate a new vector
 * @param data_size the size of a single element in the vector
 * @returns a newly allocated vector
 */
vec* vec_new(size_t data_size);
/**
 * @brief allocate a vector with a specified initial capacity rather than the
 * default
 * @param data_size the size of a single element in the vector
 * @param capacity the initial capacity of the vector
 * @returns allocate vector with capacity
 */
vec* vec_new_with_capacity(size_t data_size, size_t capacity);
/**
 * @brief append an element to the vector
 * @param vec the vector to append to
 * @param data the data to append
 * @returns 0 on success, -1 on failure
 */
int vec_push(vec** vec, void* data);
/**
 * @brief remove the last element from the vector
 * @param vec the vector to remove from
 * @param out where the element is copied to
 * @returns 0 on success, -1 on failure
 */
int vec_pop(vec* vec, void* out);
/**
 * @brief get the element at a specific index in the vector
 * @param vec the vector to retrieve the data from
 * @param idx the index of the data to retrieve
 * @returns pointer to the data on success, NULL on failure
 */
void* vec_get_at(vec* vec, size_t idx);
/**
 * @brief remove an element from the vector at a specific index
 * @param vec the vector to remove from
 * @param idx the index of the element to remove
 * @param out where the removed data is copied to
 * @returns 0 on success, -1 on failure
 */
int vec_remove_at(vec* vec, size_t idx, void* out);
/**
 * @brief find an element using linear search
 * @param vec the vector to find the data in
 * @param cmp_data what to compare the elements against. passed as first
 * argument to CmpFn
 * @param out where to copy the found data
 * @param fn the comparison function to determine if elements are equal
 * @returns index of found element on success, -1 on failure
 */
ssize_t vec_find(vec* vec, void* cmp_data, void* out, cmp_fn* fn);
/**
 * @brief free the whole vector
 * @param vec the vector to free
 * @param fn free callback function to free each element if necessary. If null,
 * it is ignored
 */
void vec_free(vec* vec, free_fn* fn);
/**
 * @brief compute binary search on vector for an item
 * @param vec the vector to search in
 * @param needle the data to search for. passed as first argument to CmpFn
 * @param fn the comparison function to determine if elements are equal
 * @returns 0 on found, -1 on not found
 */
int vec_binary_search(vec* vec, void* needle, cmp_fn* fn);

/**
 * @brief iterator over a vector
 *
 * use iter.cur to get the current element of iteration. If it is null,
 * iteration is complete
 */
typedef struct {
    void* cur;  /* the current element of iteration. NULL if iterator is done */
    void* next; /* the next element of iteration */
    size_t next_idx; /* the index of the next element of iteration */
    size_t end_idx;  /* the last index of iteration */
    vec* vec;        /* the vector being iterated over */
} vec_iter;

/**
 * @bried create a new iterator
 * @param vec the vector to iterate over
 * @returns vec_iter
 */
vec_iter vec_iter_new(vec* vec);
/**
 * @brief get the next element of iteration
 * @param iter the iterator to iterate
 */
void vec_iter_next(vec_iter* iter);

#endif /* __VEC_H__ */
