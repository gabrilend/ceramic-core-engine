/* src/015-large-value-heap.h — variable-size payload allocator.
 *
 * The large-value heap is a chunked arena: a linked list of malloc'd
 * chunks where each allocation bump-allocates within the current
 * chunk. When a chunk fills up, a new chunk is allocated and prepended.
 * Allocations are never individually freed — the entire heap is
 * destroyed at run end (same lifetime model as the slot store).
 *
 * Why a separate heap, not just bigger slot cells:
 *
 *   The slot store gives every cell a fixed cell_capacity decided at
 *   slot_alloc time. For values whose size is not known at compile
 *   time — long LLM responses, dynamically-sized arrays, binary
 *   blobs of unbounded length — there is no useful upper bound to
 *   bake into cell_capacity.
 *
 *   The large-value heap solves this by decoupling cell size from
 *   payload size. A slot allocated with SLOT_FLAG_LARGE_VALUE
 *   (issue 302) stores only a handle in its cell; the payload bytes
 *   live in the large-value heap and the cell carries a stable
 *   pointer to them. Push allocates a heap region of the requested
 *   size; pop follows the pointer.
 *
 * Why stable pointers matter: the heap may grow by appending new
 * chunks, but existing chunks never move, so a pointer returned by
 * lvh_alloc remains valid for the life of the heap. This is what
 * makes the design safe for concurrent producers and consumers —
 * the consumer's pointer doesn't get invalidated by a later
 * producer's allocation.
 *
 * Concurrency: lvh_alloc is thread-safe via an internal mutex.
 * Reads of returned pointers need no synchronization beyond what
 * the slot store already provides at the cell level.
 *
 * Cleanup: allocations are not individually freed. The heap is
 * monotonic-growth for the duration of a run. For long-running
 * graphs with iterators producing large values in a tight loop,
 * this means the heap grows in proportion to total bytes produced
 * — not in proportion to peak live bytes. That's a known trade-off;
 * a proper free-on-pop allocator is a future iteration. Track
 * total_allocated() to surface when it matters.
 */

#ifndef SORAMECH_LARGE_VALUE_HEAP_H
#define SORAMECH_LARGE_VALUE_HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* {{{ Type */
typedef struct lvh lvh_t;
/* }}} */

/* {{{ Lifecycle */
/* Construct a new heap.
 *
 *   default_chunk_size — bytes per new chunk when the heap grows.
 *                        Pass 0 to use the built-in default (64 KB).
 *
 * Returns NULL on allocation failure. */
lvh_t  *lvh_create(size_t default_chunk_size);

/* Free every chunk and the heap struct. Safe on NULL. After this
 * call, all pointers returned by lvh_alloc on this heap are
 * invalid. */
void    lvh_destroy(lvh_t *h);
/* }}} */

/* {{{ Allocation */
/* Allocate `size` bytes of unspecified contents. The returned
 * pointer is stable for the life of the heap. Returns NULL on
 * allocation failure (out of memory at the malloc layer; the
 * heap itself doesn't pre-cap).
 *
 * Allocations larger than `default_chunk_size` get a dedicated
 * chunk sized exactly to the request — no waste, no fragmentation
 * of the regular chunks. Allocations smaller or equal share the
 * current bump chunk. */
void   *lvh_alloc(lvh_t *h, size_t size);
/* }}} */

/* {{{ Stats (test / debug) */
/* Sum of every chunk's capacity (the working-set ceiling). */
uint64_t lvh_total_allocated(lvh_t *h);

/* Sum of bytes returned to callers via lvh_alloc. */
uint64_t lvh_total_used(lvh_t *h);

/* Count of chunks currently in the heap. */
uint32_t lvh_chunk_count(lvh_t *h);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_LARGE_VALUE_HEAP_H */
