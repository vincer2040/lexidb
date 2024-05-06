#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <deque>
#include <unordered_set>
#include "../src/vmap.h"
#include "test_helpers.h"
#include "debug.h"


namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::IsEmpty;
using ::testing::Lt;
using ::testing::UnorderedElementsAre;

TEST(Util, NormalizeCapacity) {
    EXPECT_EQ(1, vmap_normalize_capacity(0));
    EXPECT_EQ(1, vmap_normalize_capacity(1));
    EXPECT_EQ(3, vmap_normalize_capacity(2));
    EXPECT_EQ(3, vmap_normalize_capacity(3));
    EXPECT_EQ(7, vmap_normalize_capacity(4));
    EXPECT_EQ(7, vmap_normalize_capacity(7));
    EXPECT_EQ(15, vmap_normalize_capacity(8));
    EXPECT_EQ(15, vmap_normalize_capacity(15));
    EXPECT_EQ(15 * 2 + 1, vmap_normalize_capacity(15 + 1));
    EXPECT_EQ(15 * 2 + 1, vmap_normalize_capacity(15 + 2));
}

TEST(Util, GrowthAndCapacity) {
    for (size_t growth = 0; growth < 10000; ++growth) {
        SCOPED_TRACE(growth);
        size_t capacity = vmap_normalize_capacity(
            vmap_growth_to_lower_bound_capacity(growth));
        EXPECT_THAT(vmap_capacity_to_growth(capacity), Ge(growth));
        if (capacity + 1 < VMAP_GROUP_WIDTH) {
            EXPECT_THAT(vmap_capacity_to_growth(capacity), Eq(capacity));
        } else {
            EXPECT_THAT(vmap_capacity_to_growth(capacity), Lt(capacity));
        }
        if (growth != 0 && capacity > 1) {
            EXPECT_THAT(vmap_capacity_to_growth(capacity / 2), Lt(growth));
        }
    }

    for (size_t capacity = VMAP_GROUP_WIDTH - 1; capacity < 10000;
         capacity = 2 * capacity + 1) {
        SCOPED_TRACE(capacity);
        size_t growth = vmap_capacity_to_growth(capacity);
        EXPECT_THAT(growth, Lt(capacity));
        EXPECT_LE(vmap_growth_to_lower_bound_capacity(growth), capacity);
        EXPECT_EQ(vmap_normalize_capacity(
                      vmap_growth_to_lower_bound_capacity(growth)),
                  capacity);
    }
}

TEST(Util, probe_seq) {
    vmap_probe_seq seq;
    std::vector<size_t> offsets(8);
    auto gen = [&]() {
        size_t res = vmap_probe_seq_offset(&seq, 0);
        vmap_probe_seq_next(&seq);
        return res;
    };

    std::vector<size_t> expected;
    if (VMAP_GROUP_WIDTH == 16) {
        expected = {0, 16, 48, 96, 32, 112, 80, 64};
    } else if (VMAP_GROUP_WIDTH == 8) {
        expected = {0, 8, 24, 48, 80, 120, 40, 96};
    } else {
        FAIL() << "No test coverage for VMAP_GROUP_WIDTH == "
               << VMAP_GROUP_WIDTH;
    }

    seq = vmap_probe_seq_new(0, 127);
    std::generate_n(offsets.begin(), 8, gen);
    EXPECT_EQ(offsets, expected);

    seq = vmap_probe_seq_new(128, 127);
    std::generate_n(offsets.begin(), 8, gen);
    EXPECT_EQ(offsets, expected);
}

template <size_t Width, size_t Shift = 0>
vmap_bitmask make_mask(uint64_t mask) {
    return {mask, Width, Shift};
}

std::vector<uint32_t> mask_bits(vmap_bitmask mask) {
    std::vector<uint32_t> v;
    uint32_t x;
    while (vmap_bitmask_next(&mask, &x)) {
        v.push_back(x);
    }
    return v;
}

TEST(BitMask, Smoke) {
    EXPECT_THAT(mask_bits(make_mask<8>(0)), ElementsAre());
    EXPECT_THAT(mask_bits(make_mask<8>(0x1)), ElementsAre(0));
    EXPECT_THAT(mask_bits(make_mask<8>(0x2)), ElementsAre(1));
    EXPECT_THAT(mask_bits(make_mask<8>(0x3)), ElementsAre(0, 1));
    EXPECT_THAT(mask_bits(make_mask<8>(0x4)), ElementsAre(2));
    EXPECT_THAT(mask_bits(make_mask<8>(0x5)), ElementsAre(0, 2));
    EXPECT_THAT(mask_bits(make_mask<8>(0x55)), ElementsAre(0, 2, 4, 6));
    EXPECT_THAT(mask_bits(make_mask<8>(0xAA)), ElementsAre(1, 3, 5, 7));
}

TEST(BitMask, WithShift) {
    uint64_t ctrl = 0x1716151413121110;
    uint64_t hash = 0x12;
    constexpr uint64_t msbs = 0x8080808080808080ULL;
    constexpr uint64_t lsbs = 0x0101010101010101ULL;
    auto x = ctrl ^ (lsbs * hash);
    uint64_t mask = (x - lsbs) & ~x & msbs;
    EXPECT_EQ(0x0000000080800000, mask);

    auto b = make_mask<8, 3>(mask);
    EXPECT_EQ(vmap_bitmask_lowest_bit_set(&b), 2);
}

uint32_t mask_leading(vmap_bitmask mask) {
    return vmap_bitmask_leading_zeros(&mask);
}

uint32_t mask_trailing(vmap_bitmask mask) {
    return vmap_bitmask_trailing_zeros(&mask);
}

TEST(BitMask, LeadingTrailing) {
    EXPECT_EQ(mask_leading(make_mask<16>(0x00001a40)), 3);
    EXPECT_EQ(mask_trailing(make_mask<16>(0x00001a40)), 6);

    EXPECT_EQ(mask_leading(make_mask<16>(0x00000001)), 15);
    EXPECT_EQ(mask_trailing(make_mask<16>(0x00000001)), 0);

    EXPECT_EQ(mask_leading(make_mask<16>(0x00008000)), 0);
    EXPECT_EQ(mask_trailing(make_mask<16>(0x00008000)), 15);

    EXPECT_EQ(mask_leading(make_mask<8, 3>(0x0000008080808000)), 3);
    EXPECT_EQ(mask_trailing(make_mask<8, 3>(0x0000008080808000)), 1);

    EXPECT_EQ(mask_leading(make_mask<8, 3>(0x0000000000000080)), 7);
    EXPECT_EQ(mask_trailing(make_mask<8, 3>(0x0000000000000080)), 0);

    EXPECT_EQ(mask_leading(make_mask<8, 3>(0x8000000000000000)), 0);
    EXPECT_EQ(mask_trailing(make_mask<8, 3>(0x8000000000000000)), 7);
}

std::vector<uint32_t> group_match(const vmap_control_byte* group, vmap_h2_t h) {
    auto g = vmap_group_new(group);
    return mask_bits(vmap_group_match(&g, h));
}

std::vector<uint32_t> group_match_empty(const vmap_control_byte* group) {
    auto g = vmap_group_new(group);
    return mask_bits(vmap_group_match_empty(&g));
}

std::vector<uint32_t>
group_match_empty_or_deleted(const vmap_control_byte* group) {
    auto g = vmap_group_new(group);
    return mask_bits(vmap_group_match_empty_or_deleted(&g));
}

TEST(Group, EmptyGroup) {
    for (vmap_h2_t h = 0; h != 128; ++h) {
        EXPECT_THAT(group_match(vmap_empty_group(), h), IsEmpty());
    }
}

vmap_control_byte control(int i) { return static_cast<vmap_control_byte>(i); }

TEST(Group, Match) {
    if (VMAP_GROUP_WIDTH == 16) {
        vmap_control_byte group[] = {
            VMAP_EMPTY, control(1), VMAP_DELETED,  control(3),
            VMAP_EMPTY, control(5), VMAP_SENTINEL, control(7),
            control(7), control(5), control(3),    control(1),
            control(1), control(1), control(1),    control(1),
        };
        EXPECT_THAT(group_match(group, 0), ElementsAre());
        EXPECT_THAT(group_match(group, 1), ElementsAre(1, 11, 12, 13, 14, 15));
        EXPECT_THAT(group_match(group, 3), ElementsAre(3, 10));
        EXPECT_THAT(group_match(group, 5), ElementsAre(5, 9));
        EXPECT_THAT(group_match(group, 7), ElementsAre(7, 8));
    } else if (VMAP_GROUP_WIDTH == 8) {
        vmap_control_byte group[] = {
            VMAP_EMPTY, control(1), control(2),    VMAP_DELETED,
            control(2), control(1), VMAP_SENTINEL, control(1),
        };
        EXPECT_THAT(group_match(group, 0), ElementsAre());
        EXPECT_THAT(group_match(group, 1), ElementsAre(1, 5, 7));
        EXPECT_THAT(group_match(group, 2), ElementsAre(2, 4));
    } else {
        FAIL() << "No test coverage for CWISS_Group_kWidth == "
               << VMAP_GROUP_WIDTH;
    }
}

TEST(Group, MatchEmpty) {
    if (VMAP_GROUP_WIDTH == 16) {
        vmap_control_byte group[] = {
            VMAP_EMPTY, control(1), VMAP_DELETED,  control(3),
            VMAP_EMPTY, control(5), VMAP_SENTINEL, control(7),
            control(7), control(5), control(3),    control(1),
            control(1), control(1), control(1),    control(1),
        };
        EXPECT_THAT(group_match_empty(group), ElementsAre(0, 4));
    } else if (VMAP_GROUP_WIDTH == 8) {
        vmap_control_byte group[] = {
            VMAP_EMPTY, control(1), control(2),    VMAP_DELETED,
            control(2), control(1), VMAP_SENTINEL, control(1),
        };
        EXPECT_THAT(group_match_empty(group), ElementsAre(0));
    } else {
        FAIL() << "No test coverage for VMAP_GROUP_WIDTH == "
               << VMAP_GROUP_WIDTH;
    }
}

TEST(Group, MatchEmptyOrDeleted) {
    if (VMAP_GROUP_WIDTH == 16) {
        vmap_control_byte group[] = {
            VMAP_EMPTY, control(1), VMAP_DELETED,  control(3),
            VMAP_EMPTY, control(5), VMAP_SENTINEL, control(7),
            control(7), control(5), control(3),    control(1),
            control(1), control(1), control(1),    control(1),
        };
        EXPECT_THAT(group_match_empty_or_deleted(group), ElementsAre(0, 2, 4));
    } else if (VMAP_GROUP_WIDTH == 8) {
        vmap_control_byte group[] = {
            VMAP_EMPTY, control(1), control(2),    VMAP_DELETED,
            control(2), control(1), VMAP_SENTINEL, control(1),
        };
        EXPECT_THAT(group_match_empty_or_deleted(group), ElementsAre(0, 3));
    } else {
        FAIL() << "No test coverage for VMAP_GROUP_WIDTH == "
               << VMAP_GROUP_WIDTH;
    }
}

TEST(Batch, DropDeletes) {
    constexpr size_t capacity = 63;
    constexpr size_t group_width = VMAP_GROUP_WIDTH;

    std::vector<vmap_control_byte> ctrl(capacity + 1 + group_width);
    ctrl[capacity] = VMAP_SENTINEL;

    std::vector<vmap_control_byte> pattern = {
        VMAP_EMPTY, control(2), VMAP_DELETED, control(2),
        VMAP_EMPTY, control(1), VMAP_DELETED,
    };
    for (size_t i = 0; i != capacity; ++i) {
        ctrl[i] = pattern[i % pattern.size()];
        if (i < group_width - 1) {
            ctrl[i + capacity + 1] = pattern[i % pattern.size()];
        }
    }

    vmap_convert_deleted_to_empty_and_full_to_deleted(ctrl.data(), capacity);
    ASSERT_EQ(ctrl[capacity], VMAP_SENTINEL);

    for (size_t i = 0; i < capacity + group_width; ++i) {
        vmap_control_byte expected =
            pattern[i % (capacity + 1) % pattern.size()];
        if (i == capacity) {
            expected = VMAP_SENTINEL;
        }
        if (expected == VMAP_DELETED) {
            expected = VMAP_EMPTY;
        }
        if (vmap_is_full(expected)) {
            expected = VMAP_DELETED;
        }

        EXPECT_EQ(ctrl[i], expected);
    }
}

TEST(Group, CountLeadingEmptyOrDeleted) {
    const std::vector<vmap_control_byte> empty_examples = {VMAP_EMPTY,
                                                           VMAP_DELETED};
    const std::vector<vmap_control_byte> full_examples = {
        control(0), control(1), control(2),   control(3),
        control(5), control(9), control(127), VMAP_SENTINEL,
    };

    for (vmap_control_byte empty : empty_examples) {
        std::vector<vmap_control_byte> e(VMAP_GROUP_WIDTH, empty);
        auto g = vmap_group_new(e.data());
        EXPECT_EQ(VMAP_GROUP_WIDTH,
                  vmap_group_count_leading_empty_or_deleted(&g));

        for (vmap_control_byte full : full_examples) {
            for (size_t i = 0; i != VMAP_GROUP_WIDTH; ++i) {
                std::vector<vmap_control_byte> f(VMAP_GROUP_WIDTH, empty);
                f[i] = full;

                auto g = vmap_group_new(f.data());
                EXPECT_EQ(i, vmap_group_count_leading_empty_or_deleted(&g));
            }

            std::vector<vmap_control_byte> f(VMAP_GROUP_WIDTH, empty);
            f[VMAP_GROUP_WIDTH * 2 / 3] = full;
            f[VMAP_GROUP_WIDTH / 2] = full;

            auto g = vmap_group_new(f.data());
            EXPECT_EQ(VMAP_GROUP_WIDTH / 2,
                      vmap_group_count_leading_empty_or_deleted(&g));
        }
    }
}

VMAP_DECLARE_HASHSET_WITH(int_map, int64_t, FlatPolicy<int64_t>());

MAP_HELPERS(int_map);

struct BadHash {
    size_t operator()(const int&) { return 0; }
};

VMAP_DECLARE_HASHSET_WITH(bad, int, (FlatPolicy<int, BadHash>()));

MAP_HELPERS(bad);

TEST(Table, Empty) {
    auto t = int_map_new(0);
    EXPECT_EQ(int_map_size(&t), 0);
    EXPECT_TRUE(int_map_empty(&t));
    int_map_destroy(&t);
}

TEST(Table, LookupEmpty) {
    auto t = int_map_new(0);
    EXPECT_FALSE(find(t, 0));
    int_map_destroy(&t);
}

TEST(Table, Insert1) {
    auto t = int_map_new(0);

    EXPECT_FALSE(find(t, 0));
    auto inserted = insert(t, 0);
    EXPECT_TRUE(inserted);

    EXPECT_EQ(int_map_size(&t), 1);

    auto f = find(t, 0);
    EXPECT_TRUE(f);
    EXPECT_EQ(*f, 0);

    int_map_destroy(&t);
}

TEST(Table, Insert2) {
    auto t = int_map_new(0);

    EXPECT_FALSE(find(t, 0));

    EXPECT_TRUE(insert(t, 0));
    EXPECT_EQ(int_map_size(&t), 1);

    EXPECT_FALSE(find(t, 1));

    EXPECT_TRUE(insert(t, 1));
    EXPECT_EQ(int_map_size(&t), 2);

    auto f1 = find(t, 0);
    auto f2 = find(t, 1);
    EXPECT_TRUE(f1);
    EXPECT_TRUE(f2);
    EXPECT_EQ(*f1, 0);
    EXPECT_EQ(*f2, 1);

    int_map_destroy(&t);
}

TEST(Table, InsertCollision) {
    auto t = bad_new(0);

    EXPECT_FALSE(find(t, 0));

    EXPECT_TRUE(insert(t, 0));
    EXPECT_EQ(bad_size(&t), 1);

    EXPECT_FALSE(find(t, 1));

    EXPECT_TRUE(insert(t, 1));
    EXPECT_EQ(bad_size(&t), 2);

    auto f1 = find(t, 0);
    auto f2 = find(t, 1);
    EXPECT_TRUE(f1);
    EXPECT_TRUE(f2);
    EXPECT_EQ(*f1, 0);
    EXPECT_EQ(*f2, 1);

    bad_destroy(&t);
}

TEST(Table, InsertCollisionAndFindAfterDelete) {
    auto t = bad_new(0);

    constexpr size_t num_inserts = VMAP_GROUP_WIDTH * 2 + 5;
    for (size_t i = 0; i < num_inserts; ++i) {
        EXPECT_TRUE(insert(t, i));
        EXPECT_EQ(bad_size(&t), i + 1);
    }

    for (size_t i = 0; i < num_inserts; ++i) {
        EXPECT_TRUE(erase(t, i)) << i;

        for (size_t j = i + 1; j < num_inserts; ++j) {
            auto f = find(t, j);
            EXPECT_TRUE(f);
            EXPECT_EQ(*f, j);

            EXPECT_FALSE(insert(t, j));
            EXPECT_EQ(bad_size(&t), num_inserts - i - 1);
        }
    }

    EXPECT_TRUE(bad_empty(&t));

    bad_destroy(&t);
}

TEST(Table, InsertWithCapacity) {
    auto t = int_map_new(0);

    int_map_reserve(&t, 10);

    size_t original_capacity = int_map_capacity(&t);

    const auto addr = [&](int i) {
        return reinterpret_cast<uintptr_t>(find(t, i));
    };

    insert(t, 0);

    EXPECT_THAT(int_map_capacity(&t), original_capacity);
    const uintptr_t original_addr_0 = addr(0);

    insert(t, 0);
    EXPECT_THAT(int_map_capacity(&t), original_capacity);
    EXPECT_THAT(addr(0), original_addr_0);

    for (int i = 0; i < 100; ++i) {
        insert(t, i % 10);
    }

    EXPECT_THAT(int_map_capacity(&t), original_capacity);
    EXPECT_THAT(addr(0), original_addr_0);
    int_map_destroy(&t);
}

TEST(Table, ContainsEmpty) {
    auto t = int_map_new(0);
    int64_t k0 = 0;
    EXPECT_FALSE(int_map_contains(&t, &k0));
    int_map_destroy(&t);
}

TEST(Table, Contains1) {
    auto t = int_map_new(0);
    int64_t k0 = 0, k1 = 1;
    EXPECT_TRUE(insert(t, k0));
    EXPECT_TRUE(int_map_contains(&t, &k0));
    EXPECT_FALSE(int_map_contains(&t, &k1));
    EXPECT_TRUE(erase(t, k0));
    EXPECT_FALSE(int_map_contains(&t, &k0));
    int_map_destroy(&t);
}

TEST(Table, Contains2) {
    auto t = int_map_new(0);
    int64_t k0 = 0, k1 = 1;
    EXPECT_TRUE(insert(t, k0));
    EXPECT_TRUE(int_map_contains(&t, &k0));
    EXPECT_FALSE(int_map_contains(&t, &k1));

    int_map_clear(&t);

    EXPECT_FALSE(int_map_contains(&t, &k0));
    int_map_destroy(&t);
}

size_t max_density_size(size_t n) {
    auto t = int_map_new(n);

    for (size_t i = 0; i != n; ++i) {
        insert(t, i);
    }

    const size_t c = int_map_capacity(&t);
    while (c == int_map_capacity(&t)) {
        insert(t, n++);
    }

    size_t capacity = int_map_capacity(&t);

    int_map_destroy(&t);

    return capacity;
}

struct Modulo1000Hash {
    size_t operator()(const int& p) { return p % 1000; }
};

VMAP_DECLARE_HASHSET_WITH(modulo1000_hash_set, int,
                          (FlatPolicy<int, Modulo1000Hash>()));

MAP_HELPERS(modulo1000_hash_set);

TEST(Table, RehashWithNoResize) {
    auto t = modulo1000_hash_set_new(0);

    const size_t min_full_groups = 7;
    std::vector<int> keys;
    for (size_t i = 0; i < max_density_size(VMAP_GROUP_WIDTH * min_full_groups);
         ++i) {
        int k = i * 1000;
        insert(t, k);
        keys.push_back(k);
    }

    const size_t capacity = modulo1000_hash_set_capacity(&t);

    const size_t erase_begin = VMAP_GROUP_WIDTH / 2;
    const size_t erase_end =
        (modulo1000_hash_set_size(&t) / VMAP_GROUP_WIDTH - 1) *
        VMAP_GROUP_WIDTH;
    for (size_t i = erase_begin; i < erase_end; ++i) {
        EXPECT_TRUE(erase(t, keys[i])) << i;
    }

    keys.erase(keys.begin() + erase_begin, keys.begin() + erase_end);

    auto last_key = keys.back();

    auto probes = [&] {
        return get_hash_table_debug_num_probes(modulo1000_hash_set_type_get(),
                                               &t.set, &last_key);
    };

    size_t last_key_num_probes = probes();

    ASSERT_GE(last_key_num_probes, min_full_groups);

    int x = 1;

    while (last_key_num_probes == probes()) {
        insert(t, x);
        ASSERT_EQ(modulo1000_hash_set_capacity(&t), capacity);

        ASSERT_TRUE(find(t, x)) << x;

        for (const auto& k : keys) {
            ASSERT_TRUE(find(t, k)) << k;
        }

        erase(t, x++);
    }

    modulo1000_hash_set_destroy(&t);
}

TEST(Table, InsertEraseStressTest) {
    auto t = int_map_new(0);
    const size_t min_element_count = 250;
    std::deque<int> keys;
    size_t i = 0;
    for (; i < max_density_size(min_element_count); ++i) {
        insert(t, i);
        keys.push_back(i);
    }
    const size_t num_iterations = 1000000;
    for (; i < num_iterations; ++i) {
        int64_t k = keys.front();
        ASSERT_TRUE(erase(t, keys.front())) << keys.front();
        keys.pop_front();
        insert(t, i);
        keys.push_back(i);
    }

    int_map_destroy(&t);
}

TEST(Table, LargeTable) {
    auto t = int_map_new(0);

    for (int i = 0; i < 100000; ++i) {
        insert(t, i << 20);
    }

    for (int i = 0; i < 100000; ++i) {
        auto f = find(t, i << 20);
        ASSERT_TRUE(f);
        ASSERT_EQ(*f, i << 20);
    }

    int_map_destroy(&t);
}

TEST(Table, EnsureNonQuadratic) {
    static const size_t large_size = 1 << 15;

    auto t1 = int_map_new(0);

    for (size_t i = 0; i != large_size; ++i) {
        insert(t1, i);
    }

    auto t2 = int_map_new(0);

    for (auto it = int_map_iter_new(&t1); int_map_iter_get(&it);
         int_map_iter_next(&it)) {
        int_map_insert(&t2, int_map_iter_get(&it));
    }

    int_map_destroy(&t1);
    int_map_destroy(&t2);
}

TEST(Table, ClearBug) {
    auto t = int_map_new(0);

    constexpr size_t capacity = VMAP_GROUP_WIDTH - 1;
    constexpr size_t max_size = capacity / 2 + 1;

    for (size_t i = 0; i < max_size; ++i) {
        insert(t, i);
    }

    ASSERT_EQ(int_map_capacity(&t), capacity);

    intptr_t original = reinterpret_cast<intptr_t>(find(t, 2));

    int_map_clear(&t);
    ASSERT_EQ(int_map_capacity(&t), capacity);

    for (size_t i = 0; i < max_size; ++i) {
        insert(t, i);
    }

    ASSERT_EQ(int_map_capacity(&t), capacity);

    intptr_t second = reinterpret_cast<intptr_t>(find(t, 2));

    EXPECT_LT(std::abs(original - second), capacity * sizeof(int64_t));

    int_map_destroy(&t);
}

TEST(Table, Erase) {
    auto t = int_map_new(0);

    EXPECT_FALSE(find(t, 0));

    EXPECT_TRUE(insert(t, 0));
    EXPECT_EQ(int_map_size(&t), 1);

    erase(t, 0);

    EXPECT_FALSE(find(t, 0));

    int_map_destroy(&t);
}

TEST(Table, EraseMaintainsValidIterator) {
    auto t = int_map_new(0);

    const int num_elements = 100;
    for (int i = 0; i < num_elements; ++i) {
        EXPECT_TRUE(insert(t, i));
    }

    EXPECT_EQ(int_map_size(&t), num_elements);

    int num_erase_calls = 0;
    auto it = int_map_iter_new(&t);

    while (int_map_iter_get(&it)) {
        auto prev = it;
        int_map_iter_next(&it);
        int_map_erase_at(prev);
        ++num_erase_calls;
    }

    EXPECT_TRUE(int_map_empty(&t));
    EXPECT_EQ(num_erase_calls, num_elements);

    int_map_destroy(&t);
}

TEST(Table, EraseCollision) {
    auto t = bad_new(0);

    insert(t, 1);
    insert(t, 2);
    insert(t, 3);

    auto f1 = find(t, 1);
    auto f2 = find(t, 2);
    auto f3 = find(t, 3);

    EXPECT_TRUE(f1);
    EXPECT_TRUE(f2);
    EXPECT_TRUE(f3);

    EXPECT_EQ(*f1, 1);
    EXPECT_EQ(*f2, 2);
    EXPECT_EQ(*f3, 3);

    erase(t, 2);
    f1 = find(t, 1);
    EXPECT_TRUE(f1);
    EXPECT_EQ(*f1, 1);
    EXPECT_FALSE(find(t, 2));
    f3 = find(t, 3);
    EXPECT_TRUE(f3);
    EXPECT_EQ(*f3, 3);
    EXPECT_EQ(bad_size(&t), 2);

    erase(t, 1);
    EXPECT_FALSE(find(t, 1));
    EXPECT_FALSE(find(t, 2));
    f3 = find(t, 3);
    EXPECT_TRUE(f3);
    EXPECT_EQ(*f3, 3);
    EXPECT_EQ(bad_size(&t), 1);

    erase(t, 3);
    EXPECT_FALSE(find(t, 1));
    EXPECT_FALSE(find(t, 2));
    EXPECT_FALSE(find(t, 3));
    EXPECT_EQ(bad_size(&t), 0);

    bad_destroy(&t);
}

TEST(Table, EraseInsertProbing) {
    auto t = bad_new(0);

    insert(t, 1);
    insert(t, 2);
    insert(t, 3);
    insert(t, 4);

    erase(t, 2);
    erase(t, 4);

    insert(t, 10);
    insert(t, 11);
    insert(t, 12);

    EXPECT_EQ(bad_size(&t), 5);

    EXPECT_THAT(collect(t), UnorderedElementsAre(1, 10, 3, 11, 12));

    bad_destroy(&t);
}

TEST(Table, Clear) {
    auto t = int_map_new(0);

    EXPECT_FALSE(find(t, 0));

    int_map_clear(&t);
    EXPECT_FALSE(find(t, 0));

    EXPECT_TRUE(insert(t, 0));
    EXPECT_EQ(int_map_size(&t), 1);

    int_map_clear(&t);

    EXPECT_EQ(int_map_size(&t), 0);
    EXPECT_FALSE(find(t, 0));

    int_map_destroy(&t);
}

TEST(Table, Rehash) {
    auto t = int_map_new(0);

    EXPECT_FALSE(find(t, 0));

    insert(t, 0);
    insert(t, 1);

    EXPECT_EQ(int_map_size(&t), 2);

    int_map_rehash(&t, 128);

    EXPECT_EQ(int_map_size(&t), 2);

    auto f1 = find(t, 0);
    auto f2 = find(t, 1);
    EXPECT_TRUE(f1);
    EXPECT_EQ(*f1, 0);
    EXPECT_TRUE(f2);
    EXPECT_EQ(*f2, 1);

    int_map_destroy(&t);
}

TEST(Table, RehashDoesNotRehashWhenNotNecessary) {
    auto t = int_map_new(0);

    insert(t, 0);
    insert(t, 1);

    auto* p = find(t, 0);
    int_map_rehash(&t, 1);
    EXPECT_EQ(p, find(t, 0));

    int_map_destroy(&t);
}

TEST(Table, RehashZeroDoesNotAllocateOnEmptyTable) {
    auto t = int_map_new(0);

    int_map_rehash(&t, 0);

    EXPECT_EQ(int_map_capacity(&t), 0);

    int_map_destroy(&t);
}

TEST(Table, RehashzeroDeallocatesEmptyTable) {
    auto t = int_map_new(0);

    insert(t, 0);
    int_map_clear(&t);
    EXPECT_NE(int_map_capacity(&t), 0);
    int_map_rehash(&t, 0);
    EXPECT_EQ(int_map_capacity(&t), 0);

    int_map_destroy(&t);
}

TEST(Table, RehashZeroForcesRehash) {
    auto t = int_map_new(0);
    insert(t, 0);
    insert(t, 1);
    auto* p = find(t, 0);
    int_map_rehash(&t, 1);
    EXPECT_EQ(p, find(t, 0));
    int_map_destroy(&t);
}

TEST(Table, NumDeletedRegression) {
    auto t = int_map_new(0);
    insert(t, 0);
    erase(t, 0);
    insert(t, 0);
    int_map_clear(&t);
    int_map_destroy(&t);
}

TEST(Table, FindFullDeletedRegression) {
    auto t = int_map_new(0);

    for (int i = 0; i < 1000; ++i) {
        insert(t, i);
        erase(t, i);
    }

    EXPECT_EQ(int_map_size(&t), 0);

    int_map_destroy(&t);
}

TEST(Table, ReplacingDeletedSlotDoesNotRehash) {
    size_t n = max_density_size(1);
    auto t = int_map_new(n);
    const size_t c = int_map_capacity(&t);
    for (size_t i = 0; i != n; ++i) {
        insert(t, i);
    }

    EXPECT_EQ(int_map_capacity(&t), c) << "rehashing threshhold = " << n;
    erase(t, 0);
    insert(t, 0);
    EXPECT_EQ(int_map_capacity(&t), c) << "rehashing threshhold = " << n;

    int_map_destroy(&t);
}

TEST(Table, Iterates) {
    auto t = int_map_new(0);
    for (size_t i = 3; i != 6; ++i) {
        EXPECT_TRUE(insert(t, i));
    }
    EXPECT_THAT(collect(t), UnorderedElementsAre(3, 4, 5));
    int_map_destroy(&t);
}

int_map make_simple_table(size_t size) {
    auto t = int_map_new(0);
    for (size_t i = 0; i < size; ++i) {
        insert(t, i);
    }
    return t;
}

TEST(Table, IterationOrderChangesByInstance) {
    for (size_t size : {2, 6, 12, 20}) {
        auto reference_table = make_simple_table(size);
        auto reference = collect(reference_table);
        std::vector<int_map> tables;

        bool found_difference = false;
        for (int i = 0; !found_difference && i < 5000; ++i) {
            tables.push_back(make_simple_table(size));
            found_difference = collect(tables.back()) != reference;
        }

        if (!found_difference) {
            FAIL() << "Iteration order remained the same across many attempts "
                      "with size "
                   << size;
        }

        int_map_destroy(&reference_table);
        for (auto& tbl : tables) {
            int_map_destroy(&tbl);
        }
    }
}

TEST(Table, IterationORderChangesOnRehash) {
    std::vector<int_map> garbage;

    auto clean = [&] {
        for (auto& tbl : garbage) {
            int_map_destroy(&tbl);
        }
    };

    for (int i = 0; i < 5000; ++i) {
        garbage.push_back(make_simple_table(20));
        auto reference = collect(garbage.back());

        int_map_rehash(&garbage.back(), 0);

        auto trial = collect(garbage.back());

        if (trial != reference) {
            clean();
            return;
        }
    }

    clean();

    FAIL() << "Iteration order ramined the same across many attempts";
}

TEST(Table, UnstablePointers) {
    auto t = int_map_new(0);
    const auto addr = [&](int i) {
        return reinterpret_cast<uintptr_t>(find(t, i));
    };

    insert(t, 0);
    const uintptr_t old_ptr = addr(0);

    // this causes a rehash
    insert(t, 1);

    EXPECT_NE(old_ptr, addr(0));

    int_map_destroy(&t);
}

TEST(TableDeathTest, EraseOfEndAsserts) {
    bool assert_enabled = false;

    assert([&]() {
        assert_enabled = true;
        return true;
    }());
    if (!assert_enabled)
        return;

    auto t = int_map_new(0);
    auto it = int_map_iter_new(&t);
    constexpr char death_message[] = "Invalid operation";
    EXPECT_DEATH_IF_SUPPORTED(int_map_erase_at(it), death_message);
    int_map_destroy(&t);
}

#if VMAP_HAVE_ASAN
TEST(Sanitizer, PoisoningUnused) {
    auto t = int_map_new(0);

    int_map_reserve(&t, 5);

    insert(t, 0);
    auto cap = int_map_capacity(&t);

    ASSERT_GT(cap, 1);

    int64_t* slots = reinterpret_cast<int64_t*>(t.set.slots);

    for (size_t i = 0; i < cap; ++i) {
        int64_t* slot = slots + i;
        EXPECT_EQ(__asan_address_is_poisoned(slots + i), slot != v) << i;
    }

    int_map_destroy(&t);
}

TEST(Sanitizer, PoisoningOnErase) {
    auto t = int_map_new(0);

    insert(t, 0);
    auto v = find(t, 0);

    EXPECT_FALSE(__asan_address_is_poisoned(v));

    erase(t, 0);

    EXPECT_TRUIE(__asan_address_is_poisoned(v));

    int_map_destroy(&t);
}
#endif

VMAP_DECLARE_HASHSET_WITH(u8_map, uint8_t, FlatPolicy<uint8_t>());
MAP_HELPERS(u8_map);

TEST(Table, AlignOne) {
    auto t = u8_map_new(0);

    std::unordered_set<uint8_t> verifier;

    for (int64_t i = 0; i < 100000; ++i) {
        SCOPED_TRACE(i);
        const uint8_t u = (i * -i) & 0xFF;
        auto it = find(t, u);
        auto verifier_it = verifier.find(u);
        if (it == NULL) {
            ASSERT_EQ(verifier_it, verifier.end());
            insert(t, u);
            verifier.insert(u);
        } else {
            ASSERT_NE(verifier_it, verifier.end());
            erase(t, u);
            verifier.erase(verifier_it);
        }
    }

    EXPECT_EQ(u8_map_size(&t), verifier.size());

    for (uint8_t u : collect(t)) {
        EXPECT_EQ(verifier.count(u), 1);
    }

    u8_map_destroy(&t);
}

} // namespace
#include "test_helpers.h"
