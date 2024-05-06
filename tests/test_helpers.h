#ifndef __VMAP_TEST_HELPERS_H__

#define __VMAP_TEST_HELPERS_H__

#include "../src/vmap.h"
#include <algorithm>

template <typename T> struct DefaultHash {
    size_t operator()(const T& val) {
        vmap_absl_hash_state state = VMAP_ABSL_HASH_INIT;
        vmap_absl_hash_write(&state, &val, sizeof(T));
        return vmap_absl_hash_finish(state);
    }
};

struct HashStdString {
    template <typename S> size_t operator()(const S& s) {
        vmap_absl_hash_state state = VMAP_ABSL_HASH_INIT;
        size_t size = s.size();
        vmap_absl_hash_write(&state, &size, sizeof(size_t));
        vmap_absl_hash_write(&state, s.data(), s.size());
        return vmap_absl_hash_finish(state);
    }
};

template <typename T> struct DefaultEq {
    bool operator()(const T& a, const T& b) { return a == b; }
};

template <typename T, typename Hash, typename Eq> struct FlatPolicyWrapper {
    // clang-format off
  VMAP_DECLARE_FLAT_SET_TYPE(vtype, T,
    (modifiers, static constexpr),
    (obj_copy, [](void* dst, const void* src) {
      new (dst) T(*static_cast<const T*>(src));
    }),
    (obj_dtor, [](void* val) {
      static_cast<T*>(val)->~T();
    }),
    (key_hash, [](const void* val) {
      return Hash{}(*static_cast<const T*>(val));
    }),
    (key_eq, [](const void* a, const void* b) {
      return Eq{}(*static_cast<const T*>(a), *static_cast<const T*>(b));
    }),
    (slot_transfer, [](void* dst, void* src) {
      T* old = static_cast<T*>(src);
      new (dst) T(std::move(*old));
      old->~T();
    }));
    // clang-format on
};

template <typename T, typename Hash = DefaultHash<T>,
          typename Eq = DefaultEq<T>>
constexpr const vmap_type& FlatPolicy() {
    return FlatPolicyWrapper<T, Hash, Eq>::vtype;
}

template <typename K, typename V, typename Hash, typename Eq>
struct FlatMapPolicyWrapper {
    // clang-format off
  VMAP_DECLARE_FLAT_MAP_TYPE(vtype, K, V,
    (modifiers, static constexpr),
    (obj_copy, [](void* dst, const void* src) {
      new (dst) vtype_entry(*static_cast<const vtype_entry*>(src));
    }),
    (obj_dtor, [](void* val) {
      static_cast<vtype_entry*>(val)->~vtype_entry();
    }),
    (key_hash, [](const void* val) {
      return Hash{}(static_cast<const vtype_entry*>(val)->k);
    }),
    (key_eq, [](const void* a, const void* b) {
      return Eq{}(static_cast<const vtype_entry*>(a)->k,
                  static_cast<const vtype_entry*>(b)->k);
    }),
    (slot_transfer, [](void* dst, void* src) {
      vtype_entry* old = static_cast<vtype_entry*>(src);
      new (dst) vtype_entry(std::move(*old));
      old->~kPolicy_Entry();
    }));
    // clang-format on
};

template <typename K, typename V, typename Hash = DefaultHash<K>,
          typename Eq = DefaultEq<K>>
constexpr const vmap_type& FlatMapPolicy() {
    return FlatMapPolicyWrapper<K, V, Hash, Eq>::kPolicy;
}

#define MAP_HELPERS(hashset)                                                   \
    VMAP_INLINE_ALWAYS inline const hashset##_entry* find(                     \
        const hashset& set, const hashset##_key& needle) {                     \
        return static_cast<const hashset##_entry*>(                            \
            hashset##_find(&set, &needle));                                    \
    }                                                                          \
    VMAP_INLINE_ALWAYS inline bool insert(hashset& set,                        \
                                          const hashset##_key& key) {          \
        return hashset##_insert(&set, &key);                                   \
    }                                                                          \
    VMAP_INLINE_ALWAYS inline bool erase(hashset& set,                         \
                                         const hashset##_key& key) {           \
        return hashset##_erase(&set, &key);                                    \
    }                                                                          \
    VMAP_INLINE_ALWAYS inline std::vector<hashset##_entry> collect(            \
        hashset& set) {                                                        \
        std::vector<hashset##_entry> items;                                    \
        items.reserve(hashset##_size(&set));                                   \
        for (auto it = hashset##_iter_new(&set); hashset##_iter_get(&it);      \
             hashset##_iter_next(&it)) {                                       \
            items.push_back(*hashset##_iter_get(&it));                         \
        }                                                                      \
        return items;                                                          \
    }

#endif /* __VMAP_TEST_HELPERS_H__ */
