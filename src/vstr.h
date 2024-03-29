#ifndef __VSTR_H__

#define __VSTR_H__

#define VSTR_MAX_SMALL_SIZE 23
#define VSTR_MAX_LARGE_SIZE ((((uint64_t)(1)) << 56) - 1)

#include <stddef.h>
#include <stdint.h>

/**
 * @brief an optimized small string representation
 *
 * This struct is used in vstr to represent small strings on the stack rather
 * than on the heap.
 */
typedef struct {
    char data[VSTR_MAX_SMALL_SIZE]; /* the string itself with max len of 23 */
} vstr_sm;

/**
 * @brief a string representation for large strings
 *
 * This struct is used in vstr to represent larger strings whose data is
 * stored on the heap. The length field is truncated to 56 bits in order to
 * fit 23 bytes so that the small strings data can be stored in the vstr
 * struct
 */
typedef struct __attribute__((__packed__)) {
    char* data;      /* the string */
    size_t cap;      /* the capacity of the data buffer - how much is allocated,
                        including null term */
    size_t len : 56; /* the length of the string - how much of the buffer is
                        used */
} vstr_lg;

/**
 * @brief a string representation that is optimized for small strings
 *
 * The main data of this struct is a union of the vstr_sm struct and
 * the vstr_lg struct. Both structs are 23 bytes. One more byte is used to
 * determine if the string contained in the structure is a small string or a
 * large string.
 *
 * When the string is small, it's length must be less than or equal to
 * VSTR_MAX_SMALL_SIZE, which is 23. Note that the null terminator will not be
 * stored in the buffer in the vstr_sm struct when the buffer is full - it will
 * be represented in memory in the last byte of this struct. This is possible
 * because the is_large field will be 0 when it is a small string, and
 * small_avail will be 0 when the buffer is full. Because the is_large and
 * small_avail fields make up one total byte, this will act as the null
 * terminator for the string. This allows us to store up to 23 bytes in a small
 * string. Any string that is larger will be allocated on the heap and stored in
 * the vstr_lg struct.
 */
typedef struct {
    union {
        vstr_sm sm;
        vstr_lg lg;
    } str_data; /* union of the struct that contains the data */
    uint8_t
        is_large : 1; /* determine if we are a small (0) or large (1) string */
    uint8_t small_avail : 7; /* the amount of space available when the string is
                             a small string */
} vstr;

/**
 * @breif create a new vstr. Default initial type is a small string
 * @returns a vstr struct
 */
vstr vstr_new(void);
/**
 * @brief create a new vstr with capacity len + 1
 * @param len the length to reserve
 * @returns a vstr
 */
vstr vstr_new_len(size_t len);
/**
 * @brief create a new vstr from a traditional C string.
 * @param cstr the C style string. Must be null terminated (strlen is called to
 * get the length)
 * @returns a vstr struct representing cstr
 */
vstr vstr_from(const char* cstr);
/**
 * @brief create a new vstr from a traditional C string of length len
 * @param cstr the C style string
 * @param len the length of the C str
 * @returns a vstr struct representing cstr
 */
vstr vstr_from_len(const char* cstr, size_t len);
/**
 * @brief create a vstr from a formatted string
 * @param fmt the format
 * @param ... the argument list used in format
 * @returns vstr
 */
vstr vstr_format(const char* fmt, ...);
/**
 * @brief get the length of a vstr
 * @param s pointer to the vstr
 * @returns length of the vstr
 */
size_t vstr_len(const vstr* s);
/**
 * @brief return the buffer contained in a vstr
 * @param s pointer to the vstr
 * @returns the buffer contained in the vstr
 */
const char* vstr_data(const vstr* s);
/**
 * @brief compare two vstr's
 * @param a vstr to compare
 * @param b vstr to compare
 * @returns 0 if they are equal, non zero if they are not
 */
int vstr_cmp(const vstr* a, const vstr* b);
/**
 * @brief append a char to a vstr
 * @param s the vstr to append to
 * @param c the char to append
 * @returns 0 on succes, -1 on failure
 */
int vstr_push_char(vstr* s, char c);
/**
 * @brief append a string to a vstr
 * @param s the vstr to append to
 * @param str the string to append
 * @returns 0 on success, -1 on failure
 */
int vstr_push_string(vstr* s, const char* str);
/**
 * @brief append a string of str_len to a vstr
 * @param s the vstr to append to
 * @param str the string to append
 * @param str_len the length of the string to append
 * @returns 0 on success, -1 on failure
 */
int vstr_push_string_len(vstr* s, const char* str, size_t str_len);
/**
 * @brief reset the vstr
 * @param s the vstr to reset
 */
void vstr_reset(vstr* s);
/**
 * @brief free a vstr
 * @param s the vstr to free
 */
void vstr_free(vstr* s);

#endif /* __VSTR_H__ */
