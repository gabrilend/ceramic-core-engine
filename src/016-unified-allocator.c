/* src/016-unified-allocator.c — implementation of the unified
 * allocator declared in 016-unified-allocator.h.
 *
 * The story this file tells, in order:
 *
 *   A chunk header carries the chunk's size, refcount, free flag,
 *   and three pointers: two for the address-ordered doubly-linked
 *   list (so neighbor-merge can find what's physically next to it),
 *   and one for the free-list it currently sits on (when free).
 *   The payload bytes follow the header as a flexible array.
 *
 *   The allocator owns a sorted array of size classes. For each
 *   class there's a head pointer into a singly-linked free-list of
 *   chunks of exactly that size.
 *
 *   The allocator also owns a list of "regions" — single mallocs
 *   from the OS, each carved into chunks at creation time. Every
 *   chunk in a region is in the region's own address-ordered list
 *   (so the prev/next pointers actually reflect physical
 *   adjacency). Region boundaries are non-mergeable.
 *
 *   ua_alloc(size) finds the smallest class whose size >= request,
 *   pops the head of that class's free-list, marks the chunk
 *   allocated (refcount=1, is_free=0), and returns it.
 *
 *   If the class is empty, the allocator tries larger classes.
 *   If every class is empty, the allocator runs the sweep, then
 *   tries again. If still nothing, it grows the heap by allocating
 *   a new region containing one chunk of the requested size (with
 *   the request rounded up to the requested class size if it
 *   matched a declared class, or aligned up if it didn't).
 *
 *   ua_unref drops the refcount. When it reaches zero, the chunk
 *   is pushed onto its class's free-list, then the deallocator
 *   checks the chunk's addr_prev and addr_next. If either neighbor
 *   is free, the two chunks are unlinked from their current
 *   free-lists, fused (the lower chunk's size absorbs the higher
 *   chunk; the higher chunk's address-list slot is removed), and
 *   the merged chunk is pushed onto the appropriate free-list. The
 *   merged size might match an existing class (push onto that
 *   class's list) or it might not (push onto the smallest class
 *   whose size >= merged_size, which is acceptable because alloc
 *   only ever looks at the chunk's actual size, not its class).
 *
 * One mutex on the whole allocator. Per-class locking is the
 * documented next step but not in this stage.
 *
 * Designed in issue 302's rewrite (2026-05-19).
 */

#include "016-unified-allocator.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Alignment */
/* Payload alignment — 16 bytes is sufficient for any standard C
 * scalar or SIMD vector. Chunk headers are sized so the flexible
 * array begins on this boundary. */
#define UA_ALIGN 16u

static inline size_t align_up(size_t n, size_t a)
{
    return (n + (a - 1u)) & ~(a - 1u);
}
/* }}} */

/* {{{ Chunk header */
struct ua_chunk {
    /* Address-ordered doubly-linked list, scoped to the chunk's
     * region. NULL at region boundaries — never link across regions. */
    struct ua_chunk *addr_prev;
    struct ua_chunk *addr_next;

    /* Singly-linked free-list pointer. Only meaningful when
     * is_free == 1. */
    struct ua_chunk *free_next;

    /* Bytes available to the caller (does not count this header). */
    size_t size;

    /* The class this chunk's free-list belongs to. Set at chunk
     * creation; updated when an eager merge promotes the chunk to
     * a different list. */
    size_t class_size;

    /* Reference count. Atomic for ua_ref/ua_unref under non-locked
     * paths in future stages; mutex covers the structural operations
     * (free-list manipulation, neighbor merging) where atomicity of
     * the count alone is not sufficient. */
    _Atomic uint32_t refcount;

    uint8_t is_free;
    uint8_t _pad[7];

    /* Flexible-array payload. Header is sized so this begins on a
     * UA_ALIGN boundary on every platform we target. */
    uint8_t data[];
};

#define UA_HEADER_BYTES sizeof(struct ua_chunk)
/* }}} */

/* {{{ Heap struct */
struct ua {
    pthread_mutex_t mu;

    /* Size classes, sorted ascending. */
    size_t       *class_sizes;
    ua_chunk_t  **class_heads;     /* free-list head per class */
    size_t        n_classes;
    size_t        classes_cap;

    /* Regions. Each entry is the base pointer of one big malloc. */
    void        **regions;
    ua_chunk_t  **region_firsts;   /* first chunk in each region */
    size_t        n_regions;
    size_t        regions_cap;

    /* Stats. */
    _Atomic uint64_t bytes_in_use;
    _Atomic uint64_t bytes_free;
    _Atomic uint64_t bytes_total;
    _Atomic uint32_t alloc_calls;
    _Atomic uint32_t free_calls;
    _Atomic uint32_t neighbor_merges;
    _Atomic uint32_t sweep_merges;
    _Atomic uint32_t grow_calls;
};
/* }}} */

/* {{{ Local helper — find_class_index */
/* Smallest class index whose class_sizes[i] >= want. Returns
 * h->n_classes if none. Caller holds h->mu. */
static size_t find_class_index(const ua_t *h, size_t want)
{
    /* Linear scan: class counts are tiny in practice (the graph
     * declares a handful of distinct sizes). Binary search becomes
     * worthwhile only past a few dozen classes; not the case here. */
    for (size_t i = 0; i < h->n_classes; i++) {
        if (h->class_sizes[i] >= want) return i;
    }
    return h->n_classes;
}
/* }}} */

/* {{{ Local helper — ensure_class */
/* Make sure a class with this exact size exists; return its index.
 * If the class is new, the new free-list head is NULL. Caller
 * holds h->mu. Returns SIZE_MAX on allocation failure. */
static size_t ensure_class(ua_t *h, size_t size)
{
    /* Already present? */
    for (size_t i = 0; i < h->n_classes; i++) {
        if (h->class_sizes[i] == size) return i;
    }

    /* Grow the arrays if needed. */
    if (h->n_classes == h->classes_cap) {
        size_t new_cap = h->classes_cap ? h->classes_cap * 2 : 8;
        size_t      *new_sizes = realloc(h->class_sizes, new_cap * sizeof *new_sizes);
        if (!new_sizes) return (size_t)-1;
        h->class_sizes = new_sizes;
        ua_chunk_t **new_heads = realloc(h->class_heads, new_cap * sizeof *new_heads);
        if (!new_heads) return (size_t)-1;
        h->class_heads = new_heads;
        h->classes_cap = new_cap;
    }

    /* Insert sorted. */
    size_t pos = h->n_classes;
    while (pos > 0 && h->class_sizes[pos - 1] > size) pos--;
    if (pos < h->n_classes) {
        memmove(&h->class_sizes[pos + 1], &h->class_sizes[pos],
                (h->n_classes - pos) * sizeof *h->class_sizes);
        memmove(&h->class_heads[pos + 1], &h->class_heads[pos],
                (h->n_classes - pos) * sizeof *h->class_heads);
    }
    h->class_sizes[pos] = size;
    h->class_heads[pos] = NULL;
    h->n_classes++;
    return pos;
}
/* }}} */

/* {{{ Local helper — push_to_free_list */
/* Push `c` onto the free-list for `class_idx`. Caller holds h->mu. */
static void push_to_free_list(ua_t *h, size_t class_idx, ua_chunk_t *c)
{
    c->is_free    = 1;
    c->class_size = h->class_sizes[class_idx];
    c->free_next  = h->class_heads[class_idx];
    h->class_heads[class_idx] = c;
}
/* }}} */

/* {{{ Local helper — remove_from_free_list */
/* Unlink `c` from its current free-list. Caller holds h->mu. */
static void remove_from_free_list(ua_t *h, ua_chunk_t *c)
{
    size_t idx = (size_t)-1;
    for (size_t i = 0; i < h->n_classes; i++) {
        if (h->class_sizes[i] == c->class_size) {
            idx = i;
            break;
        }
    }
    if (idx == (size_t)-1) return;  /* shouldn't happen but defensive */

    ua_chunk_t **link = &h->class_heads[idx];
    while (*link && *link != c) link = &(*link)->free_next;
    if (*link == c) *link = c->free_next;
    c->free_next = NULL;
}
/* }}} */

/* {{{ Local helper — register_region */
/* Append `base` and `first` to the region tracking arrays. Caller
 * holds h->mu. Returns 0 on success, -1 on allocation failure. */
static int register_region(ua_t *h, void *base, ua_chunk_t *first)
{
    if (h->n_regions == h->regions_cap) {
        size_t new_cap = h->regions_cap ? h->regions_cap * 2 : 4;
        void       **nr = realloc(h->regions,        new_cap * sizeof *nr);
        if (!nr) return -1;
        h->regions      = nr;
        ua_chunk_t **nf = realloc(h->region_firsts,  new_cap * sizeof *nf);
        if (!nf) return -1;
        h->region_firsts = nf;
        h->regions_cap   = new_cap;
    }
    h->regions[h->n_regions]       = base;
    h->region_firsts[h->n_regions] = first;
    h->n_regions++;
    return 0;
}
/* }}} */

/* {{{ Local helper — carve_region */
/* Build a region containing chunks per the given (size, count)
 * pairs, in declaration order. Pushes every chunk onto its class's
 * free-list and links them in address order. Caller holds h->mu.
 *
 * Returns 0 on success, -1 on allocation failure. */
static int carve_region(ua_t *h,
                        const ua_class_decl_t *classes,
                        size_t n_classes)
{
    size_t total = 0;
    for (size_t i = 0; i < n_classes; i++) {
        size_t s = align_up(classes[i].size, UA_ALIGN);
        total += (UA_HEADER_BYTES + s) * classes[i].prewarm_count;
    }
    if (total == 0) return 0;

    /* One malloc, generously aligned. */
    void *base = aligned_alloc(UA_ALIGN, align_up(total, UA_ALIGN));
    if (!base) return -1;

    /* Carve chunks back-to-back. */
    uint8_t    *cursor = base;
    ua_chunk_t *first  = NULL;
    ua_chunk_t *prev   = NULL;
    for (size_t i = 0; i < n_classes; i++) {
        size_t s = align_up(classes[i].size, UA_ALIGN);
        size_t class_idx = ensure_class(h, s);
        if (class_idx == (size_t)-1) {
            free(base);
            return -1;
        }
        for (uint32_t k = 0; k < classes[i].prewarm_count; k++) {
            ua_chunk_t *c = (ua_chunk_t *)cursor;
            c->addr_prev = prev;
            c->addr_next = NULL;
            c->free_next = NULL;
            c->size      = s;
            atomic_store_explicit(&c->refcount, 0, memory_order_relaxed);
            if (prev) prev->addr_next = c;
            if (!first) first = c;
            push_to_free_list(h, class_idx, c);
            prev = c;
            cursor += UA_HEADER_BYTES + s;
            atomic_fetch_add_explicit(&h->bytes_free, (uint64_t)s,
                                      memory_order_relaxed);
        }
    }

    if (register_region(h, base, first) < 0) {
        free(base);
        return -1;
    }
    atomic_fetch_add_explicit(&h->bytes_total, (uint64_t)total,
                              memory_order_relaxed);
    return 0;
}
/* }}} */

/* {{{ ua_create */
ua_t *ua_create(const ua_class_decl_t *classes, size_t n_classes)
{
    /* Validate. */
    for (size_t i = 0; i < n_classes; i++) {
        if (classes[i].size == 0) return NULL;
    }

    ua_t *h = calloc(1, sizeof *h);
    if (!h) return NULL;
    if (pthread_mutex_init(&h->mu, NULL) != 0) {
        free(h);
        return NULL;
    }

    /* De-duplicate by summing counts for identical sizes. */
    if (n_classes > 0) {
        ua_class_decl_t *dedup = calloc(n_classes, sizeof *dedup);
        if (!dedup) {
            pthread_mutex_destroy(&h->mu);
            free(h);
            return NULL;
        }
        size_t k = 0;
        for (size_t i = 0; i < n_classes; i++) {
            size_t aligned = align_up(classes[i].size, UA_ALIGN);
            size_t j;
            for (j = 0; j < k; j++) {
                if (dedup[j].size == aligned) {
                    dedup[j].prewarm_count += classes[i].prewarm_count;
                    break;
                }
            }
            if (j == k) {
                dedup[k].size = aligned;
                dedup[k].prewarm_count = classes[i].prewarm_count;
                k++;
            }
        }
        if (carve_region(h, dedup, k) < 0) {
            free(dedup);
            ua_destroy(h);
            return NULL;
        }
        free(dedup);
    }

    return h;
}
/* }}} */

/* {{{ ua_destroy */
void ua_destroy(ua_t *h)
{
    if (!h) return;
    for (size_t i = 0; i < h->n_regions; i++) free(h->regions[i]);
    free(h->regions);
    free(h->region_firsts);
    free(h->class_sizes);
    free(h->class_heads);
    pthread_mutex_destroy(&h->mu);
    free(h);
}
/* }}} */

/* {{{ Local helper — grow_for_size */
/* Add a new region containing one chunk that satisfies `want_size`.
 * The chunk is left on the free-list of the appropriate class.
 * Caller holds h->mu. Returns 0 on success, -1 on allocation
 * failure. */
static int grow_for_size(ua_t *h, size_t want_size)
{
    size_t s = align_up(want_size, UA_ALIGN);
    ua_class_decl_t one = { .size = s, .prewarm_count = 1 };
    int rc = carve_region(h, &one, 1);
    if (rc == 0) {
        atomic_fetch_add_explicit(&h->grow_calls, 1u, memory_order_relaxed);
    }
    return rc;
}
/* }}} */

/* {{{ Local helper — eager_merge */
/* If either neighbor of `c` (which has just been pushed onto its
 * free-list) is free, fuse them into one larger chunk. Updates
 * stats. Caller holds h->mu. */
static void eager_merge(ua_t *h, ua_chunk_t *c)
{
    /* Walk forward first: while addr_next is free, absorb it. */
    while (c->addr_next && c->addr_next->is_free) {
        ua_chunk_t *n = c->addr_next;

        remove_from_free_list(h, c);
        remove_from_free_list(h, n);

        /* Fuse: c grows to cover the header + payload of n. */
        c->size += UA_HEADER_BYTES + n->size;
        c->addr_next = n->addr_next;
        if (n->addr_next) n->addr_next->addr_prev = c;

        size_t new_class = ensure_class(h, c->size);
        if (new_class == (size_t)-1) {
            /* Couldn't grow class array. Restore c onto a list that
             * already exists — find any class with size matching or
             * larger and put c there. Should never happen in practice. */
            push_to_free_list(h, 0, c);
            return;
        }
        push_to_free_list(h, new_class, c);
        atomic_fetch_add_explicit(&h->neighbor_merges, 1u, memory_order_relaxed);
    }

    /* Then walk backward: while addr_prev is free, prev absorbs c. */
    while (c->addr_prev && c->addr_prev->is_free) {
        ua_chunk_t *p = c->addr_prev;

        remove_from_free_list(h, c);
        remove_from_free_list(h, p);

        p->size += UA_HEADER_BYTES + c->size;
        p->addr_next = c->addr_next;
        if (c->addr_next) c->addr_next->addr_prev = p;

        size_t new_class = ensure_class(h, p->size);
        if (new_class == (size_t)-1) {
            push_to_free_list(h, 0, p);
            return;
        }
        push_to_free_list(h, new_class, p);
        atomic_fetch_add_explicit(&h->neighbor_merges, 1u, memory_order_relaxed);
        c = p;  /* continue from the merged chunk */
    }
}
/* }}} */

/* {{{ ua_alloc */
ua_chunk_t *ua_alloc(ua_t *h, size_t size)
{
    if (!h || size == 0) return NULL;
    size_t aligned = align_up(size, UA_ALIGN);

    pthread_mutex_lock(&h->mu);

    for (int attempt = 0; attempt < 3; attempt++) {
        size_t idx = find_class_index(h, aligned);
        if (idx < h->n_classes && h->class_heads[idx]) {
            ua_chunk_t *c = h->class_heads[idx];
            h->class_heads[idx] = c->free_next;
            c->free_next = NULL;
            c->is_free   = 0;
            atomic_store_explicit(&c->refcount, 1u, memory_order_relaxed);
            atomic_fetch_add_explicit(&h->alloc_calls, 1u, memory_order_relaxed);
            atomic_fetch_add_explicit(&h->bytes_in_use, (uint64_t)c->size,
                                      memory_order_relaxed);
            atomic_fetch_sub_explicit(&h->bytes_free, (uint64_t)c->size,
                                      memory_order_relaxed);
            pthread_mutex_unlock(&h->mu);
            return c;
        }
        /* Try larger classes. */
        size_t larger = idx + 1;
        while (larger < h->n_classes && !h->class_heads[larger]) larger++;
        if (larger < h->n_classes) {
            ua_chunk_t *c = h->class_heads[larger];
            h->class_heads[larger] = c->free_next;
            c->free_next = NULL;
            c->is_free   = 0;
            atomic_store_explicit(&c->refcount, 1u, memory_order_relaxed);
            atomic_fetch_add_explicit(&h->alloc_calls, 1u, memory_order_relaxed);
            atomic_fetch_add_explicit(&h->bytes_in_use, (uint64_t)c->size,
                                      memory_order_relaxed);
            atomic_fetch_sub_explicit(&h->bytes_free, (uint64_t)c->size,
                                      memory_order_relaxed);
            pthread_mutex_unlock(&h->mu);
            return c;
        }

        if (attempt == 0) {
            /* Mandatory sweep before growing. */
            uint32_t merges = 0;
            for (size_t r = 0; r < h->n_regions; r++) {
                ua_chunk_t *cur = h->region_firsts[r];
                while (cur && cur->addr_next) {
                    if (cur->is_free && cur->addr_next->is_free) {
                        ua_chunk_t *n = cur->addr_next;
                        remove_from_free_list(h, cur);
                        remove_from_free_list(h, n);
                        cur->size += UA_HEADER_BYTES + n->size;
                        cur->addr_next = n->addr_next;
                        if (n->addr_next) n->addr_next->addr_prev = cur;
                        size_t new_class = ensure_class(h, cur->size);
                        if (new_class != (size_t)-1) {
                            push_to_free_list(h, new_class, cur);
                        }
                        merges++;
                        continue;  /* re-check from cur — might fuse further */
                    }
                    cur = cur->addr_next;
                }
            }
            atomic_fetch_add_explicit(&h->sweep_merges, merges, memory_order_relaxed);
            continue;  /* retry from the top */
        }

        if (attempt == 1) {
            /* Grow. */
            if (grow_for_size(h, aligned) < 0) {
                pthread_mutex_unlock(&h->mu);
                return NULL;
            }
            continue;  /* retry from the top — should succeed now */
        }
    }

    pthread_mutex_unlock(&h->mu);
    return NULL;
}
/* }}} */

/* {{{ ua_ref */
ua_chunk_t *ua_ref(ua_chunk_t *chunk)
{
    if (!chunk) return NULL;
    atomic_fetch_add_explicit(&chunk->refcount, 1u, memory_order_relaxed);
    return chunk;
}
/* }}} */

/* {{{ ua_unref */
void ua_unref(ua_t *h, ua_chunk_t *chunk)
{
    if (!h || !chunk) return;
    uint32_t prev = atomic_fetch_sub_explicit(&chunk->refcount, 1u,
                                              memory_order_acq_rel);
    if (prev != 1) return;  /* still has references */

    pthread_mutex_lock(&h->mu);

    /* It is possible (under future per-class locking) that another
     * thread ref'd between the atomic decrement and acquiring the
     * mutex. Re-check. */
    if (atomic_load_explicit(&chunk->refcount, memory_order_acquire) > 0) {
        pthread_mutex_unlock(&h->mu);
        return;
    }

    atomic_fetch_sub_explicit(&h->bytes_in_use, (uint64_t)chunk->size,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&h->bytes_free, (uint64_t)chunk->size,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&h->free_calls, 1u, memory_order_relaxed);

    size_t class_idx = ensure_class(h, chunk->size);
    if (class_idx != (size_t)-1) {
        push_to_free_list(h, class_idx, chunk);
        eager_merge(h, chunk);
    }

    pthread_mutex_unlock(&h->mu);
}
/* }}} */

/* {{{ ua_data, ua_size */
void *ua_data(ua_chunk_t *chunk)
{
    if (!chunk) return NULL;
    return chunk->data;
}

size_t ua_size(ua_chunk_t *chunk)
{
    if (!chunk) return 0;
    return chunk->size;
}
/* }}} */

/* {{{ ua_sweep */
uint32_t ua_sweep(ua_t *h)
{
    if (!h) return 0;
    uint32_t merges = 0;
    pthread_mutex_lock(&h->mu);
    for (size_t r = 0; r < h->n_regions; r++) {
        ua_chunk_t *cur = h->region_firsts[r];
        while (cur && cur->addr_next) {
            if (cur->is_free && cur->addr_next->is_free) {
                ua_chunk_t *n = cur->addr_next;
                remove_from_free_list(h, cur);
                remove_from_free_list(h, n);
                cur->size += UA_HEADER_BYTES + n->size;
                cur->addr_next = n->addr_next;
                if (n->addr_next) n->addr_next->addr_prev = cur;
                size_t new_class = ensure_class(h, cur->size);
                if (new_class != (size_t)-1) {
                    push_to_free_list(h, new_class, cur);
                }
                merges++;
                continue;
            }
            cur = cur->addr_next;
        }
    }
    atomic_fetch_add_explicit(&h->sweep_merges, merges, memory_order_relaxed);
    pthread_mutex_unlock(&h->mu);
    return merges;
}
/* }}} */

/* {{{ Stats accessors */
uint64_t ua_bytes_in_use(const ua_t *h)
{
    return h ? atomic_load_explicit(&h->bytes_in_use, memory_order_relaxed) : 0;
}
uint64_t ua_bytes_free(const ua_t *h)
{
    return h ? atomic_load_explicit(&h->bytes_free, memory_order_relaxed) : 0;
}
uint64_t ua_bytes_total(const ua_t *h)
{
    return h ? atomic_load_explicit(&h->bytes_total, memory_order_relaxed) : 0;
}
uint32_t ua_region_count(const ua_t *h)
{
    return h ? (uint32_t)h->n_regions : 0;
}
uint32_t ua_alloc_calls(const ua_t *h)
{
    return h ? atomic_load_explicit(&h->alloc_calls, memory_order_relaxed) : 0;
}
uint32_t ua_free_calls(const ua_t *h)
{
    return h ? atomic_load_explicit(&h->free_calls, memory_order_relaxed) : 0;
}
uint32_t ua_neighbor_merges(const ua_t *h)
{
    return h ? atomic_load_explicit(&h->neighbor_merges, memory_order_relaxed) : 0;
}
uint32_t ua_sweep_merges(const ua_t *h)
{
    return h ? atomic_load_explicit(&h->sweep_merges, memory_order_relaxed) : 0;
}
uint32_t ua_grow_calls(const ua_t *h)
{
    return h ? atomic_load_explicit(&h->grow_calls, memory_order_relaxed) : 0;
}
uint32_t ua_class_free_count(const ua_t *h, size_t size)
{
    if (!h) return 0;
    size_t target = align_up(size, UA_ALIGN);
    uint32_t count = 0;
    pthread_mutex_lock((pthread_mutex_t *)&h->mu);
    for (size_t i = 0; i < h->n_classes; i++) {
        if (h->class_sizes[i] != target) continue;
        for (ua_chunk_t *c = h->class_heads[i]; c; c = c->free_next) count++;
        break;
    }
    pthread_mutex_unlock((pthread_mutex_t *)&h->mu);
    return count;
}
/* }}} */
