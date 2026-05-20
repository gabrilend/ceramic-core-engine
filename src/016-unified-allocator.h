/* src/016-unified-allocator.h — the one allocator described in
 * issue 302's "Allocation strategy" section.
 *
 * What this module does, in plain terms:
 *
 *   Before the run starts, the loader writes down every distinct
 *   output-size declared by any box in the graph and how many boxes
 *   need each size. ua_create takes that list and pre-builds one
 *   free-list per distinct size, populated with enough chunks to
 *   cover the demand the loader counted.
 *
 *   During the run, ua_alloc(size) finds the smallest pre-built
 *   class whose chunks are large enough and hands one back. If the
 *   class is empty (demand exceeded what the loader pre-warmed) or
 *   the request is larger than any declared class (a runtime-only
 *   size), the allocator grows by getting another region from the
 *   operating system.
 *
 *   Every chunk handed out carries a reference count. ua_ref bumps
 *   it; ua_unref drops it; when the count hits zero, the chunk goes
 *   back to its free-list AND the deallocator checks the chunk's
 *   immediate neighbors in memory. If either neighbor is also free,
 *   they fuse into one larger chunk that goes onto the appropriate
 *   larger free-list. This is the "eager neighbor merge" from the
 *   issue file — O(1) and runs on every free.
 *
 *   When ua_alloc can't find a fitting chunk from any free-list,
 *   ua_sweep runs first (walks the address-ordered chunk list
 *   looking for runs of free chunks that the cheap neighbor-merge
 *   couldn't catch, fuses them). If the sweep produces a fitting
 *   chunk, the allocation succeeds without growing. If not, the
 *   heap grows. This is the "mandatory deep sweep at allocation
 *   failure" trigger from the issue file.
 *
 *   The quiescence trigger described in the issue file is not in
 *   this stage — the dispatch layer will call ua_sweep at its own
 *   quiescence points when that integration happens. For now the
 *   only automatic sweep is the one inside ua_alloc.
 *
 * What this module does NOT do (yet):
 *   - It does not integrate with the slot store or the dispatch
 *     layer. Those are later stages of the implementation. The
 *     existing chained-block region for variable-size payloads
 *     remains in use until those stages land.
 *   - It does not slice oversized chunks (a 2000-byte request that
 *     pops a 4096-byte chunk does not get the 2096-byte tail
 *     pushed back as a residual). The eager neighbor-merge and the
 *     pre-warmed class sizes cover the common case; slicing is a
 *     future optimization the API supports without source changes.
 *   - It uses one allocator-wide mutex instead of per-class locks.
 *     The issue file describes per-class locks as the optimization
 *     path; this stage keeps the locking simple and correct.
 *
 * Designed in issue 302's rewrite (2026-05-19).
 */

#ifndef SORAMECH_UNIFIED_ALLOCATOR_H
#define SORAMECH_UNIFIED_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* {{{ Opaque types */
typedef struct ua       ua_t;        /* the heap */
typedef struct ua_chunk ua_chunk_t;  /* one allocation */
/* }}} */

/* {{{ Declared size classes */
/* A single (size, count) pair telling the allocator: "the graph
 * needs `count` chunks of `size` bytes." */
typedef struct ua_class_decl {
    size_t   size;
    uint32_t prewarm_count;
} ua_class_decl_t;
/* }}} */

/* {{{ Lifecycle */
/* Construct a new heap.
 *
 *   classes        — array of declared size classes from graph analysis,
 *                    or NULL/zero if no pre-warming is wanted. Sizes
 *                    do not need to be sorted; the allocator sorts
 *                    them internally. Duplicate sizes are merged
 *                    (counts summed).
 *   n_classes      — number of entries in `classes`.
 *
 * Returns NULL on allocation failure or invalid arguments (e.g. any
 * class with size == 0). */
ua_t *ua_create(const ua_class_decl_t *classes, size_t n_classes);

/* Free every region and the heap struct. Safe on NULL. After this
 * call, every chunk previously returned by ua_alloc on this heap is
 * invalid. */
void  ua_destroy(ua_t *h);
/* }}} */

/* {{{ Allocation */
/* Allocate a chunk of at least `size` bytes. The returned chunk has
 * reference count 1; the caller is the initial owner.
 *
 * Lookup is "smallest free-list whose class size >= requested." If
 * no free-list has a chunk and the sweep can't produce one, the
 * heap grows by getting a new region from the OS.
 *
 * Returns NULL on allocation failure or if h is NULL or size is 0. */
ua_chunk_t *ua_alloc(ua_t *h, size_t size);

/* Bump the chunk's reference count by 1. Safe on NULL chunk
 * (no-op). Returns the chunk for chaining. */
ua_chunk_t *ua_ref(ua_chunk_t *chunk);

/* Drop the chunk's reference count by 1. If the count reaches zero
 * the chunk is returned to its free-list and an O(1) eager merge
 * with physical neighbors runs. Safe on NULL chunk (no-op). */
void ua_unref(ua_t *h, ua_chunk_t *chunk);

/* Direct accessors. Both safe on NULL (data returns NULL, size
 * returns 0). */
void   *ua_data(ua_chunk_t *chunk);
size_t  ua_size(ua_chunk_t *chunk);
/* }}} */

/* {{{ Sweep */
/* Walk the address-ordered chunk list and fuse runs of contiguous
 * free chunks that the eager neighbor-merge couldn't catch (e.g.
 * because the middle chunk in a 1-2-3 free run only got merged with
 * one neighbor at unref time, leaving the other free chunk on a
 * different list).
 *
 * The allocator runs this automatically before growing the heap.
 * Callers may also invoke it directly at quiescence points (e.g.
 * between dispatch batches). Returns the number of merges
 * performed. */
uint32_t ua_sweep(ua_t *h);
/* }}} */

/* {{{ Stats (test / debug / future telemetry) */
uint64_t ua_bytes_in_use(const ua_t *h);
uint64_t ua_bytes_free  (const ua_t *h);
uint64_t ua_bytes_total (const ua_t *h);  /* sum of every region's capacity */
uint32_t ua_region_count(const ua_t *h);
uint32_t ua_alloc_calls (const ua_t *h);
uint32_t ua_free_calls  (const ua_t *h);  /* unrefs that reached zero */
uint32_t ua_neighbor_merges(const ua_t *h);
uint32_t ua_sweep_merges(const ua_t *h);
uint32_t ua_grow_calls  (const ua_t *h);

/* For tests: how many free chunks currently sit on the free-list
 * for the class whose chunks are exactly `size` bytes. Returns 0
 * if no such class exists or the list is empty. */
uint32_t ua_class_free_count(const ua_t *h, size_t size);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_UNIFIED_ALLOCATOR_H */
