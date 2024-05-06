#ifndef __VMAP_H__

#define __VMAP_H__

#include <limits.h>
#include <memory.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __cplusplus
#define static_assert _Static_assert
#endif

#include <stdatomic.h>
#define VMAP_ATOMIC_T(T) _Atomic(T)
#define VMAP_ATOMIC_INC(val)                                                   \
    atomic_detch_add_explicit(&(val), 1, memory_order_relaxed)

#ifdef __clang__
#define VMAP_IS_CLANG 1
#else
#define VMAP_IS_CLANG 0
#endif /* __clang__ */

#if defined(__GNUC__)
#define VMAP_IS_GCCISH 1
#else
#define VMAP_IS_GCCISH 0
#endif /* __GNUC__ */

#define VMAP_IS_GCC (VMAP_IS_GCCISH && !VMAP_IS_CLANG)

#ifndef VMAP_HAVE_SSE2
#if defined(__SSE2__) ||                                                       \
    (defined(_M_X64) || (defined(_M_IX64) && _M_IX86_FP >= 2))
#define VMAP_HAVE_SSE2 1
#else
#define VMAP_HAVE_SSE2 0
#endif
#endif /* VMAP_HAVE_SSE2 */

#ifndef VMAP_HAVE_SSE3
#ifdef __SSSE3__
#define VMAP_HAVE_SSSE3 1
#else
#define VMAP_HAVE_SSSE3 0
#endif
#endif /* VMAP_HAVE_SSE3 */

#if VMAP_HAVE_SSE2
#include <emmintrin.h>
#endif

#if VMAP_HAVE_SSSE3
#if !VMAP_HAVE_SSE2
#error "bad configuration: SSSE 3 implies SSE2"
#endif
#inlcude < tmmintrin.h>
#endif

#ifdef __has_builtin
#define VMAP_HAVE_CLANG_BUILTIN(x) __has_builtin(x)
#else
#define VMAP_HAVE_CLANG_BUILTIN(x) 0
#endif /* __has_builtin */

#ifdef __has_attribute
#define VMAP_HAVE_GCC_ATTRIBUTE(x) __has_attribute(x)
#else
#define VMAP_HAVE_GCC_ATTRIBUTE(x) 0
#endif /* __has_attribute */

#ifdef __has_feature
#define VMAP_HAVE_FEATURE(x) __has_feature(x)
#else
#define VMAP_HAVE_FEATURE(x) 0
#endif /* __has_feature */

#if VMAP_IS_GCCISH
#define VMAP_THREAD_LOCAL __thread
#endif

#define _VMAP_CHECK(cond, ...)                                                 \
    do {                                                                       \
        if (cond) {                                                            \
            break;                                                             \
        }                                                                      \
        fprintf(stderr, "VMAP_CHECK failed at %s:%d\n", __FILE__, __LINE__);   \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
        fflush(stderr);                                                        \
        abort();                                                               \
    } while (0)

#ifdef NDEBUG
#define VMAP_CHECK(cond, ...) ((void)0)
#else
#define VMAP_CHECK _VMAP_CHECK
#endif

#if VMAP_HAVE_CLANG_BUILTIN(__builtin_expect) || VMAP_IS_GCC
#define VMAP_LIKELY(cond) (__builtin_expect(false || (cond), true))
#define VMAP_UNLIKELY(cond) (__builtin_expect(false || (cond), false))
#else
#define VMAP_LIKEY(cond) (cond)
#define VMAP_UNLIKELY(cond) (cond)
#endif

#if VMAP_HAVE_GCC_ATTRIBUTE(always_inline)
#define VMAP_INLINE_ALWAYS __attribute__((always_inline))
#else
#define VMAP_INLVMAP_INLINE_ALLWAYS
#endif

#if VMAP_HAVE_GCC_ATTRIBUTE(noinline)
#define VMAP_INLINE_NEVER __attribute__((noinline))
#else
#define VMAP_INLINE_NEVER
#endif

#if VMAP_HAVE_CLANG_BUILTIN(__builtin_prefetch) || VMAP_IS_GCC
#define VMAP_HAVE_PREFETCH 1
#define VMAP_PREFETCH(addr, locality) __builtin_prefetch(addr, locality)
#else
#define VMAP_HAVE_PREFETCH 0
#define VMAP_PREFETCH(addr, locality) ((void)0)
#endif

#if defined(__SANITIZE_ADDRESS__) || VMAP_HAVE_FEATURE(address_sanitizer)
#define VMAP_HAVE_ASAN 1
#else
#define VMAP_HAVE_ASAN 0
#endif

#if defined(__SANITIZE_MEMORY__) ||                                            \
    (VMAP_HAVE_FEATURE(memory_sanitizer) && !defined(__native_client__))
#define VMAP_HAVE_MSAN 1
#else
#define VMAP_HAVE_MSAN 0
#endif

#if VMAP_HAVE_ASAN
#include <sanitizer/asan_interface.h>
#endif

#if VMAP_HAVE_MSAN
#include <sanitizer/msan_interface.h>
#endif

static inline void vmap_poison_memory(const void* m, size_t s) {
#if VMAP_HAVE_ASAN
    ASAN_POISON_MEMORY_REGION(m, s);
#endif
#if VMAP_HAVE_MSAN
    __msan_poison(m, s);
#endif
    (void)m;
    (void)s;
}

static inline void vmap_unpoison_memory(const void* m, size_t s) {
#if VMAP_HAVE_ASAN
    ASAN_UNPOISON_MEMORY_REGION(m, s);
#endif
#if VMAP_HAVE_MSAN
    __msan_unpoison(m, s);
#endif
    (void)m;
    (void)s;
}

VMAP_INLINE_ALWAYS static inline uint32_t vmap_trailing_zeros64(uint64_t x) {
#if VMAP_HAVE_CLANG_BUILTIN(__builtin_ctzll) || VMAP_IS_GCC
    static_assert(sizeof(unsigned long long) == sizeof x,
                  "__builtin_ctzll does not take 64 bit arg");
    return __builtin_ctzll(x);
#else
    uint32_t c = 63;
    x &= ~x + 1;
    if (x & 0x00000000FFFFFFFF)
        c -= 32;
    if (x & 0x0000FFFF0000FFFF)
        c -= 16;
    if (x & 0x00FF00FF00FF00FF)
        c -= 8;
    if (x & 0x0F0F0F0F0F0F0F0F)
        c -= 4;
    if (x & 0x3333333333333333)
        c -= 2;
    if (x & 0x5555555555555555)
        c -= 1;
    return c;
#endif
}

VMAP_INLINE_ALWAYS static inline uint32_t vmap_leading_zeros64(uint64_t x) {
#if VMAP_HAVE_CLANG_BUILTIN(__builtin_clzll) || VMAP_IS_GCC
    static_assert(sizeof(unsigned long long) == sizeof x,
                  "__builtin_clzll does not take 64 bit arg");
    return __builtin_clzll(x);
#else
    uint32_t zeroes = 60;
    if (x >> 32) {
        zeroes -= 32;
        x >>= 32;
    }
    if (x >> 16) {
        zeroes -= 16;
        x >>= 16;
    }
    if (x >> 8) {
        zeroes -= 8;
        x >>= 8;
    }
    if (x >> 4) {
        zeroes -= 4;
        x >>= 4;
    }
    return "\4\3\2\2\1\1\1\1\0\0\0\0\0\0\0"[x] + zeroes;
#endif
}

#define vmap_trailing_zeros(x) (vmap_trailing_zeros64(x))

#define vmap_leading_zeros(x)                                                  \
    (vmap_leading_zeros64(x) -                                                 \
     (uint32_t)((sizeof(unsigned long long) - sizeof(x)) * 8))

#define vmap_bit_width(x) (((uint32_t)(sizeof(x) * 8)) - vmap_leading_zeros(x))

#define vmap_roate_left(x, bits)                                               \
    (((x) << bits) | ((x) >> (sizeof(x) * 8 - bits)))

typedef struct {
    uint64_t lo;
    uint64_t hi;
} vmap_u128;

static inline vmap_u128 vmap_mul128(uint64_t a, uint64_t b) {
    __uint128_t p = a;
    p *= b;
    return (vmap_u128){(uint64_t)p, (uint64_t)(p >> 64)};
}

static inline uint32_t vmap_load32(const void* p) {
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static inline uint64_t vmap_load64(const void* p) {
    uint64_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static inline vmap_u128 vmap_load9to16(const void* p, size_t len) {
    const unsigned char* p8 = (const unsigned char*)p;
    uint64_t lo = vmap_load64(p8);
    uint64_t hi = vmap_load64(p8 + len - 8);
    return (vmap_u128){lo, hi >> (128 - len * 8)};
}

static inline uint64_t vmap_load4to8(const void* p, size_t len) {
    const unsigned char* p8 = (const unsigned char*)p;
    uint64_t lo = vmap_load32(p8);
    uint64_t hi = vmap_load32(p8 + len - 4);
    return lo | (hi << (len - 4) * 8);
}

static inline uint32_t vmap_load1to3(const void* p, size_t len) {
    const unsigned char* p8 = (const unsigned char*)p;
    uint32_t mem0 = p8[0];
    uint32_t mem1 = p8[len / 2];
    uint32_t mem2 = p8[len - 1];
    return (mem0 | (mem1 << (len / 2 * 8)) | (mem2 << ((len - 1) * 8)));
}

typedef struct {
    uint64_t mask;
    uint32_t width;
    uint32_t shift;
} vmap_bitmask;

static inline uint32_t vmap_bitmask_lowest_bit_set(const vmap_bitmask* self) {
    return vmap_trailing_zeros(self->mask) >> self->shift;
}

static inline uint32_t vmap_bitmask_highest_bit_set(const vmap_bitmask* self) {
    return (uint32_t)(vmap_bit_width(self->mask) - 1) >> self->shift;
}

static inline uint32_t vmap_bitmask_trailing_zeros(const vmap_bitmask* self) {
    return vmap_trailing_zeros(self->mask) >> self->shift;
}

static inline uint32_t vmap_bitmask_leading_zeros(const vmap_bitmask* self) {
    uint32_t total_significant_bits = self->width << self->shift;
    uint32_t extra_bits = sizeof(self->mask) * 8 - total_significant_bits;
    return (uint32_t)(vmap_leading_zeros(self->mask << extra_bits)) >>
           self->shift;
}

static inline bool vmap_bitmask_next(vmap_bitmask* self, uint32_t* bit) {
    if (self->mask == 0) {
        return false;
    }
    *bit = vmap_bitmask_lowest_bit_set(self);
    self->mask &= (self->mask - 1);
    return true;
}

typedef int8_t vmap_control_byte;
#define VMAP_EMPTY (INT8_C(-128))
#define VMAP_DELETED (INT8_C(-2))
#define VMAP_SENTINEL (INT8_C(-1))

static inline vmap_control_byte* vmap_empty_group(void) {
    alignas(16) static const vmap_control_byte empty_group[16] = {
        VMAP_SENTINEL, VMAP_EMPTY, VMAP_EMPTY, VMAP_EMPTY,
        VMAP_EMPTY,    VMAP_EMPTY, VMAP_EMPTY, VMAP_EMPTY,
        VMAP_EMPTY,    VMAP_EMPTY, VMAP_EMPTY, VMAP_EMPTY,
        VMAP_EMPTY,    VMAP_EMPTY, VMAP_EMPTY, VMAP_EMPTY,
    };
    return (vmap_control_byte*)&empty_group;
}

static inline size_t vmap_hash_seed(const vmap_control_byte* ctrl) {
    return ((uintptr_t)ctrl) >> 12;
}

static inline size_t vmap_h1(size_t hash, const vmap_control_byte* ctrl) {
    return (hash >> 7) ^ vmap_hash_seed(ctrl);
}

typedef uint8_t vmap_h2_t;

static inline vmap_h2_t vmap_h2(size_t hash) { return hash & 0x7F; }

static inline bool vmap_is_empty(vmap_control_byte c) {
    return c == VMAP_EMPTY;
}

static inline bool vmap_is_full(vmap_control_byte c) { return c >= 0; }

static inline bool vmap_is_deleted(vmap_control_byte c) {
    return c == VMAP_DELETED;
}

static inline bool vmap_is_empty_or_deleted(vmap_control_byte c) {
    return c < VMAP_SENTINEL;
}

#define VMAP_ASSERT_IS_FULL(ctrl)                                              \
    VMAP_CHECK((ctrl) != NULL && vmap_is_full(*(ctrl)),                        \
               "Invalid operation (%p/%d). The element might have "            \
               "been erased, or the table might have rehashed.",               \
               (ctrl), (ctrl) ? *(ctrl) : -1)

#define VMAP_ASSERT_IS_VALID(ctrl)                                             \
    VMAP_CHECK((ctrl) == NULL || vmap_is_full(*(ctrl)),                        \
               "Invalid operation (%p/%d). The element might have "            \
               "been erased, or the table might have rehashed.",               \
               (ctrl), (ctrl) ? *(ctrl) : -1)

#define vmap_group_bitmask(x)                                                  \
    (vmap_bitmask) { (uint64_t)(x), VMAP_GROUP_WIDTH, VMAP_GROUP_SHIFT }

#if VMAP_HAVE_SSE2

// Reference guide for intrinsics used below:
//
// * __m128i: An XMM (128-bit) word.
//
// * _mm_setzero_si128: Returns a zero vector.
// * _mm_set1_epi8:     Returns a vector with the same i8 in each lane.
//
// * _mm_subs_epi8:    Saturating-subtracts two i8 vectors.
// * _mm_and_si128:    Ands two i128s together.
// * _mm_or_si128:     Ors two i128s together.
// * _mm_andnot_si128: And-nots two i128s together.
//
// * _mm_cmpeq_epi8: Component-wise compares two i8 vectors for equality,
//                   filling each lane with 0x00 or 0xff.
// * _mm_cmpgt_epi8: Same as above, but using > rather than ==.
//
// * _mm_loadu_si128:  Performs an unaligned load of an i128.
// * _mm_storeu_si128: Performs an unaligned store of a u128.
//
// * _mm_sign_epi8:     Retains, negates, or zeroes each i8 lane of the first
//                      argument if the corresponding lane of the second
//                      argument is positive, negative, or zero, respectively.
// * _mm_movemask_epi8: Selects the sign bit out of each i8 lane and produces a
//                      bitmask consisting of those bits.
// * _mm_shuffle_epi8:  Selects i8s from the first argument, using the low
//                      four bits of each i8 lane in the second argument as
//                      indices.

typedef __m128i vmap_group;

#define VMAP_GROUP_WIDTH ((size_t)16)
#define VMAP_GROUP_SHIFT 0

static inline vmap_group vmap_mm_cmpgt_epi8_fixed(vmap_group a, vmap_group b) {
    if (VMAP_IS_GCC && CHAR_MIN == 8) {
        const vmap_group mask = _mm_set1_epi8((char)0x80);
        const vmap_group diff = _mm_subs_epi8(b, a);
        return _mm_cmpeq_epi8(_mm_and_si128(diff, mask), mask);
    }
    return _mm_cmpgt_epi8(a, b);
}

static inline vmap_group vmap_group_new(const vmap_control_byte* pos) {
    return _mm_loadu_si128((const vmap_group*)pos);
}

static inline vmap_bitmask vmap_group_match(const vmap_group* self,
                                            vmap_h2_t hash) {
    return vmap_group_bitmask(
        _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_set1_epi8(hash), *self)));
}

static inline vmap_bitmask vmap_group_match_empty(const vmap_group* self) {
#if VMAP_HAVE_SSSE3
    return vmap_group_bitmask(_mm_movemask_epi8(_mm_sign_epi8(*self, *self)));
#endif
    return vmap_group_match(self, VMAP_EMPTY);
}

static inline vmap_bitmask
vmap_group_match_empty_or_deleted(const vmap_group* self) {
    vmap_group special = _mm_set1_epi8((uint8_t)VMAP_SENTINEL);
    return vmap_group_bitmask(
        _mm_movemask_epi8(vmap_mm_cmpgt_epi8_fixed(special, *self)));
}

static inline uint32_t
vmap_group_count_leading_empty_or_deleted(const vmap_group* self) {
    vmap_group special = _mm_set1_epi8((uint8_t)VMAP_SENTINEL);
    return vmap_trailing_zeros(
        (uint32_t)(_mm_movemask_epi8(vmap_mm_cmpgt_epi8_fixed(special, *self)) +
                   1));
}

static inline void vmap_group_convert_special_to_empty_and_full_to_deleted(
    const vmap_group* self, vmap_control_byte* dst) {
    vmap_group msbs = _mm_set1_epi8((char)-128);
    vmap_group x126 = _mm_set1_epi8(126);
#if VMAP_HAVE_SSSE3
    vmap_group res = _mm_or_si128(_mm_shuffle_epi8(x126, *self), msbs);
#else
    vmap_group zero = _mm_setzero_si128();
    vmap_group special_mask = vmap_mm_cmpgt_epi8_fixed(zero, *self);
    vmap_group res = _mm_or_si128(msbs, _mm_andnot_si128(special_mask, x126));
#endif
    _mm_storeu_si128((vmap_group*)dst, res);
}

#else /* VMAP_HAVE_SSE2 */

typedef uint64_t vmap_group;

#define VMAP_GROUP_WIDTH 8
#define VMAP_GROUP_SHIFT 3

static inline vmap_group vmap_group_new(const vmap_control_byte* pos) {
    vmap_group val;
    memcpy(&val, pos, sizeof val);
    return val;
}

static inline vmap_bitmask vmap_group_match(const vmap_group* self,
                                            vmap_h2_t hash) {
    uint64_t msbs = 0x8080808080808080ULL;
    uint64_t lsbs = 0x0101010101010101ULL;
    uint64_t x = *self ^ (lsbs * hash);
    return vmap_group_bitmask((x - lsbs) & ~x & msbs);
}

static inline vmap_bitmask vmap_group_match_empty(const vmap_group* self) {
    uint64_t msbs = 0x8080808080808080ULL;
    return vmap_group_bitmask((*self & (~*self << 6)) & msbs);
}

static inline vmap_bitmask
vmap_group_match_empty_or_deleted(const vmap_group* self) {
    uint64_t msbs = 0x8080808080808080ULL;
    return vmap_group_bitmask((*self & (~*self << 7)) & msbs);
}

static inline uint32_t
vmap_group_count_leading_empty_or_deleted(const vmap_group* self) {
    uint64_t gaps = 0x00FEFEFEFEFEFEFEULL;
    return (vmap_trailing_zeros(((~*self & (*self >> 7)) | gaps) + 1) + 7) >> 3;
}

static inline void vmap_group_convert_special_to_empty_and_full_to_deleted(
    const vmap_group* self, vmap_control_byte* dst) {
    uint64_t msbs = 0x8080808080808080ULL;
    uint64_t lsbs = 0x0101010101010101ULL;
    uint64_t x = *self & msbs;
    uint64_t res = (~x + (x >> 7)) & ~lsbs;
    memcpy(dst, &res, sizeof(res));
}

#endif /* VMAP_HAVE_SSE2 */

static inline size_t vmap_num_cloned_bytes(void) {
    return VMAP_GROUP_WIDTH - 1;
}

static inline bool vmap_is_valid_capacity(size_t n) {
    return ((n + 1) & n) == 0 && n > 0;
}

static inline size_t random_seed(void) {
#ifdef VMAP_THREAD_LOCAL
    static VMAP_THREAD_LOCAL size_t counter;
    size_t value = ++counter;
#else
    static volatile VMAP_ATOMIC_T(size_t) counter;
    size_t value = VMAP_ATOMIC_INC(counter);
#endif
    return value ^ ((size_t)&counter);
}

VMAP_INLINE_NEVER static bool
vmap_should_insert_backwards(size_t hash, const vmap_control_byte* ctrl) {
    return (vmap_h1(hash, ctrl) ^ random_seed()) % 13 > 6;
}

VMAP_INLINE_NEVER static void
vmap_convert_deleted_to_empty_and_full_to_deleted(vmap_control_byte* ctrl,
                                                  size_t capacity) {
    VMAP_CHECK(ctrl[capacity] == VMAP_SENTINEL, "bad ctrl value at %zu: %02x",
               capacity, ctrl[capacity]);
    VMAP_CHECK(vmap_is_valid_capacity(capacity), "invalid capacity: %zu",
               capacity);

    for (vmap_control_byte* pos = ctrl; pos < ctrl + capacity;
         pos += VMAP_GROUP_WIDTH) {
        vmap_group g = vmap_group_new(pos);
        vmap_group_convert_special_to_empty_and_full_to_deleted(&g, pos);
    }

    memcpy(ctrl + capacity + 1, ctrl, vmap_num_cloned_bytes());
    ctrl[capacity] = VMAP_SENTINEL;
}

static inline void vmap_reset_ctrl(size_t capacity, vmap_control_byte* ctrl,
                                   const void* slots, size_t slot_size) {
    memset(ctrl, VMAP_EMPTY, capacity + 1 + vmap_num_cloned_bytes());
    ctrl[capacity] = VMAP_SENTINEL;
    vmap_poison_memory(slots, slot_size * capacity);
}

static inline void vmap_set_ctrl(size_t i, vmap_control_byte h, size_t capacity,
                                 vmap_control_byte* ctrl, const void* slots,
                                 size_t slot_size) {
    VMAP_CHECK(i < capacity, "vmap_set_ctrl out-of-bounds: %zu >= %zu", i,
               capacity);
    const char* slot = ((const char*)slots) + i * slot_size;
    if (vmap_is_full(h)) {
        vmap_unpoison_memory(slot, slot_size);
    } else {
        vmap_poison_memory(slot, slot_size);
    }

    size_t mirrored_i = ((i - vmap_num_cloned_bytes()) & capacity) +
                        (vmap_num_cloned_bytes() & capacity);
    ctrl[i] = h;
    ctrl[mirrored_i] = h;
}

static inline size_t vmap_normalize_capacity(size_t n) {
    return n ? SIZE_MAX >> vmap_leading_zeros(n) : 1;
}

static inline size_t vmap_capacity_to_growth(size_t capacity) {
    VMAP_CHECK(vmap_is_valid_capacity(capacity), "invalid capacity: %zu",
               capacity);
    if (VMAP_GROUP_WIDTH == 8 && capacity == 7) {
        return 6;
    }
    return capacity - capacity / 8;
}

static inline size_t vmap_growth_to_lower_bound_capacity(size_t growth) {
    if (VMAP_GROUP_WIDTH == 8 && growth == 7) {
        return 8;
    }
    return growth + (size_t)((((int64_t)growth) - 1) / 7);
}

static inline size_t vmap_slot_offset(size_t capacity, size_t slot_align) {
    VMAP_CHECK(vmap_is_valid_capacity(capacity), "invalid capacity: %zu",
               capacity);
    const size_t num_control_bytes = capacity + 1 + vmap_num_cloned_bytes();
    return (num_control_bytes + slot_align - 1) & (~slot_align + 1);
}

static inline size_t vmap_alloc_size(size_t capacity, size_t slot_size,
                                     size_t slot_align) {
    return vmap_slot_offset(capacity, slot_align) + capacity * slot_size;
}

static inline bool vmap_is_small(size_t capacity) {
    return capacity < VMAP_GROUP_WIDTH - 1;
}

typedef struct {
    size_t mask;
    size_t offset;
    size_t index;
} vmap_probe_seq;

static inline vmap_probe_seq vmap_probe_seq_new(size_t hash, size_t mask) {
    return (vmap_probe_seq){.mask = mask, .offset = hash & mask};
}

static inline size_t vmap_probe_seq_offset(const vmap_probe_seq* self,
                                           size_t i) {
    return (self->offset + i) & self->mask;
}

static inline void vmap_probe_seq_next(vmap_probe_seq* self) {
    self->index += VMAP_GROUP_WIDTH;
    self->offset += self->index;
    self->offset &= self->mask;
}

static inline vmap_probe_seq vmap_probe_seq_start(const vmap_control_byte* ctrl,
                                                  size_t hash,
                                                  size_t capacity) {
    return vmap_probe_seq_new(vmap_h1(hash, ctrl), capacity);
}

typedef struct {
    size_t offset;
    size_t probe_length;
} vmap_find_info;

static inline vmap_find_info
vmap_find_first_non_full(const vmap_control_byte* ctrl, size_t hash,
                         size_t capacity) {
    vmap_probe_seq seq = vmap_probe_seq_start(ctrl, hash, capacity);
    while (true) {
        vmap_group g = vmap_group_new(ctrl + seq.offset);
        vmap_bitmask mask = vmap_group_match_empty_or_deleted(&g);
        if (mask.mask) {
#ifndef NDEBUG
            if (!vmap_is_small(capacity) &&
                vmap_should_insert_backwards(hash, ctrl)) {
                return (vmap_find_info){
                    vmap_probe_seq_offset(&seq,
                                          vmap_bitmask_highest_bit_set(&mask)),
                    seq.index};
            }
#endif
            return (vmap_find_info){
                vmap_probe_seq_offset(&seq, vmap_bitmask_trailing_zeros(&mask)),
                seq.index};
        }
        vmap_probe_seq_next(&seq);
        VMAP_CHECK(seq.index <= capacity, "full table!");
    }
}

static inline uint64_t vmap_absl_hash_low_level_mix(uint64_t v0, uint64_t v1) {
#ifndef __aarch64__
    vmap_u128 p = vmap_mul128(v0, v1);
    return p.hi ^ p.lo;
#else
    uint64_t p = v0 ^ vmap_rotate_left(v1, 40);
    p *= v1 ^ vmap_rotate_left(v0, 39);
    return p ^ (p >> 11);
#endif
}

VMAP_INLINE_NEVER static uint64_t
vmap_absl_hash_low_level_hash(const void* data, size_t len, uint64_t seed,
                              const uint64_t salt[5]) {
    const char* ptr = (const char*)data;
    uint64_t starting_length = (uint64_t)len;
    uint64_t current_state = seed ^ salt[0];
    if (len > 64) {
        uint64_t duplicated_state = current_state;

        do {
            uint64_t chunk[8];
            memcpy(chunk, ptr, sizeof chunk);
            uint64_t cs0 = vmap_absl_hash_low_level_mix(
                chunk[0] ^ salt[1], chunk[1] ^ current_state);
            uint64_t cs1 = vmap_absl_hash_low_level_mix(
                chunk[2] ^ salt[2], chunk[3] ^ current_state);
            current_state = (cs0 ^ cs1);

            uint64_t ds0 = vmap_absl_hash_low_level_mix(
                chunk[4] ^ salt[3], chunk[5] ^ duplicated_state);
            uint64_t ds1 = vmap_absl_hash_low_level_mix(
                chunk[6] ^ salt[4], chunk[7] ^ duplicated_state);
            duplicated_state = (ds0 ^ ds1);
            ptr += 64;
            len -= 8;
        } while (len > 64);

        current_state = current_state ^ duplicated_state;
    }

    while (len > 16) {
        uint64_t a = vmap_load64(ptr);
        uint64_t b = vmap_load64(ptr + 8);

        current_state =
            vmap_absl_hash_low_level_mix(a ^ salt[1], b ^ current_state);

        ptr += 16;
        len -= 16;
    }

    uint64_t a = 0;
    uint64_t b = 0;
    if (len > 8) {
        a = vmap_load64(ptr);
        b = vmap_load64(ptr + len - 8);
    } else if (len > 3) {
        a = vmap_load32(ptr);
        b = vmap_load32(ptr + len - 4);
    } else if (len > 0) {
        a = vmap_load1to3(ptr, len);
    }

    uint64_t w = vmap_absl_hash_low_level_mix(a ^ salt[1], b ^ current_state);
    uint64_t z = salt[1] ^ starting_length;
    return vmap_absl_hash_low_level_mix(w, z);
}

static const void* const vmap_absl_hash_seed = &vmap_absl_hash_seed;

static const uint64_t vmap_absl_hash_salt[5] = {
    0x243F6A8885A308D3, 0x13198A2E03707344, 0xA4093822299F31D0,
    0x082EFA98EC4E6C89, 0x452821E638D01377,
};

#define VMAP_ABSL_HASH_PIECEWISE_CHUNK ((size_t)1024)

typedef uint64_t vmap_absl_hash_state;
#define VMAP_ABSL_HASH_INIT ((vmap_absl_hash_state)vmap_absl_hash_seed)

static inline void vmap_absl_hash_mix(vmap_absl_hash_state* state, uint64_t v) {
    const uint64_t mul = sizeof(size_t) == 4 ? 0xcc9e2d51 : 0x9ddfea08eb382d69;
    *state = vmap_absl_hash_low_level_mix(*state + v, mul);
}

VMAP_INLINE_NEVER static uint64_t vmap_absl_hash_hash64(const void* val,
                                                        size_t len) {
    return vmap_absl_hash_low_level_hash(val, len, VMAP_ABSL_HASH_INIT,
                                         vmap_absl_hash_salt);
}

typedef size_t vmap_fxhash_state;
#define VMAP_FXHASH_INIT ((vmap_fxhash_state)0)

static inline void vmap_fxhash_write(vmap_fxhash_state* state, const void* val,
                                     size_t len) {
    const uint64_t seed = (size_t)(UINT64_C(0x517cc1b727220a95));
    const uint32_t rotate = 5;

    const char* p = (const char*)val;
    vmap_fxhash_state state_ = *state;

    while (len > 0) {
        size_t word = 0;
        size_t to_read = len >= sizeof(state_) ? sizeof(state_) : len;
        memcpy(&word, p, to_read);

        state_ = vmap_roate_left(state_, rotate);

        state_ ^= word;
        state_ *= seed;

        len -= to_read;
        p += to_read;
    }
    *state = state_;
}

static inline size_t vmap_fxhash_finish(vmap_fxhash_state state) {
    return state;
}

static inline void vmap_absl_hash_write(vmap_absl_hash_state* state,
                                        const void* val, size_t len) {
    const char* val8 = (const char*)val;
    if (VMAP_LIKELY(len < VMAP_ABSL_HASH_PIECEWISE_CHUNK)) {
        goto vmap_absl_hash_write_small;
    }

    while (len >= VMAP_ABSL_HASH_PIECEWISE_CHUNK) {
        vmap_absl_hash_mix(
            state, vmap_absl_hash_hash64(val8, VMAP_ABSL_HASH_PIECEWISE_CHUNK));
        len -= VMAP_ABSL_HASH_PIECEWISE_CHUNK;
        val8 += VMAP_ABSL_HASH_PIECEWISE_CHUNK;
    }

vmap_absl_hash_write_small: {
    uint64_t v;
    if (len > 16) {
        v = vmap_absl_hash_hash64(val8, len);
    } else if (len > 8) {
        vmap_u128 p = vmap_load9to16(val8, len);
        vmap_absl_hash_mix(state, p.lo);
        v = p.hi;
    } else if (len >= 4) {
        v = vmap_load4to8(val8, len);
    } else if (len > 0) {
        v = vmap_load1to3(val8, len);
    } else {
        return;
    }

    vmap_absl_hash_mix(state, v);
}
}

static inline size_t vmap_absl_hash_finish(vmap_absl_hash_state state) {
    return state;
}

typedef struct {
    vmap_control_byte* ctrl;
    char* slots;
    size_t size;
    size_t capacity;
    size_t growth_left;
} vmap_raw_map;

typedef struct {
    vmap_raw_map* set;
    vmap_control_byte* ctrl;
    char* slot;
} vmap_raw_iter;

typedef struct {
    size_t size;
    size_t align;
    void (*copy)(void* dst, const void* src);
    void (*dtor)(void* val);
} vmap_object_type;

typedef struct {
    size_t (*hash)(const void* val);
    bool (*eq)(const void* needle, const void* candidate);
} vmap_key_type;

typedef struct {
    void* (*alloc)(size_t size, size_t align);
    void (*free)(void* array, size_t size, size_t align);
} vmap_alloc_type;

typedef struct {
    size_t size;
    size_t align;
    void (*init)(void* slot);
    void (*del)(void* slot);
    void (*transfer)(void* dst, void* src);
    void* (*get)(void* slot);
} vmap_slot_type;

typedef struct {
    const vmap_object_type* obj;
    const vmap_key_type* key;
    const vmap_alloc_type* alloc;
    const vmap_slot_type* slot;
} vmap_type;

#define VMAP_DECLARE_FLAT_SET_TYPE(vtype, Type, ...)                           \
    VMAP_DECLARE_TYPE(vtype, Type, Type, __VA_ARGS__)

#define VMAP_DECLARE_FLAT_MAP_TYPE(vtype, K, V, ...)                           \
    typedef struct {                                                           \
        K k;                                                                   \
        V v;                                                                   \
    } vtype##_entry;                                                           \
    VMAP_DECLARE_TYPE(vtype, vtype##_entry, K, __VA_ARGS__)

#define VMAP_DECLARE_TYPE(vtype, Type_, Key_, ...)                             \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    inline void vtype##_default_copy(void* dst, const void* src) {             \
        memcpy(dst, src, sizeof(Type_));                                       \
    }                                                                          \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    inline size_t vtype##_default_hash(const void* val) {                      \
        vmap_absl_hash_state state = VMAP_ABSL_HASH_INIT;                      \
        vmap_absl_hash_write(&state, val, sizeof(Key_));                       \
        return vmap_absl_hash_finish(state);                                   \
    }                                                                          \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    inline bool vtype##_default_eq(const void* a, const void* b) {             \
        return memcmp(a, b, sizeof(Key_)) == 0;                                \
    }                                                                          \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    inline void vtype##_default_slot_init(void* slot) {}                       \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    inline void vtype##_default_slot_transfer(void* dst, void* src) {          \
        memcpy(dst, src, sizeof(Type_));                                       \
    }                                                                          \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    inline void* vtype##_default_slot_get(void* slot) { return slot; }         \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    inline void vtype##_default_slot_dtor(void* slot) {                        \
        if (VMAP_EXTRACT(obj_dtor, NULL, __VA_ARGS__) != NULL) {               \
            VMAP_EXTRACT(obj_dtor, (void (*)(void*))NULL, __VA_ARGS__)(slot);  \
        }                                                                      \
    }                                                                          \
                                                                               \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    const vmap_object_type vtype##_object_type = {                             \
        sizeof(Type_),                                                         \
        alignof(Type_),                                                        \
        VMAP_EXTRACT(obj_copy, vtype##_default_copy, __VA_ARGS__),             \
        VMAP_EXTRACT(obj_dtor, NULL, __VA_ARGS__),                             \
    };                                                                         \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    const vmap_key_type vtype##_key_type = {                                   \
        VMAP_EXTRACT(key_hash, vtype##_default_hash, __VA_ARGS__),             \
        VMAP_EXTRACT(key_eq, vtype##_default_eq, __VA_ARGS__),                 \
    };                                                                         \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    const vmap_alloc_type vtype##_alloc_type = {                               \
        VMAP_EXTRACT(alloc_alloc, vmap_default_malloc, __VA_ARGS__),           \
        VMAP_EXTRACT(alloc_free, vmap_default_free, __VA_ARGS__),              \
    };                                                                         \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    const vmap_slot_type vtype##_slot_type = {                                 \
        VMAP_EXTRACT(slot_size, sizeof(Type_), __VA_ARGS__),                   \
        VMAP_EXTRACT(slot_align, sizeof(Type_), __VA_ARGS__),                  \
        VMAP_EXTRACT(slot_init, vtype##_default_slot_init, __VA_ARGS__),       \
        VMAP_EXTRACT(slot_dtor, vtype##_default_slot_dtor, __VA_ARGS__),       \
        VMAP_EXTRACT(slot_transfer, vtype##_default_slot_transfer,             \
                     __VA_ARGS__),                                             \
        VMAP_EXTRACT(slot_get, vtype##_default_slot_get, __VA_ARGS__),         \
    };                                                                         \
    VMAP_EXTRACT_RAW(modifiers, static, __VA_ARGS__)                           \
    const vmap_type vtype = {                                                  \
        &vtype##_object_type,                                                  \
        &vtype##_key_type,                                                     \
        &vtype##_alloc_type,                                                   \
        &vtype##_slot_type,                                                    \
    }

static inline void* vmap_default_malloc(size_t size, size_t align) {
    void* p = malloc(size);
    VMAP_CHECK(p != NULL, "malloc returned NULL");
    return p;
}

static inline void vmap_default_free(void* array, size_t size, size_t align) {
    free(array);
}

static inline void vmap_raw_map_dump(const vmap_type* type,
                                     const vmap_raw_map* self) {
    fprintf(stderr, "ptr: %p, len: %zu, cap: %zu, growth: %zu\n", self->ctrl,
            self->size, self->capacity, self->growth_left);
    if (self->capacity == 0) {
        return;
    }

    size_t ctrl_bytes = self->capacity + vmap_num_cloned_bytes();

    size_t slot_size = type->slot->size;
    size_t obj_size = type->obj->size;

    for (size_t i = 0; i <= ctrl_bytes; ++i) {
        fprintf(stderr, "[%4zu] %p / ", i, &self->ctrl[i]);
        switch (self->ctrl[i]) {
        case VMAP_SENTINEL:
            fprintf(stderr, "Sentinel: //\n");
            continue;
        case VMAP_EMPTY:
            fprintf(stderr, "   Empty");
            break;
        case VMAP_DELETED:
            fprintf(stderr, " Deleted");
            break;
        default:
            fprintf(stderr, " H@(0x%02X)", self->ctrl[i]);
            break;
        }

        if (i >= self->capacity) {
            fprintf(stderr, ": <mirrored>\n");
            continue;
        }

        char* slot = self->slots + i * slot_size;
        fprintf(stderr, ": %p /", slot);
        for (size_t j = 0; j < slot_size; ++j) {
            fprintf(stderr, " %0x2x", (unsigned char)slot[j]);
        }
        char* elem = (char*)type->slot->get(slot);
        if (elem != slot && vmap_is_full(self->ctrl[i])) {
            fprintf(stderr, " ->");
            for (size_t j = 0; j < obj_size; ++j) {
                fprintf(stderr, " %02x", (unsigned char)elem[j]);
            }
        }
        fprintf(stderr, "\n");
    }
}

static inline void vmap_raw_iter_skip_empty_or_deleted(const vmap_type* type,
                                                       vmap_raw_iter* self) {
    while (vmap_is_empty_or_deleted(*self->ctrl)) {
        vmap_group g = vmap_group_new(self->ctrl);
        uint32_t shift = vmap_group_count_leading_empty_or_deleted(&g);
        self->ctrl += shift;
        self->slot += shift * type->slot->size;
    }

    if (VMAP_UNLIKELY(*self->ctrl == VMAP_SENTINEL)) {
        self->ctrl = NULL;
        self->slot = NULL;
    }
}

static inline vmap_raw_iter
vmap_raw_map_iter_at(const vmap_type* type, vmap_raw_map* self, size_t index) {
    vmap_raw_iter iter = {
        self,
        self->ctrl + index,
        self->slots + index * type->slot->size,
    };
    vmap_raw_iter_skip_empty_or_deleted(type, &iter);
    VMAP_ASSERT_IS_VALID(iter.ctrl);
    return iter;
}

static inline vmap_raw_iter vmap_raw_map_iter(const vmap_type* type,
                                              vmap_raw_map* self) {
    return vmap_raw_map_iter_at(type, self, 0);
}

static inline vmap_raw_iter vmap_raw_map_citer(const vmap_type* type,
                                               const vmap_raw_map* self) {
    return vmap_raw_map_iter(type, (vmap_raw_map*)self);
}

static inline void* vmap_raw_iter_get(const vmap_type* type,
                                      const vmap_raw_iter* self) {
    VMAP_ASSERT_IS_VALID(self->ctrl);
    if (self->slot == NULL) {
        return NULL;
    }
    return type->slot->get(self->slot);
}

static inline void* vmap_raw_iter_next(const vmap_type* type,
                                       vmap_raw_iter* self) {
    VMAP_ASSERT_IS_FULL(self->ctrl);
    ++self->ctrl;
    self->slot += type->slot->size;
    vmap_raw_iter_skip_empty_or_deleted(type, self);
    return vmap_raw_iter_get(type, self);
}

static inline void vmap_raw_map_erase_meta_only(const vmap_type* type,
                                                vmap_raw_iter it) {
    VMAP_CHECK(vmap_is_full(*it.ctrl), "erasing a dangling iterator");
    --it.set->size;
    const size_t index = (size_t)(it.ctrl - it.set->ctrl);
    const size_t index_before = (index - VMAP_GROUP_WIDTH) & it.set->capacity;
    vmap_group g_after = vmap_group_new(it.ctrl);
    vmap_bitmask empty_after = vmap_group_match_empty(&g_after);
    vmap_group g_before = vmap_group_new(it.set->ctrl + index_before);
    vmap_bitmask empty_before = vmap_group_match_empty(&g_before);

    bool was_never_full =
        empty_before.mask && empty_after.mask &&
        (size_t)(vmap_bitmask_trailing_zeros(&empty_after) +
                 vmap_bitmask_leading_zeros(&empty_before)) < VMAP_GROUP_WIDTH;

    vmap_set_ctrl(index, was_never_full ? VMAP_EMPTY : VMAP_DELETED,
                  it.set->capacity, it.set->ctrl, it.set->slots,
                  type->slot->size);
    it.set->growth_left += was_never_full;
}

static inline void vmap_raw_map_reset_growth_left(const vmap_type* type,
                                                  vmap_raw_map* self) {
    self->growth_left = vmap_capacity_to_growth(self->capacity) - self->size;
}

static inline void vmap_raw_map_initialize_slots(const vmap_type* type,
                                                 vmap_raw_map* self) {
    VMAP_CHECK(self->capacity, "capacity should be nonzero");
    char* mem = (char*)type->alloc->alloc(
        vmap_alloc_size(self->capacity, type->slot->size, type->slot->align),
        type->slot->align);
    self->ctrl = (vmap_control_byte*)mem;
    self->slots = mem + vmap_slot_offset(self->capacity, type->slot->align);
    vmap_reset_ctrl(self->capacity, self->ctrl, self->slots, type->slot->size);
    vmap_raw_map_reset_growth_left(type, self);
}

static inline void vmap_raw_map_destroy_slots(const vmap_type* type,
                                              vmap_raw_map* self) {
    if (self->capacity == 0) {
        return;
    }

    size_t capacity = self->capacity;
    size_t slot_size = type->slot->size;

    if (type->slot->del != NULL) {
        for (size_t i = 0; i != capacity; ++i) {
            if (vmap_is_full(self->ctrl[i])) {
                type->slot->del(self->slots + i * slot_size);
            }
        }
    }

    type->alloc->free(
        self->ctrl,
        vmap_alloc_size(self->capacity, slot_size, type->slot->align),
        type->slot->align);
    self->ctrl = vmap_empty_group();
    self->slots = NULL;
    self->size = 0;
    self->capacity = 0;
    self->growth_left = 0;
}

static inline void vmap_raw_map_resize(const vmap_type* type,
                                       vmap_raw_map* self,
                                       size_t new_capacity) {
    VMAP_CHECK(vmap_is_valid_capacity(new_capacity), "invalid capacity: %zu",
               new_capacity);

    vmap_control_byte* old_ctrl = self->ctrl;
    char* old_slots = self->slots;
    const size_t old_capacity = self->capacity;
    size_t slot_size = type->slot->size;
    self->capacity = new_capacity;
    vmap_raw_map_initialize_slots(type, self);

    size_t total_probe_length = 0;
    for (size_t i = 0; i != old_capacity; ++i) {
        if (vmap_is_full(old_ctrl[i])) {
            size_t hash =
                type->key->hash(type->slot->get(old_slots + i * slot_size));
            vmap_find_info target =
                vmap_find_first_non_full(self->ctrl, hash, self->capacity);
            size_t new_i = target.offset;
            total_probe_length += target.probe_length;
            vmap_set_ctrl(new_i, vmap_h2(hash), self->capacity, self->ctrl,
                          self->slots, slot_size);
            type->slot->transfer(self->slots + new_i * slot_size,
                                 old_slots + i * slot_size);
        }
    }

    if (old_capacity) {
        vmap_unpoison_memory(old_slots, slot_size * old_capacity);
        type->alloc->free(
            old_ctrl,
            vmap_alloc_size(old_capacity, slot_size, type->slot->align),
            type->slot->align);
    }
}

VMAP_INLINE_NEVER static void
vmap_raw_map_drop_deletes_without_resize(const vmap_type* type,
                                         vmap_raw_map* self) {
    VMAP_CHECK(vmap_is_valid_capacity(self->capacity), "invalid capacity");
    VMAP_CHECK(!vmap_is_small(self->capacity), "unexpected small capacity: %zu",
               self->capacity);

    vmap_convert_deleted_to_empty_and_full_to_deleted(self->ctrl,
                                                      self->capacity);
    size_t total_probe_length = 0;

    void* slot = type->alloc->alloc(type->slot->size, type->slot->align);
    size_t slot_size = type->slot->size;

    for (size_t i = 0; i != self->capacity; ++i) {
        if (!vmap_is_deleted(self->ctrl[i])) {
            continue;
        }

        char* old_slot = self->slots + i * slot_size;
        size_t hash = type->key->hash(type->slot->get(old_slot));

        const vmap_find_info target =
            vmap_find_first_non_full(self->ctrl, hash, self->capacity);
        const size_t new_i = target.offset;

        total_probe_length += target.probe_length;

        char* new_slot = self->slots + new_i * slot_size;

        const size_t probe_offset =
            vmap_probe_seq_start(self->ctrl, hash, self->capacity).offset;

#define vmap_probe_index(pos)                                                  \
    (((pos - probe_offset) & self->capacity) / VMAP_GROUP_WIDTH)

        if (VMAP_LIKELY(vmap_probe_index(new_i) == vmap_probe_index(i))) {
            vmap_set_ctrl(i, vmap_h2(hash), self->capacity, self->ctrl,
                          self->slots, slot_size);
            continue;
        }

        if (vmap_is_empty(self->ctrl[new_i])) {
            vmap_set_ctrl(new_i, vmap_h2(hash), self->capacity, self->ctrl,
                          self->slots, slot_size);
            type->slot->transfer(new_slot, old_slot);
            vmap_set_ctrl(i, VMAP_EMPTY, self->capacity, self->ctrl,
                          self->slots, slot_size);
        } else {
            VMAP_CHECK(vmap_is_deleted(self->ctrl[new_i]),
                       "bad ctrl value at %zu: %02x", new_i, self->ctrl[new_i]);
            vmap_set_ctrl(new_i, vmap_h2(hash), self->capacity, self->ctrl,
                          self->slots, slot_size);

            type->slot->transfer(slot, old_slot);
            type->slot->transfer(old_slot, new_slot);
            type->slot->transfer(new_slot, slot);
            --i;
        }
#undef vmap_probe_seq_start_index
    }

    vmap_raw_map_reset_growth_left(type, self);
    type->alloc->free(slot, slot_size, type->slot->align);
}

static inline void
vmap_raw_map_rehash_and_grow_if_necessary(const vmap_type* type,
                                          vmap_raw_map* self) {
    if (self->capacity == 0) {
        vmap_raw_map_resize(type, self, 1);
    } else if (self->capacity > VMAP_GROUP_WIDTH &&
               self->size * UINT64_C(32) <= self->capacity * UINT64_C(25)) {
        vmap_raw_map_drop_deletes_without_resize(type, self);
    } else {
        vmap_raw_map_resize(type, self, self->capacity * 2 + 1);
    }
}

static inline void vmap_raw_map_prefetch_heap_block(const vmap_type* type,
                                                    const vmap_raw_map* self) {
    VMAP_PREFETCH(self->ctrl, 1);
}

typedef struct {
    size_t index;
    bool inserted;
} vmap_prepare_insert;

VMAP_INLINE_NEVER static size_t
vmap_raw_map_prepare_insert(const vmap_type* type, vmap_raw_map* self,
                            size_t hash) {
    vmap_find_info target =
        vmap_find_first_non_full(self->ctrl, hash, self->capacity);
    if (VMAP_UNLIKELY(self->growth_left == 0 &&
                      !vmap_is_deleted(self->ctrl[target.offset]))) {
        vmap_raw_map_rehash_and_grow_if_necessary(type, self);
        target = vmap_find_first_non_full(self->ctrl, hash, self->capacity);
    }
    ++self->size;
    self->growth_left -= vmap_is_empty(self->ctrl[target.offset]);
    vmap_set_ctrl(target.offset, vmap_h2(hash), self->capacity, self->ctrl,
                  self->slots, type->slot->size);
    return target.offset;
}

static inline vmap_prepare_insert
vmap_raw_map_find_or_prepare_insert(const vmap_type* type,
                                    const vmap_key_type* key_type,
                                    vmap_raw_map* self, const void* key) {
    vmap_raw_map_prefetch_heap_block(type, self);
    size_t hash = key_type->hash(key);
    vmap_probe_seq seq = vmap_probe_seq_start(self->ctrl, hash, self->capacity);
    size_t slot_size = type->slot->size;
    while (true) {
        vmap_group g = vmap_group_new(self->ctrl + seq.offset);
        vmap_bitmask match = vmap_group_match(&g, vmap_h2(hash));
        uint32_t i;
        while (vmap_bitmask_next(&match, &i)) {
            size_t idx = vmap_probe_seq_offset(&seq, i);
            char* slot = self->slots + idx * slot_size;
            if (VMAP_LIKELY(key_type->eq(key, type->slot->get(slot)))) {
                return (vmap_prepare_insert){idx, false};
            }
        }
        if (VMAP_LIKELY(vmap_group_match_empty(&g).mask)) {
            break;
        }
        vmap_probe_seq_next(&seq);
        VMAP_CHECK(seq.index <= self->capacity, "full table");
    }
    return (vmap_prepare_insert){vmap_raw_map_prepare_insert(type, self, hash),
                                 true};
}

static inline void* vmap_raw_map_pre_insert(const vmap_type* type,
                                            vmap_raw_map* self, size_t i) {
    void* dst = self->slots + i * type->slot->size;
    type->slot->init(dst);
    return type->slot->get(dst);
}

static inline vmap_raw_map vmap_raw_map_new(const vmap_type* type,
                                            size_t capacity) {
    vmap_raw_map self = {
        .ctrl = vmap_empty_group(),
    };

    if (capacity != 0) {
        self.capacity = vmap_normalize_capacity(capacity);
        vmap_raw_map_initialize_slots(type, &self);
    }
    return self;
}

static inline void vmap_raw_map_reserve(const vmap_type* type,
                                        vmap_raw_map* self, size_t n) {
    if (n <= self->size + self->growth_left) {
        return;
    }

    n = vmap_normalize_capacity(vmap_growth_to_lower_bound_capacity(n));
    vmap_raw_map_resize(type, self, n);
}

static inline void vmap_raw_map_rehash(const vmap_type* type,
                                       vmap_raw_map* self, size_t n) {
    if (n == 0 && self->capacity == 0) {
        return;
    }
    if (n == 0 && self->size == 0) {
        vmap_raw_map_destroy_slots(type, self);
        return;
    }

    size_t m = vmap_normalize_capacity(
        n | vmap_growth_to_lower_bound_capacity(self->size));
    if (n == 0 || m > self->capacity) {
        vmap_raw_map_resize(type, self, m);
    }
}

static inline void vmap_raw_map_destroy(const vmap_type* type,
                                        vmap_raw_map* self) {
    vmap_raw_map_destroy_slots(type, self);
}

static inline bool vmap_raw_map_empty(const vmap_type* type,
                                      const vmap_raw_map* self) {
    return self->size == 0;
}

static inline size_t vmap_raw_map_size(const vmap_type* tpye,
                                       const vmap_raw_map* self) {
    return self->size;
}

static inline size_t vmap_raw_map_capacity(const vmap_type* type,
                                           const vmap_raw_map* self) {
    return self->capacity;
}

static inline void vmap_raw_map_clear(const vmap_type* type,
                                      vmap_raw_map* self) {
    if (self->capacity > 127) {
        vmap_raw_map_destroy_slots(type, self);
    } else if (self->capacity) {
        if (type->slot->del != NULL) {
            for (size_t i = 0; i != self->capacity; ++i) {
                if (vmap_is_full(self->ctrl[i])) {
                    type->slot->del(self->slots + i * type->slot->size);
                }
            }
        }
        self->size = 0;
        vmap_reset_ctrl(self->capacity, self->ctrl, self->slots,
                        type->slot->size);
        vmap_raw_map_reset_growth_left(type, self);
    }
    VMAP_CHECK(!self->size, "size was still nonzero");
}

static inline bool vmap_raw_map_insert(const vmap_type* type,
                                       vmap_raw_map* self, const void* val) {
    vmap_prepare_insert res =
        vmap_raw_map_find_or_prepare_insert(type, type->key, self, val);
    if (res.inserted) {
        void* slot = vmap_raw_map_pre_insert(type, self, res.index);
        type->obj->copy(slot, val);
    }
    return res.inserted;
}

static inline vmap_raw_iter vmap_raw_map_find_hinted_it(
    const vmap_type* type, const vmap_key_type* key_type,
    const vmap_raw_map* self, const void* key, size_t hash) {
    vmap_probe_seq seq = vmap_probe_seq_start(self->ctrl, hash, self->capacity);
    while (true) {
        vmap_group g = vmap_group_new(self->ctrl + seq.offset);
        vmap_bitmask match = vmap_group_match(&g, vmap_h2(hash));
        uint32_t i;
        while (vmap_bitmask_next(&match, &i)) {
            char* slot =
                self->slots + vmap_probe_seq_offset(&seq, i) * type->slot->size;
            if (VMAP_LIKELY(key_type->eq(key, type->slot->get(slot)))) {
                return vmap_raw_map_iter_at(type, (vmap_raw_map*)self,
                                            vmap_probe_seq_offset(&seq, i));
            }
        }
        if (VMAP_LIKELY(vmap_group_match_empty(&g).mask)) {
            return (vmap_raw_iter){0};
        }
        vmap_probe_seq_next(&seq);
        VMAP_CHECK(seq.index <= self->capacity, "full table");
    }
}

static inline vmap_raw_iter vmap_raw_map_find_it(const vmap_type* type,
                                                 const vmap_key_type* key_type,
                                                 const vmap_raw_map* self,
                                                 const void* key) {
    return vmap_raw_map_find_hinted_it(type, key_type, self, key,
                                       key_type->hash(key));
}

static inline const void*
vmap_raw_map_find_hinted(const vmap_type* type, const vmap_key_type* key_type,
                         const vmap_raw_map* self, const void* key,
                         size_t hash) {
    vmap_probe_seq seq = vmap_probe_seq_start(self->ctrl, hash, self->capacity);
    size_t slot_size = type->slot->size;
    vmap_h2_t h2 = vmap_h2(hash);
    while (true) {
        vmap_group g = vmap_group_new(self->ctrl + seq.offset);
        vmap_bitmask match = vmap_group_match(&g, h2);
        uint32_t i;
        while (vmap_bitmask_next(&match, &i)) {
            char* slot =
                self->slots + vmap_probe_seq_offset(&seq, i) * slot_size;
            if (VMAP_LIKELY(key_type->eq(key, type->slot->get(slot)))) {
                return type->slot->get(slot);
            }
        }
        if (VMAP_LIKELY(vmap_group_match_empty(&g).mask)) {
            return NULL;
        }
        vmap_probe_seq_next(&seq);
        VMAP_CHECK(seq.index <= self->capacity, "full table!");
    }
}

static inline const void* vmap_raw_map_find(const vmap_type* type,
                                            const vmap_key_type* key_type,
                                            const vmap_raw_map* self,
                                            const void* key) {
    return vmap_raw_map_find_hinted(type, key_type, self, key,
                                    key_type->hash(key));
}

static inline void vmap_raw_map_erase_at(const vmap_type* type,
                                         vmap_raw_iter it) {
    VMAP_ASSERT_IS_FULL(it.ctrl);
    if (type->slot->del != NULL) {
        type->slot->del(it.slot);
    }
    vmap_raw_map_erase_meta_only(type, it);
}

static inline bool vmap_raw_map_contains(const vmap_type* type,
                                         const vmap_key_type* key_type,
                                         const vmap_raw_map* self,
                                         const void* key) {
    return vmap_raw_map_find(type, key_type, self, key) != NULL;
}

static inline bool vmap_raw_map_erase(const vmap_type* type,
                                      const vmap_key_type* key_type,
                                      vmap_raw_map* self, const void* key) {
    vmap_raw_iter it = vmap_raw_map_find_it(type, key_type, self, key);
    if (it.slot == NULL) {
        return false;
    }
    vmap_raw_map_erase_at(type, it);
    return true;
}

#define VMAP_EXTRACT(needle, default, ...)                                     \
    (VMAP_EXTRACT_RAW(needle, default, __VA_ARGS__))

#define VMAP_EXTRACT_RAW(needle, default, ...)                                 \
    VMAP_EXTRACT00(VMAP_EXTRACT_##needle, __VA_ARGS__, (needle, default))

#define VMAP_EXTRACT_VALUE(key, val) val

/* the rest of this file is generated by extract.py */
/* !!! */

#define VMAP_EXTRACT_obj_copy(key_, val_) VMAP_EXTRACT_obj_copyZ##key_
#define VMAP_EXTRACT_obj_copyZobj_copy VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_obj_dtor(key_, val_) VMAP_EXTRACT_obj_dtorZ##key_
#define VMAP_EXTRACT_obj_dtorZobj_dtor VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_key_hash(key_, val_) VMAP_EXTRACT_key_hashZ##key_
#define VMAP_EXTRACT_key_hashZkey_hash VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_key_eq(key_, val_) VMAP_EXTRACT_key_eqZ##key_
#define VMAP_EXTRACT_key_eqZkey_eq VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_alloc_alloc(key_, val_) VMAP_EXTRACT_alloc_allocZ##key_
#define VMAP_EXTRACT_alloc_allocZalloc_alloc                                   \
    VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_alloc_free(key_, val_) VMAP_EXTRACT_alloc_freeZ##key_
#define VMAP_EXTRACT_alloc_freeZalloc_free                                     \
    VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_slot_size(key_, val_) VMAP_EXTRACT_slot_sizeZ##key_
#define VMAP_EXTRACT_slot_sizeZslot_size                                       \
    VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_slot_align(key_, val_) VMAP_EXTRACT_slot_alignZ##key_
#define VMAP_EXTRACT_slot_alignZslot_align                                     \
    VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_slot_init(key_, val_) VMAP_EXTRACT_slot_initZ##key_
#define VMAP_EXTRACT_slot_initZslot_init                                       \
    VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_slot_transfer(key_, val_) VMAP_EXTRACT_slot_transferZ##key_
#define VMAP_EXTRACT_slot_transferZslot_transfer                               \
    VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_slot_get(key_, val_) VMAP_EXTRACT_slot_getZ##key_
#define VMAP_EXTRACT_slot_getZslot_get VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_slot_dtor(key_, val_) VMAP_EXTRACT_slot_dtorZ##key_
#define VMAP_EXTRACT_slot_dtorZslot_dtor                                       \
    VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING
#define VMAP_EXTRACT_modifiers(key_, val_) VMAP_EXTRACT_modifiersZ##key_
#define VMAP_EXTRACT_modifiersZmodifiers                                       \
    VMAP_NOTHING, VMAP_NOTHING, VMAP_NOTHING

#define VMAP_EXTRACT00(needle_, kv_, ...)                                      \
    VMAP_SELECT00(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT01,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT01(needle_, kv_, ...)                                      \
    VMAP_SELECT01(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT02,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT02(needle_, kv_, ...)                                      \
    VMAP_SELECT02(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT03,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT03(needle_, kv_, ...)                                      \
    VMAP_SELECT03(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT04,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT04(needle_, kv_, ...)                                      \
    VMAP_SELECT04(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT05,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT05(needle_, kv_, ...)                                      \
    VMAP_SELECT05(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT06,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT06(needle_, kv_, ...)                                      \
    VMAP_SELECT06(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT07,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT07(needle_, kv_, ...)                                      \
    VMAP_SELECT07(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT08,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT08(needle_, kv_, ...)                                      \
    VMAP_SELECT08(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT09,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT09(needle_, kv_, ...)                                      \
    VMAP_SELECT09(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT0A,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT0A(needle_, kv_, ...)                                      \
    VMAP_SELECT0A(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT0B,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT0B(needle_, kv_, ...)                                      \
    VMAP_SELECT0B(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT0C,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT0C(needle_, kv_, ...)                                      \
    VMAP_SELECT0C(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT0D,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT0D(needle_, kv_, ...)                                      \
    VMAP_SELECT0D(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT0E,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT0E(needle_, kv_, ...)                                      \
    VMAP_SELECT0E(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT0F,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT0F(needle_, kv_, ...)                                      \
    VMAP_SELECT0F(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT10,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT10(needle_, kv_, ...)                                      \
    VMAP_SELECT10(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT11,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT11(needle_, kv_, ...)                                      \
    VMAP_SELECT11(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT12,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT12(needle_, kv_, ...)                                      \
    VMAP_SELECT12(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT13,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT13(needle_, kv_, ...)                                      \
    VMAP_SELECT13(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT14,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT14(needle_, kv_, ...)                                      \
    VMAP_SELECT14(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT15,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT15(needle_, kv_, ...)                                      \
    VMAP_SELECT15(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT16,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT16(needle_, kv_, ...)                                      \
    VMAP_SELECT16(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT17,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT17(needle_, kv_, ...)                                      \
    VMAP_SELECT17(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT18,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT18(needle_, kv_, ...)                                      \
    VMAP_SELECT18(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT19,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT19(needle_, kv_, ...)                                      \
    VMAP_SELECT19(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT1A,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT1A(needle_, kv_, ...)                                      \
    VMAP_SELECT1A(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT1B,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT1B(needle_, kv_, ...)                                      \
    VMAP_SELECT1B(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT1C,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT1C(needle_, kv_, ...)                                      \
    VMAP_SELECT1C(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT1D,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT1D(needle_, kv_, ...)                                      \
    VMAP_SELECT1D(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT1E,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT1E(needle_, kv_, ...)                                      \
    VMAP_SELECT1E(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT1F,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT1F(needle_, kv_, ...)                                      \
    VMAP_SELECT1F(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT20,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT20(needle_, kv_, ...)                                      \
    VMAP_SELECT20(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT21,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT21(needle_, kv_, ...)                                      \
    VMAP_SELECT21(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT22,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT22(needle_, kv_, ...)                                      \
    VMAP_SELECT22(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT23,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT23(needle_, kv_, ...)                                      \
    VMAP_SELECT23(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT24,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT24(needle_, kv_, ...)                                      \
    VMAP_SELECT24(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT25,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT25(needle_, kv_, ...)                                      \
    VMAP_SELECT25(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT26,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT26(needle_, kv_, ...)                                      \
    VMAP_SELECT26(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT27,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT27(needle_, kv_, ...)                                      \
    VMAP_SELECT27(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT28,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT28(needle_, kv_, ...)                                      \
    VMAP_SELECT28(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT29,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT29(needle_, kv_, ...)                                      \
    VMAP_SELECT29(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT2A,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT2A(needle_, kv_, ...)                                      \
    VMAP_SELECT2A(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT2B,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT2B(needle_, kv_, ...)                                      \
    VMAP_SELECT2B(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT2C,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT2C(needle_, kv_, ...)                                      \
    VMAP_SELECT2C(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT2D,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT2D(needle_, kv_, ...)                                      \
    VMAP_SELECT2D(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT2E,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT2E(needle_, kv_, ...)                                      \
    VMAP_SELECT2E(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT2F,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT2F(needle_, kv_, ...)                                      \
    VMAP_SELECT2F(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT30,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT30(needle_, kv_, ...)                                      \
    VMAP_SELECT30(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT31,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT31(needle_, kv_, ...)                                      \
    VMAP_SELECT31(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT32,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT32(needle_, kv_, ...)                                      \
    VMAP_SELECT32(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT33,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT33(needle_, kv_, ...)                                      \
    VMAP_SELECT33(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT34,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT34(needle_, kv_, ...)                                      \
    VMAP_SELECT34(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT35,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT35(needle_, kv_, ...)                                      \
    VMAP_SELECT35(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT36,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT36(needle_, kv_, ...)                                      \
    VMAP_SELECT36(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT37,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT37(needle_, kv_, ...)                                      \
    VMAP_SELECT37(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT38,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT38(needle_, kv_, ...)                                      \
    VMAP_SELECT38(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT39,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT39(needle_, kv_, ...)                                      \
    VMAP_SELECT39(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT3A,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT3A(needle_, kv_, ...)                                      \
    VMAP_SELECT3A(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT3B,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT3B(needle_, kv_, ...)                                      \
    VMAP_SELECT3B(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT3C,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT3C(needle_, kv_, ...)                                      \
    VMAP_SELECT3C(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT3D,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT3D(needle_, kv_, ...)                                      \
    VMAP_SELECT3D(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT3E,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT3E(needle_, kv_, ...)                                      \
    VMAP_SELECT3E(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT3F,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)
#define VMAP_EXTRACT3F(needle_, kv_, ...)                                      \
    VMAP_SELECT3F(needle_ kv_, VMAP_EXTRACT_VALUE, kv_, VMAP_EXTRACT40,        \
                  (needle_, __VA_ARGS__), VMAP_NOTHING)

#define VMAP_SELECT00(x_, ...) VMAP_SELECT00_(x_, __VA_ARGS__)
#define VMAP_SELECT01(x_, ...) VMAP_SELECT01_(x_, __VA_ARGS__)
#define VMAP_SELECT02(x_, ...) VMAP_SELECT02_(x_, __VA_ARGS__)
#define VMAP_SELECT03(x_, ...) VMAP_SELECT03_(x_, __VA_ARGS__)
#define VMAP_SELECT04(x_, ...) VMAP_SELECT04_(x_, __VA_ARGS__)
#define VMAP_SELECT05(x_, ...) VMAP_SELECT05_(x_, __VA_ARGS__)
#define VMAP_SELECT06(x_, ...) VMAP_SELECT06_(x_, __VA_ARGS__)
#define VMAP_SELECT07(x_, ...) VMAP_SELECT07_(x_, __VA_ARGS__)
#define VMAP_SELECT08(x_, ...) VMAP_SELECT08_(x_, __VA_ARGS__)
#define VMAP_SELECT09(x_, ...) VMAP_SELECT09_(x_, __VA_ARGS__)
#define VMAP_SELECT0A(x_, ...) VMAP_SELECT0A_(x_, __VA_ARGS__)
#define VMAP_SELECT0B(x_, ...) VMAP_SELECT0B_(x_, __VA_ARGS__)
#define VMAP_SELECT0C(x_, ...) VMAP_SELECT0C_(x_, __VA_ARGS__)
#define VMAP_SELECT0D(x_, ...) VMAP_SELECT0D_(x_, __VA_ARGS__)
#define VMAP_SELECT0E(x_, ...) VMAP_SELECT0E_(x_, __VA_ARGS__)
#define VMAP_SELECT0F(x_, ...) VMAP_SELECT0F_(x_, __VA_ARGS__)
#define VMAP_SELECT10(x_, ...) VMAP_SELECT10_(x_, __VA_ARGS__)
#define VMAP_SELECT11(x_, ...) VMAP_SELECT11_(x_, __VA_ARGS__)
#define VMAP_SELECT12(x_, ...) VMAP_SELECT12_(x_, __VA_ARGS__)
#define VMAP_SELECT13(x_, ...) VMAP_SELECT13_(x_, __VA_ARGS__)
#define VMAP_SELECT14(x_, ...) VMAP_SELECT14_(x_, __VA_ARGS__)
#define VMAP_SELECT15(x_, ...) VMAP_SELECT15_(x_, __VA_ARGS__)
#define VMAP_SELECT16(x_, ...) VMAP_SELECT16_(x_, __VA_ARGS__)
#define VMAP_SELECT17(x_, ...) VMAP_SELECT17_(x_, __VA_ARGS__)
#define VMAP_SELECT18(x_, ...) VMAP_SELECT18_(x_, __VA_ARGS__)
#define VMAP_SELECT19(x_, ...) VMAP_SELECT19_(x_, __VA_ARGS__)
#define VMAP_SELECT1A(x_, ...) VMAP_SELECT1A_(x_, __VA_ARGS__)
#define VMAP_SELECT1B(x_, ...) VMAP_SELECT1B_(x_, __VA_ARGS__)
#define VMAP_SELECT1C(x_, ...) VMAP_SELECT1C_(x_, __VA_ARGS__)
#define VMAP_SELECT1D(x_, ...) VMAP_SELECT1D_(x_, __VA_ARGS__)
#define VMAP_SELECT1E(x_, ...) VMAP_SELECT1E_(x_, __VA_ARGS__)
#define VMAP_SELECT1F(x_, ...) VMAP_SELECT1F_(x_, __VA_ARGS__)
#define VMAP_SELECT20(x_, ...) VMAP_SELECT20_(x_, __VA_ARGS__)
#define VMAP_SELECT21(x_, ...) VMAP_SELECT21_(x_, __VA_ARGS__)
#define VMAP_SELECT22(x_, ...) VMAP_SELECT22_(x_, __VA_ARGS__)
#define VMAP_SELECT23(x_, ...) VMAP_SELECT23_(x_, __VA_ARGS__)
#define VMAP_SELECT24(x_, ...) VMAP_SELECT24_(x_, __VA_ARGS__)
#define VMAP_SELECT25(x_, ...) VMAP_SELECT25_(x_, __VA_ARGS__)
#define VMAP_SELECT26(x_, ...) VMAP_SELECT26_(x_, __VA_ARGS__)
#define VMAP_SELECT27(x_, ...) VMAP_SELECT27_(x_, __VA_ARGS__)
#define VMAP_SELECT28(x_, ...) VMAP_SELECT28_(x_, __VA_ARGS__)
#define VMAP_SELECT29(x_, ...) VMAP_SELECT29_(x_, __VA_ARGS__)
#define VMAP_SELECT2A(x_, ...) VMAP_SELECT2A_(x_, __VA_ARGS__)
#define VMAP_SELECT2B(x_, ...) VMAP_SELECT2B_(x_, __VA_ARGS__)
#define VMAP_SELECT2C(x_, ...) VMAP_SELECT2C_(x_, __VA_ARGS__)
#define VMAP_SELECT2D(x_, ...) VMAP_SELECT2D_(x_, __VA_ARGS__)
#define VMAP_SELECT2E(x_, ...) VMAP_SELECT2E_(x_, __VA_ARGS__)
#define VMAP_SELECT2F(x_, ...) VMAP_SELECT2F_(x_, __VA_ARGS__)
#define VMAP_SELECT30(x_, ...) VMAP_SELECT30_(x_, __VA_ARGS__)
#define VMAP_SELECT31(x_, ...) VMAP_SELECT31_(x_, __VA_ARGS__)
#define VMAP_SELECT32(x_, ...) VMAP_SELECT32_(x_, __VA_ARGS__)
#define VMAP_SELECT33(x_, ...) VMAP_SELECT33_(x_, __VA_ARGS__)
#define VMAP_SELECT34(x_, ...) VMAP_SELECT34_(x_, __VA_ARGS__)
#define VMAP_SELECT35(x_, ...) VMAP_SELECT35_(x_, __VA_ARGS__)
#define VMAP_SELECT36(x_, ...) VMAP_SELECT36_(x_, __VA_ARGS__)
#define VMAP_SELECT37(x_, ...) VMAP_SELECT37_(x_, __VA_ARGS__)
#define VMAP_SELECT38(x_, ...) VMAP_SELECT38_(x_, __VA_ARGS__)
#define VMAP_SELECT39(x_, ...) VMAP_SELECT39_(x_, __VA_ARGS__)
#define VMAP_SELECT3A(x_, ...) VMAP_SELECT3A_(x_, __VA_ARGS__)
#define VMAP_SELECT3B(x_, ...) VMAP_SELECT3B_(x_, __VA_ARGS__)
#define VMAP_SELECT3C(x_, ...) VMAP_SELECT3C_(x_, __VA_ARGS__)
#define VMAP_SELECT3D(x_, ...) VMAP_SELECT3D_(x_, __VA_ARGS__)
#define VMAP_SELECT3E(x_, ...) VMAP_SELECT3E_(x_, __VA_ARGS__)
#define VMAP_SELECT3F(x_, ...) VMAP_SELECT3F_(x_, __VA_ARGS__)

#define VMAP_SELECT00_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT01_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT02_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT03_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT04_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT05_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT06_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT07_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT08_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT09_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT0A_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT0B_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT0C_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT0D_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT0E_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT0F_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT10_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT11_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT12_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT13_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT14_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT15_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT16_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT17_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT18_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT19_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT1A_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT1B_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT1C_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT1D_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT1E_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT1F_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT20_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT21_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT22_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT23_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT24_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT25_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT26_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT27_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT28_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT29_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT2A_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT2B_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT2C_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT2D_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT2E_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT2F_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT30_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT31_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT32_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT33_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT34_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT35_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT36_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT37_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT38_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT39_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT3A_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT3B_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT3C_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT3D_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT3E_(ignored_, _call_, _args_, call_, args_, ...) call_ args_
#define VMAP_SELECT3F_(ignored_, _call_, _args_, call_, args_, ...) call_ args_

#define VMAP_DECLARE_FLAT_HASHSET(hashset, Type)                               \
    VMAP_DECLARE_FLAT_SET_TYPE(hashset##_type, Type, (_, _));                  \
    VMAP_DECLARE_HASHSET_WITH(hashset, Type, hashset##_type)

#define VMAP_DECLARE_FLAT_HASHMAP(hashmap, K, V)                               \
    VMAP_DECLARE_FLAT_MAP_TYPE(hashmap##_type, K, V, (_, _));                  \
    VMAP_DECLARE_HASHMAP_WITH(hashmap, K, V, hashmap##_type)

#define VMAP_DECLARE_HASHSET_WITH(hashset, Type, vtype)                        \
    typedef Type hashset##_entry;                                              \
    typedef Type hashset##_key;                                                \
    VMAP_DECLARE_COMMON(hashset, hashset##_entry, hashset##_key, vtype)

#define VMAP_DECLARE_HASHMAP_WITH(hashmap, K, V, vtype)                        \
    typedef struct {                                                           \
        K key;                                                                 \
        V value;                                                               \
    } hashmap##_entry;                                                         \
    typedef K hashmap##_key;                                                   \
    VMAP_DECLARE_COMMON(hashmap, hashmap##_entry, hashmap##_key, vtype)

#define VMAP_DECLARE_COMMON(hashset, Type, Key, vtype)                         \
    static inline const vmap_type* hashset##_type_get(void) { return &vtype; } \
    typedef struct {                                                           \
        vmap_raw_map set;                                                      \
    } hashset;                                                                 \
    static inline hashset hashset##_new(size_t bucket_count) {                 \
        return (hashset){vmap_raw_map_new(&vtype, bucket_count)};              \
    }                                                                          \
    static inline void hashset##_destroy(hashset* self) {                      \
        vmap_raw_map_destroy(&vtype, &self->set);                              \
    }                                                                          \
    typedef struct {                                                           \
        vmap_raw_iter it;                                                      \
    } hashset##_iter;                                                          \
    static inline hashset##_iter hashset##_iter_new(hashset* self) {           \
        return (hashset##_iter){vmap_raw_map_iter(&vtype, &self->set)};        \
    }                                                                          \
    static inline Type* hashset##_iter_get(const hashset##_iter* it) {         \
        return (Type*)vmap_raw_iter_get(&vtype, &it->it);                      \
    }                                                                          \
    static inline Type* hashset##_iter_next(hashset##_iter* it) {              \
        return (Type*)vmap_raw_iter_next(&vtype, &it->it);                     \
    }                                                                          \
    static inline void hashset##_reserve(hashset* self, size_t n) {            \
        vmap_raw_map_reserve(&vtype, &self->set, n);                           \
    }                                                                          \
    static inline void hashset##_rehash(hashset* self, size_t n) {             \
        vmap_raw_map_rehash(&vtype, &self->set, n);                            \
    }                                                                          \
    static inline size_t hashset##_size(const hashset* self) {                 \
        return vmap_raw_map_size(&vtype, &self->set);                          \
    }                                                                          \
    static inline size_t hashset##_capacity(const hashset* self) {             \
        return vmap_raw_map_capacity(&vtype, &self->set);                      \
    }                                                                          \
    static inline void hashset##_clear(hashset* self) {                        \
        return vmap_raw_map_clear(&vtype, &self->set);                         \
    }                                                                          \
    static inline bool hashset##_empty(const hashset* self) {                  \
        return vmap_raw_map_empty(&vtype, &self->set);                         \
    }                                                                          \
    static inline bool hashset##_insert(hashset* self, const Key* key) {       \
        return vmap_raw_map_insert(&vtype, &self->set, key);                   \
    }                                                                          \
    static inline const void* hashset##_find(const hashset* self,              \
                                             const Key* key) {                 \
        return vmap_raw_map_find(&vtype, vtype.key, &self->set, key);          \
    }                                                                          \
    static inline bool hashset##_contains(const hashset* self,                 \
                                          const Key* key) {                    \
        return vmap_raw_map_contains(&vtype, vtype.key, &self->set, key);      \
    }                                                                          \
    static inline void hashset##_erase_at(hashset##_iter it) {                 \
        vmap_raw_map_erase_at(&vtype, it.it);                                  \
    }                                                                          \
    static inline bool hashset##_erase(hashset* self, const void* key) {       \
        return vmap_raw_map_erase(&vtype, vtype.key, &self->set, key);         \
    }

#endif /* __VMAP_H__ */
