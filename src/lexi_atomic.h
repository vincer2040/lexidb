#ifndef __LEXI_ATOMIC_H__

#define __LEXI_ATOMIC_H__

#include <stdatomic.h>

#define LEXI_ATOMIC_T(Type) _Atomic(Type)

#define LEXI_ATOMIC_INT(val) atomic_fetch_add_explicit(&(val), 1, memory_order_relaxed);

#endif /* __LEXI_ATOMIC_H__ */
