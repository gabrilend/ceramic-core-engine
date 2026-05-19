/* src/015-large-value-heap.c — variable-size payload allocator.
 *
 * Chunked arena. Every chunk is a single malloc; the chunk's
 * capacity and used-watermark live in the chunk header, the payload
 * bytes follow as a flexible array. The heap holds the chunk list
 * head and a mutex serializing alloc / grow.
 *
 * The single mutex is intentional: large-value allocations are
 * relatively coarse (kilobytes-to-megabytes), and the time spent in
 * the lock is dominated by the memcpy that the caller performs
 * *outside* the lock. A per-chunk lock would add complexity for
 * negligible win. If profiling later shows the mutex is hot, the
 * upgrade path is a try-lock-with-thread-local-chunk scheme.
 *
 * Designed in issue 302's "Large-value heap (variable-size
 * payloads)" section; landed 2026-05-19 as a follow-on within 302.
 */

#include "015-large-value-heap.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

/* {{{ Defaults */
#define LVH_DEFAULT_CHUNK_SIZE  (64u * 1024u)
#define LVH_ALIGN               (16u)  /* generous alignment for any payload type */
/* }}} */

/* {{{ Chunk record */
typedef struct lvh_chunk {
    struct lvh_chunk *next;
    size_t            cap;     /* bytes available in `data` */
    size_t            used;    /* bytes bump-allocated so far */
    /* The flexible array begins at the alignment boundary — gcc
     * naturally aligns to 8 on x86-64; LVH_ALIGN is enforced inside
     * align_up() to give consumers a generous guarantee. */
    uint8_t           data[];
} lvh_chunk_t;
/* }}} */

/* {{{ Heap record */
struct lvh {
    lvh_chunk_t    *head;
    pthread_mutex_t mu;
    size_t          default_chunk_size;

    /* Stats. Atomic so callers can read without taking the mutex. */
    _Atomic uint64_t total_allocated;  /* sum of chunk capacities */
    _Atomic uint64_t total_used;       /* sum of used bytes        */
    _Atomic uint32_t chunk_count;
};
/* }}} */

/* {{{ Local helper — align_up() */
/* Round `n` up to the next multiple of LVH_ALIGN. Aligning here
 * means any pointer we hand out is at least 16-byte aligned, which
 * is enough for any standard C scalar or vector type. */
static inline size_t align_up(size_t n)
{
    return (n + (LVH_ALIGN - 1u)) & ~(size_t)(LVH_ALIGN - 1u);
}
/* }}} */

/* {{{ Local helper — alloc_chunk() */
/* Allocate a new chunk with at least `min_cap` bytes available.
 * Returns NULL on allocation failure. Updates the stats counters. */
static lvh_chunk_t *alloc_chunk(lvh_t *h, size_t min_cap)
{
    size_t cap = h->default_chunk_size;
    if (min_cap > cap) {
        /* Caller asked for more than a default chunk holds. Give it
         * its own oversized chunk sized to exactly fit; this keeps
         * the default chunks from being permanently bloated by one
         * unusual allocation. */
        cap = min_cap;
    }
    lvh_chunk_t *c = malloc(sizeof(lvh_chunk_t) + cap);
    if (!c) return NULL;
    c->next = NULL;
    c->cap  = cap;
    c->used = 0;

    atomic_fetch_add_explicit(&h->total_allocated,
                              (uint64_t)cap,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&h->chunk_count, 1u, memory_order_relaxed);
    return c;
}
/* }}} */

/* {{{ lvh_create() */
lvh_t *lvh_create(size_t default_chunk_size)
{
    lvh_t *h = calloc(1, sizeof *h);
    if (!h) return NULL;
    h->default_chunk_size = (default_chunk_size > 0)
                            ? default_chunk_size
                            : LVH_DEFAULT_CHUNK_SIZE;
    if (pthread_mutex_init(&h->mu, NULL) != 0) {
        free(h);
        return NULL;
    }
    atomic_init(&h->total_allocated, 0u);
    atomic_init(&h->total_used,      0u);
    atomic_init(&h->chunk_count,     0u);
    return h;
}
/* }}} */

/* {{{ lvh_destroy() */
void lvh_destroy(lvh_t *h)
{
    if (!h) return;
    lvh_chunk_t *c = h->head;
    while (c) {
        lvh_chunk_t *next = c->next;
        free(c);
        c = next;
    }
    pthread_mutex_destroy(&h->mu);
    free(h);
}
/* }}} */

/* {{{ lvh_alloc() */
void *lvh_alloc(lvh_t *h, size_t size)
{
    if (!h) return NULL;
    if (size == 0) {
        /* Zero-byte allocation: return a non-NULL "this is intentional"
         * pointer. Hand out a stable address that nobody else will
         * see — the head of an allocation of LVH_ALIGN bytes, which
         * we waste deliberately. This keeps callers from special-
         * casing the empty case while still letting NULL mean
         * "failure". */
        size = 1;
    }

    size_t need = align_up(size);

    pthread_mutex_lock(&h->mu);

    /* Fast path: current chunk has room. */
    lvh_chunk_t *c = h->head;
    if (!c || c->used + need > c->cap) {
        lvh_chunk_t *fresh = alloc_chunk(h, need);
        if (!fresh) {
            pthread_mutex_unlock(&h->mu);
            return NULL;
        }
        fresh->next = h->head;
        h->head     = fresh;
        c           = fresh;
    }

    void *p = c->data + c->used;
    c->used += need;

    atomic_fetch_add_explicit(&h->total_used,
                              (uint64_t)need,
                              memory_order_relaxed);

    pthread_mutex_unlock(&h->mu);
    return p;
}
/* }}} */

/* {{{ Stats accessors */
uint64_t lvh_total_allocated(lvh_t *h)
{
    return h ? atomic_load_explicit(&h->total_allocated,
                                    memory_order_relaxed) : 0;
}

uint64_t lvh_total_used(lvh_t *h)
{
    return h ? atomic_load_explicit(&h->total_used,
                                    memory_order_relaxed) : 0;
}

uint32_t lvh_chunk_count(lvh_t *h)
{
    return h ? atomic_load_explicit(&h->chunk_count,
                                    memory_order_relaxed) : 0;
}
/* }}} */
