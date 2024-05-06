#ifndef __VMAP_DEBUG_H__

#define __VMAP_DEBUG_H__

#include "../src/vmap.h"
#include <stddef.h>

size_t get_hash_table_debug_num_probes(const vmap_type* type,
                                       const vmap_raw_map* self,
                                       const void* key);

#endif /* __VMAP_DEBUG_H__ */
