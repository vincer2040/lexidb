#include "../src/vmap.h"

size_t get_hash_table_debug_num_probes(const vmap_type* type, const vmap_raw_map* self, const void* key) {
    size_t num_probes = 0;
    size_t hash = type->key->hash(key);
    auto seq = vmap_probe_seq_start(self->ctrl, hash, self->capacity);

    while (true) {
        auto g = vmap_group_new(self->ctrl + seq.offset);
        auto match = vmap_group_match(&g, vmap_h2(hash));

        uint32_t i;

        while (vmap_bitmask_next(&match, &i)) {
            size_t idx = vmap_probe_seq_offset(&seq, i);
            char* slot = self->slots + idx * type->slot->size;
            if (VMAP_LIKELY(type->key->eq(slot, key))) {
                return num_probes;
            }
            ++num_probes;
        }

        if (VMAP_LIKELY(vmap_group_match_empty(&g).mask)) {
            return num_probes;
        }

        vmap_probe_seq_next(&seq);
        ++num_probes;
    }
}
