/* src/009-slot-store.c — per-input-port slot store, implementation.
 *
 * Three slot shapes coexist behind one set of APIs:
 *
 *   - Ring-buffer slots (peek and pop modes) — a header followed by
 *     n_cells cells laid out contiguously. Each cell is a 4-byte
 *     filled_size, optionally a 4-byte tag (for SLOT_FLAG_TAGGED),
 *     then cell_capacity bytes of payload. Untagged slots use the
 *     standard head/tail ring discipline; tagged slots use head/tail
 *     for the bookkeeping of "where to put the next push" but pop
 *     scans for the lowest-tag filled cell.
 *
 *   - Atomic-counter slots — no header bookkeeping, no payload cells;
 *     just one atomic_uint that slot_read_inc fetches-and-adds. No
 *     lock needed.
 *
 * Per-slot concurrency is a single atomic_flag spinlock for the
 * ring-buffer slots. Inside the lock the header and cells are mutated
 * without atomics; outside the lock no header bytes are touched.
 *
 * The store itself owns a flat array of slot pointers indexed by
 * slot_id_t. The array grows on demand. Ids are never reused — there
 * is no slot_free; slots live for the run.
 *
 * Variable-size payloads (SLOT_FLAG_LARGE_VALUE) used to point into a
 * monotonically-growing arena. That arena was a memory leak by design
 * — every push consumed bytes that were never reclaimed until run end.
 * The 2026-05-19 retraction in issue 302 commits to the unified
 * allocator (016) instead: every variable-size payload is a
 * reference-counted chunk, popped values release their reference, and
 * freed chunks rejoin the allocator's free-lists with eager
 * neighbor-merge. The cell layout is unchanged — the 8-byte payload
 * slot that used to hold a raw arena pointer now holds an opaque
 * chunk handle.
 *
 * Designed in issue 302. This file implements the API declared in
 * 009-slot-store.h. Free-list optimization for the fixed-size cell
 * arrays themselves is still deferred.
 */

#include "009-slot-store.h"
#include "016-unified-allocator.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Internal slot record */
typedef struct slot_rec {
    uint32_t    cell_capacity;   /* bytes per cell payload (ring slots) */
    uint32_t    n_cells;         /* ring size (ring slots)              */
    _Atomic uint32_t head;       /* read index, mod n_cells             */
    _Atomic uint32_t tail;       /* write index, mod n_cells            */
    atomic_flag lock;            /* per-slot spinlock for header+cells  */
    uint32_t    flags;           /* SLOT_FLAG_*                         */
    uint32_t    cell_record;     /* derived: 4 (+4 if tagged) + cap     */

    /* For atomic-counter slots: a single atomic uint, used by
     * slot_read_inc. Lives separately from `cells` so the two slot
     * shapes don't share storage interpretation. */
    _Atomic uint32_t counter;

    /* The cell array. Memory is owned by `cells_chunk` (a chunk
     * from the store's unified allocator). For atomic-counter
     * slots both fields are NULL. The split between header and
     * cell-array storage lets the cell-array memory go through
     * the same recycling pipeline as the large-value payloads —
     * one heap for both kinds of slot memory, as the design
     * retraction requires. */
    uint8_t     *cells;
    ua_chunk_t  *cells_chunk;
} slot_rec_t;
/* }}} */

/* {{{ Internal store */
struct slot_store {
    slot_rec_t **slots;        /* dynamic array, indexed by slot_id_t */
    int32_t      count;
    int32_t      capacity;

    /* The unified allocator. Owns the cell-array memory for every
     * ring slot in this store, plus the payload bytes for every
     * LARGE_VALUE cell. Created in slot_store_create; destroyed
     * alongside the store. Any chunks still parked in cells at
     * destroy time are reclaimed in bulk when the allocator's
     * regions are freed. */
    ua_t        *heap;
};
/* }}} */

/* {{{ Local helpers — locking */
static inline void lock_slot(slot_rec_t *r)
{
    while (atomic_flag_test_and_set_explicit(&r->lock, memory_order_acquire)) {
        /* spin — contention is expected to be brief because the
         * locked section is just a header update plus a memcpy. */
    }
}

static inline void unlock_slot(slot_rec_t *r)
{
    atomic_flag_clear_explicit(&r->lock, memory_order_release);
}
/* }}} */

/* {{{ Local helpers — cell layout */
/* Pointer to the i-th cell within the slot's flexible array. The cell
 * record layout for ring slots is:
 *
 *   bytes 0..3   : filled_size (uint32_t; 0 means empty)
 *   bytes 4..7   : tag         (uint32_t; SLOT_FLAG_TAGGED *or*
 *                               SLOT_FLAG_LARGE_VALUE — the latter
 *                               keeps the 8-byte payload offset
 *                               consistent so the inline pointer is
 *                               always aligned)
 *   bytes (4|8)..: payload     (cell_capacity bytes; for LARGE_VALUE
 *                               this is a stable lvh-heap pointer)
 */
static inline uint8_t *cell_ptr(slot_rec_t *r, uint32_t i)
{
    /* r->cells is now a pointer into an allocator chunk, not a
     * flexible array on the slot record itself — same indexing
     * arithmetic. */
    return r->cells + (size_t)i * r->cell_record;
}

static inline uint32_t cell_filled(slot_rec_t *r, uint32_t i)
{
    uint32_t v;
    memcpy(&v, cell_ptr(r, i), sizeof v);
    return v;
}

static inline uint32_t cell_tag(slot_rec_t *r, uint32_t i)
{
    uint32_t v;
    memcpy(&v, cell_ptr(r, i) + 4, sizeof v);
    return v;
}

/* The payload offset within a cell is 8 if there's a tag header OR
 * if the slot is LARGE_VALUE (we always reserve 4 bytes of pad for
 * LARGE_VALUE so the inline pointer is 8-byte aligned). Otherwise
 * it's 4. */
static inline uint8_t *cell_data(slot_rec_t *r, uint32_t i)
{
    int has_8b_header = (r->flags & SLOT_FLAG_TAGGED)
                     || (r->flags & SLOT_FLAG_LARGE_VALUE);
    return cell_ptr(r, i) + (has_8b_header ? 8 : 4);
}
/* }}} */

/* {{{ slot_store_create() */
slot_store_t *slot_store_create(void)
{
    slot_store_t *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->capacity = 16;
    s->slots    = calloc((size_t)s->capacity, sizeof(slot_rec_t *));
    if (!s->slots) { free(s); return NULL; }

    /* Eagerly create the unified allocator. Earlier in this issue
     * the allocator was lazy-created on the first LARGE_VALUE slot;
     * since the cell arrays of ordinary ring slots also live in the
     * allocator now, every slot allocation needs the allocator
     * present from the start. No pre-warmed classes — they accrete
     * on first use until the graph loader is wired to declare
     * sizes up front. */
    s->heap = ua_create(NULL, 0);
    if (!s->heap) { free(s->slots); free(s); return NULL; }

    return s;
}
/* }}} */

/* {{{ slot_store_destroy() */
void slot_store_destroy(slot_store_t *s)
{
    if (!s) return;
    /* Release each slot's cell-array chunk before tearing down the
     * allocator. We don't strictly need to (ua_destroy frees all
     * regions in bulk) but the unref'ed-then-destroyed path leaves
     * the allocator's stats coherent if anything inspects them on
     * the way out. The slot record itself was allocated via plain
     * malloc and is freed here. */
    for (int32_t i = 0; i < s->count; i++) {
        slot_rec_t *r = s->slots[i];
        if (!r) continue;
        if (r->cells_chunk) ua_unref(s->heap, r->cells_chunk);
        free(r);
    }
    free(s->slots);
    /* The unified allocator owns the cell arrays for every ring
     * slot in this store and the payload bytes for every
     * LARGE_VALUE cell. ua_destroy releases all regions in one
     * sweep. Any LARGE_VALUE chunks still parked in cells at this
     * point become harmless garbage at the same moment the
     * regions are freed. */
    if (s->heap) ua_destroy(s->heap);
    free(s);
}
/* }}} */

/* {{{ slot_store_size() */
int32_t slot_store_size(slot_store_t *s)
{
    return s ? s->count : 0;
}
/* }}} */

/* {{{ Local helper — grow_store_if_needed() */
static int grow_store_if_needed(slot_store_t *s)
{
    if (s->count < s->capacity) return 0;
    int32_t new_cap = s->capacity * 2;
    slot_rec_t **grown = realloc(s->slots, (size_t)new_cap * sizeof(slot_rec_t *));
    if (!grown) return -1;
    memset(grown + s->capacity, 0,
           (size_t)(new_cap - s->capacity) * sizeof(slot_rec_t *));
    s->slots    = grown;
    s->capacity = new_cap;
    return 0;
}
/* }}} */

/* {{{ slot_alloc() */
slot_id_t slot_alloc(slot_store_t *s,
                     int32_t cell_capacity,
                     int32_t n_cells,
                     int32_t flags)
{
    if (!s) return SLOT_INVALID;

    size_t bytes_cells = 0;
    uint32_t cell_record = 0;
    uint32_t stored_capacity = 0;

    if (flags & SLOT_FLAG_ATOMIC_COUNTER) {
        /* Atomic-counter slot: no ring cells. cell_capacity and
         * n_cells are ignored. */
        bytes_cells = 0;
    } else if (flags & SLOT_FLAG_LARGE_VALUE) {
        /* Large-value slot: the cell holds an opaque chunk handle
         * from the unified allocator, not the bytes themselves.
         * Caller-supplied cell_capacity is ignored (the actual
         * payload bytes live in the allocator and can be any
         * size). n_cells controls ring depth as usual.
         *
         * Cell layout for LARGE_VALUE is 4 (filled_size) + 4
         * (tag or pad) + 8 (chunk handle) = 16 bytes; cell_data()
         * uses the 8-byte payload offset so the handle is
         * naturally aligned. */
        if (n_cells <= 0) return SLOT_INVALID;
        /* The cell payload is one machine pointer; sizeof(void *)
         * sidesteps a clang-tidy warning about "sizeof of a pointer
         * to an aggregate" while still being the right number — all
         * data pointers have the same size on the platforms we
         * target. */
        stored_capacity = (uint32_t)sizeof(void *);
        cell_record     = 4u + 4u + (uint32_t)sizeof(void *);
        bytes_cells     = (size_t)n_cells * cell_record;

        /* Allocator is created up front in slot_store_create. */
    } else {
        if (cell_capacity <= 0 || n_cells <= 0) return SLOT_INVALID;
        stored_capacity = (uint32_t)cell_capacity;
        cell_record = (uint32_t)(4
                                  + ((flags & SLOT_FLAG_TAGGED) ? 4 : 0)
                                  + cell_capacity);
        bytes_cells = (size_t)n_cells * cell_record;
    }

    slot_rec_t *r = calloc(1, sizeof(slot_rec_t));
    if (!r) return SLOT_INVALID;

    r->cell_capacity = (flags & SLOT_FLAG_ATOMIC_COUNTER) ? 0 : stored_capacity;
    r->n_cells       = (flags & SLOT_FLAG_ATOMIC_COUNTER) ? 0 : (uint32_t)n_cells;
    r->cell_record   = cell_record;
    r->flags         = (uint32_t)flags;
    atomic_init(&r->head,    0u);
    atomic_init(&r->tail,    0u);
    atomic_init(&r->counter, 0u);
    /* atomic_flag has no atomic_flag_init in C11; ATOMIC_FLAG_INIT is
     * for initializers. Clearing it leaves it in the "not set" state. */
    atomic_flag_clear(&r->lock);

    /* Acquire the cell-array memory from the unified allocator. Ring
     * slots take one chunk sized to n_cells * cell_record; atomic
     * counter slots have no cells and skip this step. The chunk's
     * refcount starts at 1 — the slot itself owns that reference for
     * the duration of the run. */
    if (bytes_cells > 0) {
        r->cells_chunk = ua_alloc(s->heap, bytes_cells);
        if (!r->cells_chunk) { free(r); return SLOT_INVALID; }
        r->cells = ua_data(r->cells_chunk);
        /* The allocator hands back uninitialized bytes. The cell
         * header's filled_size must start at zero so empty cells
         * look empty to peek and pop. */
        memset(r->cells, 0, bytes_cells);
    }

    if (grow_store_if_needed(s) != 0) {
        if (r->cells_chunk) ua_unref(s->heap, r->cells_chunk);
        free(r);
        return SLOT_INVALID;
    }
    slot_id_t id = s->count++;
    s->slots[id] = r;
    return id;
}
/* }}} */

/* {{{ Local helper — get_slot() */
static slot_rec_t *get_slot(slot_store_t *s, slot_id_t id)
{
    if (!s || id < 0 || id >= s->count) return NULL;
    return s->slots[id];
}
/* }}} */

/* {{{ slot_push() */
int slot_push(slot_store_t *s, slot_id_t id,
              const void *data, int32_t size, uint32_t tag)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r)                                      return -1;
    if (r->flags & SLOT_FLAG_ATOMIC_COUNTER)     return -1;
    if (size < 0)                                return -1;
    if (!data && size > 0)                       return -1;

    int large = (r->flags & SLOT_FLAG_LARGE_VALUE) != 0;
    /* For LARGE_VALUE slots size has no per-cell upper bound; the
     * heap absorbs whatever the caller wants to store. For
     * fixed-size slots, enforce the cell capacity as before. */
    if (!large && (uint32_t)size > r->cell_capacity) return -1;

    /* For LARGE_VALUE, allocate the chunk BEFORE taking the slot's
     * spinlock. The unified allocator has its own mutex; nesting the
     * two is fine but reducing time-under-spinlock keeps the lock
     * holder fast for everyone else. */
    ua_chunk_t *chunk = NULL;
    if (large && size > 0) {
        chunk = ua_alloc(s->heap, (size_t)size);
        if (!chunk) return -1;
        memcpy(ua_data(chunk), data, (size_t)size);
    }

    lock_slot(r);

    uint32_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    if (tail - head >= r->n_cells) {
        /* Ring full. Release the chunk we just took so it goes back
         * to the allocator's free-lists — the old arena had no such
         * release path and accumulated bytes on every failed push. */
        unlock_slot(r);
        if (chunk) ua_unref(s->heap, chunk);
        return -1;
    }

    uint32_t i = tail % r->n_cells;
    uint8_t *cp = cell_ptr(r, i);

    /* filled_size */
    uint32_t fs = (uint32_t)size;
    memcpy(cp, &fs, sizeof fs);
    /* tag (only if tagged); LARGE_VALUE without TAGGED still has 4
     * pad bytes in the same position so the chunk handle stays
     * 8-byte aligned, but we leave them zero. */
    if (r->flags & SLOT_FLAG_TAGGED) {
        memcpy(cp + 4, &tag, sizeof tag);
    }
    /* payload */
    if (large) {
        /* Store the chunk handle (may be NULL for zero-size — peek
         * and pop both treat NULL as "no bytes to copy"). */
        memcpy(cell_data(r, i), &chunk, sizeof(void *));
    } else if (size > 0) {
        memcpy(cell_data(r, i), data, (size_t)size);
    }

    atomic_store_explicit(&r->tail, tail + 1u, memory_order_release);
    unlock_slot(r);
    return 0;
}
/* }}} */

/* {{{ slot_peek() */
int32_t slot_peek(slot_store_t *s, slot_id_t id,
                  void *buf, int32_t buf_size)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r)                                  return -1;
    if (r->flags & SLOT_FLAG_ATOMIC_COUNTER) return -1;
    if (buf_size < 0 || (!buf && buf_size > 0)) return -1;

    int large = (r->flags & SLOT_FLAG_LARGE_VALUE) != 0;

    lock_slot(r);

    uint32_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    if (head == tail) {
        unlock_slot(r);
        return -1;
    }

    uint32_t i = head % r->n_cells;
    uint32_t fs = cell_filled(r, i);
    if (fs == 0) {
        unlock_slot(r);
        return -1;
    }
    if ((uint32_t)buf_size < fs) {
        unlock_slot(r);
        return -1;
    }
    if (fs > 0) {
        if (large) {
            ua_chunk_t *chunk = NULL;
            memcpy(&chunk, cell_data(r, i), sizeof(void *));
            if (chunk) memcpy(buf, ua_data(chunk), fs);
        } else {
            memcpy(buf, cell_data(r, i), fs);
        }
    }
    /* head not advanced — peek leaves the cell in place and the
     * chunk's reference count untouched. */
    unlock_slot(r);
    return (int32_t)fs;
}
/* }}} */

/* {{{ Local helper — find_lowest_tag_index() */
/* Return the in-ring cell index with the lowest tag among filled
 * cells. The caller holds the slot lock. Returns UINT32_MAX if no
 * filled cells exist. */
static uint32_t find_lowest_tag_index(slot_rec_t *r)
{
    uint32_t best     = UINT32_MAX;
    uint32_t best_tag = UINT32_MAX;
    for (uint32_t i = 0; i < r->n_cells; i++) {
        if (cell_filled(r, i) == 0) continue;
        uint32_t t = cell_tag(r, i);
        if (best == UINT32_MAX || t < best_tag) {
            best     = i;
            best_tag = t;
        }
    }
    return best;
}
/* }}} */

/* {{{ slot_pop() */
int32_t slot_pop(slot_store_t *s, slot_id_t id,
                 void *buf, int32_t buf_size)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r)                                  return -1;
    if (r->flags & SLOT_FLAG_ATOMIC_COUNTER) return -1;
    if (buf_size < 0 || (!buf && buf_size > 0)) return -1;

    lock_slot(r);

    uint32_t pick;
    if (r->flags & SLOT_FLAG_TAGGED) {
        pick = find_lowest_tag_index(r);
        if (pick == UINT32_MAX) { unlock_slot(r); return -1; }
    } else {
        uint32_t head = atomic_load_explicit(&r->head, memory_order_acquire);
        uint32_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
        if (head == tail) { unlock_slot(r); return -1; }
        pick = head % r->n_cells;
    }

    uint32_t fs = cell_filled(r, pick);
    if (fs == 0)                  { unlock_slot(r); return -1; }
    if ((uint32_t)buf_size < fs)  { unlock_slot(r); return -1; }

    /* For LARGE_VALUE we capture the chunk handle here so we can
     * release it AFTER the slot lock is dropped — the allocator
     * takes its own mutex during ua_unref, and nesting the two is
     * fine but unnecessary if we can release the cheap lock first. */
    ua_chunk_t *released_chunk = NULL;

    if (fs > 0) {
        if (r->flags & SLOT_FLAG_LARGE_VALUE) {
            memcpy(&released_chunk, cell_data(r, pick), sizeof(void *));
            if (released_chunk) memcpy(buf, ua_data(released_chunk), fs);
        } else {
            memcpy(buf, cell_data(r, pick), fs);
        }
    }

    /* Mark cell empty. For LARGE_VALUE cells, also zero the handle
     * so a stale chunk pointer can't be misread by a later inspector
     * — the actual release happens after we drop the slot lock. */
    uint32_t zero = 0;
    memcpy(cell_ptr(r, pick), &zero, sizeof zero);
    if (r->flags & SLOT_FLAG_LARGE_VALUE) {
        ua_chunk_t *null_chunk = NULL;
        memcpy(cell_data(r, pick), &null_chunk, sizeof(void *));
    }

    /* For untagged: advance head by one. For tagged: walk head
     * forward over any contiguous empty cells (the head/tail
     * relationship is preserved so further pushes still land at the
     * right spot, but stale empties at the front of the ring don't
     * block future pushes that wrap). */
    if (!(r->flags & SLOT_FLAG_TAGGED)) {
        uint32_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
        atomic_store_explicit(&r->head, head + 1u, memory_order_release);
    } else {
        uint32_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
        uint32_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
        while (head < tail && cell_filled(r, head % r->n_cells) == 0) {
            head++;
        }
        atomic_store_explicit(&r->head, head, memory_order_release);
    }

    unlock_slot(r);

    /* Drop the consumer's reference to the chunk now that the slot
     * lock is gone. The bytes have already been copied into the
     * caller's buffer, so reclamation is safe. If the refcount goes
     * to zero, the chunk rejoins the allocator's free-lists with an
     * eager neighbor-merge — the line that turns the old leak into
     * proper reclamation. */
    if (released_chunk) ua_unref(s->heap, released_chunk);

    return (int32_t)fs;
}
/* }}} */

/* {{{ slot_read_inc() */
uint32_t slot_read_inc(slot_store_t *s, slot_id_t id, uint32_t mod)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r)                                     return UINT32_MAX;
    if (!(r->flags & SLOT_FLAG_ATOMIC_COUNTER)) return UINT32_MAX;
    if (mod == 0)                               return UINT32_MAX;

    uint32_t prev = atomic_fetch_add_explicit(&r->counter, 1u,
                                              memory_order_relaxed);
    return prev % mod;
}
/* }}} */

/* {{{ slot_has_value() */
int slot_has_value(slot_store_t *s, slot_id_t id)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r) return 0;
    if (r->flags & SLOT_FLAG_ATOMIC_COUNTER) return 1;

    if (r->flags & SLOT_FLAG_TAGGED) {
        /* Tagged: any filled cell counts. Cheap scan; tagged slots
         * are usually small. */
        lock_slot(r);
        int found = 0;
        for (uint32_t i = 0; i < r->n_cells; i++) {
            if (cell_filled(r, i) != 0) { found = 1; break; }
        }
        unlock_slot(r);
        return found;
    }

    uint32_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    return head != tail;
}
/* }}} */

/* {{{ slot_store_allocator() */
struct ua *slot_store_allocator(slot_store_t *s)
{
    return s ? s->heap : NULL;
}
/* }}} */

/* {{{ slot_store_large_value_bytes_in_use() */
/* A small accessor for the reclamation regression test in issue
 * 302. Exposes the unified allocator's in-use byte total without
 * leaking the allocator type into the slot store's public API. */
uint64_t slot_store_large_value_bytes_in_use(slot_store_t *s)
{
    if (!s || !s->heap) return 0;
    return ua_bytes_in_use(s->heap);
}
/* }}} */

/* {{{ slot_fill_count() */
int32_t slot_fill_count(slot_store_t *s, slot_id_t id)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r)                                  return 0;
    if (r->flags & SLOT_FLAG_ATOMIC_COUNTER) return 0;

    if (r->flags & SLOT_FLAG_TAGGED) {
        lock_slot(r);
        int32_t n = 0;
        for (uint32_t i = 0; i < r->n_cells; i++) {
            if (cell_filled(r, i) != 0) n++;
        }
        unlock_slot(r);
        return n;
    }

    uint32_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    uint32_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    return (int32_t)(tail - head);
}
/* }}} */
