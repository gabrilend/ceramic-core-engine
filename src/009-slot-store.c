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
#include <pthread.h>   /* alloc_lock — issue 319b */

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
     * retraction requires.
     *
     * For SLOT_FLAG_DUAL_RING slots, this is the native ring (the
     * one that holds same-language native bytes). The json ring
     * and ordering ring live in the dual-ring fields below. */
    uint8_t     *cells;
    ua_chunk_t  *cells_chunk;

    /* Dual-ring extension (issue 312). All four pointer fields are
     * NULL on non-dual slots; they're populated by slot_alloc when
     * SLOT_FLAG_DUAL_RING is set. The single per-slot lock above
     * covers all three rings — push_native writes (data + ordering)
     * atomically, push_json mirrors, and pop_ordered consumes both
     * the ordering tuple and the indicated data ring atomically.
     *
     * The json ring uses the same cell_capacity / cell_record / n_cells
     * as the native ring (one slot, one cell size). The ordering ring
     * has its own per-cell layout — see ORDER_CELL_RECORD below. */
    _Atomic uint32_t json_head;
    _Atomic uint32_t json_tail;
    uint8_t         *json_cells;
    ua_chunk_t      *json_cells_chunk;

    _Atomic uint32_t order_head;
    _Atomic uint32_t order_tail;
    uint8_t         *order_cells;
    ua_chunk_t      *order_cells_chunk;
} slot_rec_t;

/* Ordering-ring cell layout for SLOT_FLAG_DUAL_RING slots.
 *
 *   bytes 0..3 : which_ring (uint32; SLOT_RING_NATIVE or SLOT_RING_JSON)
 *   bytes 4..7 : idx        (uint32; cell index within the indicated ring)
 *
 * Total 8 bytes per cell. Head/tail track FIFO position; head == tail
 * means the ring is empty. No filled_size field — the order ring is
 * plain FIFO (no lowest-tag pop semantics), so head/tail are
 * sufficient to detect emptiness. */
#define ORDER_CELL_RECORD ((uint32_t)8)
/* }}} */

/* {{{ Internal store — two-level chunked-append index (issue 319b)
 *
 * Slot pointers are looked up via top-level chunk pointer table:
 *
 *     slot N lives at chunks[N / SLOT_CHUNK_SIZE][N % SLOT_CHUNK_SIZE]
 *
 * Why two levels: a flat realloc'd array (the original design) moves
 * its base address on growth, invalidating any slot pointer a worker
 * has cached. The two-level shape lets the slot RECORDS themselves
 * stay at their original heap addresses forever (they always were —
 * each is a separate malloc) AND lets the index that maps id → record
 * grow without invalidating cached records.
 *
 * Growth happens in three layers, each rarer than the last:
 *   Layer 1 — append a slot pointer inside an existing chunk
 *             (every slot_alloc — the common case)
 *   Layer 2 — allocate a new chunk when crossing a chunk boundary
 *             (every SLOT_CHUNK_SIZE alloc — uncommon)
 *   Layer 3 — grow the top-level chunk-pointer array when it fills
 *             (logarithmic in total slot count — rare)
 *
 * Concurrency: slot_alloc is serialized by alloc_lock. The hot
 * read/write path (slot_push, slot_pop, slot_peek, slot_flags) takes
 * no allocator-wide lock — only the per-slot atomic_flag spinlock on
 * the slot itself. Readers acquire-load `count` to know how many
 * slots exist; the matching release-store at the end of slot_alloc
 * pairs with this so a reader seeing count = N also sees the slot N-1
 * record published.
 *
 * Layer 3 (top-level growth) uses copy-on-grow with defer-free: the
 * new top-level array is malloc'd at 2x size, the old chunk pointers
 * are memcpy'd in, the new top is atomically published, and the OLD
 * top is parked on the stale_tops list to be freed at destroy time.
 * A worker that loaded the old top pointer right before the swap can
 * finish its lookup against it — every chunk the old top points to
 * is still alive. */
#define SLOT_CHUNK_SIZE      64u
#define INITIAL_TOP_CAPACITY 16u

struct stale_top {
    slot_rec_t      ***ptr;
    struct stale_top  *next;
};

struct slot_store {
    /* Top-level chunk-pointer table. Atomic because layer-3 growth
     * publishes a new table while readers are still walking the old
     * one. */
    _Atomic(slot_rec_t ***) chunks;
    _Atomic uint32_t        top_capacity;   /* in chunk-pointer slots */

    /* Total slots ever allocated. Atomic; readers see released
     * slot publications via the release-store at the end of
     * slot_alloc. */
    _Atomic uint32_t        count;

    /* Serializes layer-2 chunk creation and layer-3 top growth.
     * Readers don't take this — they walk the atomic structure. */
    pthread_mutex_t         alloc_lock;

    /* Defer-free list for stale top-level arrays. At most
     * log2(final_top_capacity) entries — typically 0–3 over an
     * entire run. Walked once at destroy. */
    struct stale_top       *stale_tops;

    /* The unified allocator. Owns the cell-array memory for every
     * ring slot in this store, plus the payload bytes for every
     * LARGE_VALUE cell. Created in slot_store_create; destroyed
     * alongside the store. Any chunks still parked in cells at
     * destroy time are reclaimed in bulk when the allocator's
     * regions are freed. */
    ua_t                   *heap;
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

    /* Initial top-level chunk-pointer table. 16 entries means we
     * can hold 16 * SLOT_CHUNK_SIZE = 1024 slots before any
     * top-level growth fires; growth doubles thereafter and there
     * is no upper ceiling (issue 319b). Chunks themselves are
     * allocated lazily on first crossing of each chunk boundary. */
    slot_rec_t ***top = calloc((size_t)INITIAL_TOP_CAPACITY,
                               sizeof(slot_rec_t **));
    if (!top) { free(s); return NULL; }

    atomic_init(&s->chunks,       top);
    atomic_init(&s->top_capacity, INITIAL_TOP_CAPACITY);
    atomic_init(&s->count,        0u);
    pthread_mutex_init(&s->alloc_lock, NULL);
    s->stale_tops = NULL;

    /* Eagerly create the unified allocator. Earlier in this issue
     * the allocator was lazy-created on the first LARGE_VALUE slot;
     * since the cell arrays of ordinary ring slots also live in the
     * allocator now, every slot allocation needs the allocator
     * present from the start. No pre-warmed classes — they accrete
     * on first use until the graph loader is wired to declare
     * sizes up front. */
    s->heap = ua_create(NULL, 0);
    if (!s->heap) {
        free(top);
        pthread_mutex_destroy(&s->alloc_lock);
        free(s);
        return NULL;
    }

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
    uint32_t count   = atomic_load_explicit(&s->count,        memory_order_relaxed);
    uint32_t top_cap = atomic_load_explicit(&s->top_capacity, memory_order_relaxed);
    slot_rec_t ***top = atomic_load_explicit(&s->chunks,      memory_order_relaxed);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t ci = i / SLOT_CHUNK_SIZE;
        uint32_t si = i % SLOT_CHUNK_SIZE;
        if (ci >= top_cap || !top[ci]) continue;
        slot_rec_t *r = top[ci][si];
        if (!r) continue;
        if (r->cells_chunk)       ua_unref(s->heap, r->cells_chunk);
        if (r->json_cells_chunk)  ua_unref(s->heap, r->json_cells_chunk);
        if (r->order_cells_chunk) ua_unref(s->heap, r->order_cells_chunk);
        free(r);
    }

    /* Free each chunk (those that were ever allocated). */
    for (uint32_t ci = 0; ci < top_cap; ci++) {
        if (top[ci]) free(top[ci]);
    }
    free(top);

    /* Free any stale top-level arrays that growth left behind.
     * Their chunks were the same chunks the current top points to
     * (we only copied the pointers in, didn't deep-copy), so
     * they've already been freed above. Just drop the array
     * memory. */
    struct stale_top *st = s->stale_tops;
    while (st) {
        struct stale_top *next = st->next;
        free(st->ptr);
        free(st);
        st = next;
    }

    pthread_mutex_destroy(&s->alloc_lock);
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
    /* Acquire-load pairs with the release-store in slot_alloc; a
     * reader that sees count = N also sees the slot N-1 record
     * fully published into its chunk. */
    return s ? (int32_t)atomic_load_explicit(&s->count, memory_order_acquire)
             : 0;
}
/* }}} */

/* slot_flags() lives further down — it needs the get_slot() helper
 * that's defined after slot_alloc(). */

/* {{{ Local helper — grow_top_level() (issue 319b layer 3)
 *
 * Called with alloc_lock held. Allocates a new top-level chunk-
 * pointer table of at least 2x the current size, copies the
 * current chunk pointers into it, atomically publishes the new
 * table, and parks the old one on the stale_tops free-list for
 * destroy-time reclamation. */
static int grow_top_level(slot_store_t *s, uint32_t needed_idx)
{
    uint32_t old_cap = atomic_load_explicit(&s->top_capacity,
                                            memory_order_relaxed);
    uint32_t new_cap = old_cap * 2u;
    while (new_cap <= needed_idx) new_cap *= 2u;

    slot_rec_t ***new_top = calloc((size_t)new_cap, sizeof(slot_rec_t **));
    if (!new_top) return -1;

    slot_rec_t ***old_top = atomic_load_explicit(&s->chunks,
                                                 memory_order_relaxed);
    memcpy(new_top, old_top, (size_t)old_cap * sizeof(slot_rec_t **));

    /* Park the old top-level pointer so any in-flight reader that
     * loaded it before the swap finishes safely. The chunks it
     * points to are the same chunks the new top points to (we only
     * copied the pointer values), so reads from either top resolve
     * to the same live chunks. */
    struct stale_top *st = malloc(sizeof *st);
    if (!st) { free(new_top); return -1; }
    st->ptr  = old_top;
    st->next = s->stale_tops;
    s->stale_tops = st;

    /* Publish the new top-level first, then the new capacity.
     * Readers that race-load the new chunks pointer and the old
     * capacity will compute id/SLOT_CHUNK_SIZE within the original
     * range, which is still valid. The reverse — old chunks, new
     * capacity — would let a reader index past the old top's
     * allocation, which would be UB. So: chunks first. */
    atomic_store_explicit(&s->chunks,       new_top, memory_order_release);
    atomic_store_explicit(&s->top_capacity, new_cap, memory_order_release);
    return 0;
}
/* }}} */

/* {{{ Local helper — ensure_chunk() (issue 319b layer 2)
 *
 * Called with alloc_lock held. Allocates the chunk at index `ci`
 * if it's not already populated, and publishes the chunk pointer
 * into the top-level table. */
static int ensure_chunk(slot_store_t *s, uint32_t ci)
{
    slot_rec_t ***top = atomic_load_explicit(&s->chunks,
                                             memory_order_relaxed);
    if (top[ci]) return 0;
    slot_rec_t **chunk = calloc((size_t)SLOT_CHUNK_SIZE,
                                sizeof(slot_rec_t *));
    if (!chunk) return -1;
    top[ci] = chunk;
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

    /* DUAL_RING constraints (slice 1 of issue 312):
     * - Doesn't combine with ATOMIC_COUNTER (no rings on counter slots).
     * - Doesn't combine with LARGE_VALUE or TAGGED yet — both could
     *   land here later, but slice 1 keeps the dual-ring path
     *   narrow to inline-byte cells. */
    if ((flags & SLOT_FLAG_DUAL_RING) &&
        (flags & (SLOT_FLAG_ATOMIC_COUNTER | SLOT_FLAG_LARGE_VALUE
                                           | SLOT_FLAG_TAGGED))) {
        return SLOT_INVALID;
    }

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
    atomic_init(&r->head,       0u);
    atomic_init(&r->tail,       0u);
    atomic_init(&r->counter,    0u);
    atomic_init(&r->json_head,  0u);
    atomic_init(&r->json_tail,  0u);
    atomic_init(&r->order_head, 0u);
    atomic_init(&r->order_tail, 0u);
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

    /* Dual-ring slots: allocate the JSON-ring cell array (same shape
     * as the native cell array) and the ordering ring (8 bytes per
     * cell). All three rings live in the same unified allocator so
     * they recycle on slot destroy like every other slot byte. */
    if (flags & SLOT_FLAG_DUAL_RING) {
        size_t json_bytes  = (size_t)n_cells * cell_record;
        size_t order_bytes = (size_t)n_cells * ORDER_CELL_RECORD;
        r->json_cells_chunk  = ua_alloc(s->heap, json_bytes);
        if (!r->json_cells_chunk) {
            if (r->cells_chunk) ua_unref(s->heap, r->cells_chunk);
            free(r);
            return SLOT_INVALID;
        }
        r->json_cells = ua_data(r->json_cells_chunk);
        memset(r->json_cells, 0, json_bytes);

        r->order_cells_chunk = ua_alloc(s->heap, order_bytes);
        if (!r->order_cells_chunk) {
            ua_unref(s->heap, r->json_cells_chunk);
            if (r->cells_chunk) ua_unref(s->heap, r->cells_chunk);
            free(r);
            return SLOT_INVALID;
        }
        r->order_cells = ua_data(r->order_cells_chunk);
        memset(r->order_cells, 0, order_bytes);
    }

    /* Publish into the two-level chunked index. The hot read path
     * is lock-free; only this rare allocation path takes the lock,
     * and only to serialize structural changes to the index
     * (layer-2 chunk creation, layer-3 top growth, count bump).
     * Workers' get_slot calls walk the atomic structure without
     * contending here. */
    pthread_mutex_lock(&s->alloc_lock);

    uint32_t id = atomic_load_explicit(&s->count, memory_order_relaxed);
    uint32_t ci = id / SLOT_CHUNK_SIZE;
    uint32_t si = id % SLOT_CHUNK_SIZE;

    /* Layer 3: top-level grows if the chunk index is past current
     * capacity. */
    if (ci >= atomic_load_explicit(&s->top_capacity, memory_order_relaxed)) {
        if (grow_top_level(s, ci) != 0) {
            pthread_mutex_unlock(&s->alloc_lock);
            if (r->cells_chunk)       ua_unref(s->heap, r->cells_chunk);
            if (r->json_cells_chunk)  ua_unref(s->heap, r->json_cells_chunk);
            if (r->order_cells_chunk) ua_unref(s->heap, r->order_cells_chunk);
            free(r);
            return SLOT_INVALID;
        }
    }

    /* Layer 2: allocate the chunk if it's the first slot inside it. */
    if (ensure_chunk(s, ci) != 0) {
        pthread_mutex_unlock(&s->alloc_lock);
        if (r->cells_chunk)       ua_unref(s->heap, r->cells_chunk);
        if (r->json_cells_chunk)  ua_unref(s->heap, r->json_cells_chunk);
        if (r->order_cells_chunk) ua_unref(s->heap, r->order_cells_chunk);
        free(r);
        return SLOT_INVALID;
    }

    /* Layer 1: publish the slot pointer into the chunk. The count
     * bump that follows is the release-store readers acquire-load
     * to know the slot exists. */
    slot_rec_t ***top = atomic_load_explicit(&s->chunks,
                                             memory_order_relaxed);
    top[ci][si] = r;

    atomic_store_explicit(&s->count, id + 1u, memory_order_release);

    pthread_mutex_unlock(&s->alloc_lock);
    return (slot_id_t)id;
}
/* }}} */

/* {{{ Local helper — get_slot() */
static slot_rec_t *get_slot(slot_store_t *s, slot_id_t id)
{
    if (!s || id < 0) return NULL;
    uint32_t uid = (uint32_t)id;
    /* Acquire-load count pairs with the release-store in slot_alloc.
     * If we see count = N then the slot at id = N-1 (and all earlier)
     * is fully published — the chunk pointer and the slot pointer
     * inside it. */
    if (uid >= atomic_load_explicit(&s->count, memory_order_acquire)) {
        return NULL;
    }
    uint32_t ci = uid / SLOT_CHUNK_SIZE;
    uint32_t si = uid % SLOT_CHUNK_SIZE;
    /* Acquire-load the top so we see the chunk publishes that the
     * release-store in grow_top_level orders before its top swap.
     * After top growth, both the old and new top arrays point to
     * the same chunks — either is safe to read from. */
    slot_rec_t ***top = atomic_load_explicit(&s->chunks,
                                             memory_order_acquire);
    return top[ci][si];
}
/* }}} */

/* {{{ slot_push() */
int slot_push(slot_store_t *s, slot_id_t id,
              const void *data, int32_t size, uint32_t tag)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r)                                      return -1;
    if (r->flags & SLOT_FLAG_ATOMIC_COUNTER)     return -1;
    /* Dual-ring slots require the caller to pick a ring via
     * slot_push_native or slot_push_json — using the plain push
     * would skip the ordering ring and break read order. */
    if (r->flags & SLOT_FLAG_DUAL_RING)          return -1;
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
    /* Peek on dual-ring would have to choose a ring to peek, and
     * "the next" isn't meaningful without consulting the ordering
     * ring. Dual-ring slots are pop-only via slot_pop_ordered. */
    if (r->flags & SLOT_FLAG_DUAL_RING)      return -1;
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
    /* Dual-ring slots require slot_pop_ordered — popping the native
     * ring directly would let the consumer skip cells whose ordering
     * entry says to pop from the JSON ring next. */
    if (r->flags & SLOT_FLAG_DUAL_RING)      return -1;
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

/* {{{ Dual-ring helpers (issue 312)
 *
 * The three rings (native data, JSON data, ordering) all live on the
 * same slot_rec and share one lock. A push writes (data cell +
 * ordering entry) under the lock — readers either see both writes or
 * neither.
 *
 * Data-ring cell layout for dual-ring slots is the standard layout
 * minus the tag field (DUAL_RING doesn't combine with TAGGED in
 * slice 1): bytes 0..3 are filled_size, bytes 4.. are payload. The
 * ordering ring uses ORDER_CELL_RECORD (8 bytes — 4 for which_ring,
 * 4 for idx); empty-cell detection is via head/tail, not filled_size.
 *
 * Sizing: all three rings have n_cells cells. The order ring is the
 * binding constraint on total in-flight pushes (n_cells across both
 * data rings combined). Callers that need more pending pushes pass
 * a larger n_cells. */

/* dual_push_locked() — write `data` into the indicated data ring at
 * its tail, write the (which_ring, idx) tuple into the ordering ring
 * at its tail, advance both tails. Caller holds the slot lock.
 * Returns 0 on success, -1 if either ring is full. */
static int dual_push_locked(slot_rec_t *r, int32_t which_ring,
                            const void *data, int32_t size)
{
    _Atomic uint32_t *data_head, *data_tail;
    uint8_t          *data_cells;
    if (which_ring == SLOT_RING_NATIVE) {
        data_head  = &r->head;
        data_tail  = &r->tail;
        data_cells = r->cells;
    } else {
        data_head  = &r->json_head;
        data_tail  = &r->json_tail;
        data_cells = r->json_cells;
    }

    uint32_t dh = atomic_load_explicit(data_head,     memory_order_relaxed);
    uint32_t dt = atomic_load_explicit(data_tail,     memory_order_relaxed);
    uint32_t oh = atomic_load_explicit(&r->order_head, memory_order_relaxed);
    uint32_t ot = atomic_load_explicit(&r->order_tail, memory_order_relaxed);
    if (dt - dh >= r->n_cells) return -1;
    if (ot - oh >= r->n_cells) return -1;

    uint32_t di = dt % r->n_cells;
    uint8_t *dp = data_cells + (size_t)di * r->cell_record;
    uint32_t fs = (uint32_t)size;
    memcpy(dp, &fs, sizeof fs);
    if (size > 0) memcpy(dp + 4, data, (size_t)size);

    uint32_t oi = ot % r->n_cells;
    uint8_t *op = r->order_cells + (size_t)oi * ORDER_CELL_RECORD;
    uint32_t which_u32 = (uint32_t)which_ring;
    memcpy(op,     &which_u32, sizeof which_u32);
    memcpy(op + 4, &di,        sizeof di);

    atomic_store_explicit(data_tail,      dt + 1u, memory_order_release);
    atomic_store_explicit(&r->order_tail, ot + 1u, memory_order_release);
    return 0;
}
/* }}} */

/* {{{ slot_push_native() */
int slot_push_native(slot_store_t *s, slot_id_t id,
                     const void *data, int32_t size, uint32_t tag)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r) return -1;

    /* Non-dual: alias to slot_push. Callers can use slot_push_native
     * uniformly across both shapes; only the dual-ring path consults
     * the ordering ring. */
    if (!(r->flags & SLOT_FLAG_DUAL_RING))
        return slot_push(s, id, data, size, tag);

    if (r->flags & SLOT_FLAG_ATOMIC_COUNTER)         return -1;
    if (size < 0)                                    return -1;
    if (!data && size > 0)                           return -1;
    if ((uint32_t)size > r->cell_capacity)           return -1;

    lock_slot(r);
    int rc = dual_push_locked(r, SLOT_RING_NATIVE, data, size);
    unlock_slot(r);
    return rc;
}
/* }}} */

/* {{{ slot_push_json() */
int slot_push_json(slot_store_t *s, slot_id_t id,
                   const void *data, int32_t size, uint32_t tag)
{
    (void)tag;  /* dual-ring slice 1 doesn't combine with TAGGED */
    slot_rec_t *r = get_slot(s, id);
    if (!r)                                          return -1;
    /* JSON ring only exists on dual-ring slots. */
    if (!(r->flags & SLOT_FLAG_DUAL_RING))           return -1;
    if (size < 0)                                    return -1;
    if (!data && size > 0)                           return -1;
    if ((uint32_t)size > r->cell_capacity)           return -1;

    lock_slot(r);
    int rc = dual_push_locked(r, SLOT_RING_JSON, data, size);
    unlock_slot(r);
    return rc;
}
/* }}} */

/* {{{ slot_pop_ordered() */
int32_t slot_pop_ordered(slot_store_t *s, slot_id_t id,
                         void *buf, int32_t buf_size,
                         int32_t *out_which_ring)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r) return -1;

    /* Non-dual: alias to slot_pop; report NATIVE. */
    if (!(r->flags & SLOT_FLAG_DUAL_RING)) {
        if (out_which_ring) *out_which_ring = SLOT_RING_NATIVE;
        return slot_pop(s, id, buf, buf_size);
    }

    if (r->flags & SLOT_FLAG_ATOMIC_COUNTER)         return -1;
    if (buf_size < 0 || (!buf && buf_size > 0))      return -1;

    lock_slot(r);

    /* Read the next ordering entry. Empty ring → nothing to pop. */
    uint32_t oh = atomic_load_explicit(&r->order_head, memory_order_acquire);
    uint32_t ot = atomic_load_explicit(&r->order_tail, memory_order_acquire);
    if (oh == ot) { unlock_slot(r); return -1; }

    uint32_t oi = oh % r->n_cells;
    uint8_t *op = r->order_cells + (size_t)oi * ORDER_CELL_RECORD;
    uint32_t which_ring, idx;
    memcpy(&which_ring, op,     sizeof which_ring);
    memcpy(&idx,        op + 4, sizeof idx);

    /* Pick the indicated data ring's head pointer and cell array. */
    _Atomic uint32_t *data_head;
    uint8_t          *data_cells;
    if (which_ring == (uint32_t)SLOT_RING_NATIVE) {
        data_head  = &r->head;
        data_cells = r->cells;
    } else {
        data_head  = &r->json_head;
        data_cells = r->json_cells;
    }

    uint8_t *dp = data_cells + (size_t)idx * r->cell_record;
    uint32_t fs;
    memcpy(&fs, dp, sizeof fs);
    if (fs == 0)                  { unlock_slot(r); return -1; }
    if ((uint32_t)buf_size < fs)  { unlock_slot(r); return -1; }

    if (fs > 0) memcpy(buf, dp + 4, fs);

    /* Mark the data cell empty. */
    uint32_t zero = 0;
    memcpy(dp, &zero, sizeof zero);

    /* Advance both heads. The data ring's head matches the ordering
     * entry's idx at this point (entries are popped strictly in push
     * order, and each data ring fills sequentially), so head + 1 is
     * always the next-to-pop cell on that ring. */
    uint32_t dh = atomic_load_explicit(data_head, memory_order_relaxed);
    atomic_store_explicit(data_head,      dh + 1u, memory_order_release);
    atomic_store_explicit(&r->order_head, oh + 1u, memory_order_release);

    unlock_slot(r);

    if (out_which_ring) *out_which_ring = (int32_t)which_ring;
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

    if (r->flags & SLOT_FLAG_DUAL_RING) {
        /* Dual-ring: presence is dictated by the ordering ring, since
         * pop is order-driven. The two data rings may have unconsumed
         * cells beyond the order ring's tail (impossible in practice
         * because every push writes both atomically) but the order
         * ring is the authority. */
        uint32_t oh = atomic_load_explicit(&r->order_head, memory_order_acquire);
        uint32_t ot = atomic_load_explicit(&r->order_tail, memory_order_acquire);
        return oh != ot;
    }

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

/* {{{ slot_flags() */
int32_t slot_flags(slot_store_t *s, slot_id_t id)
{
    slot_rec_t *r = get_slot(s, id);
    if (!r) return -1;
    return (int32_t)r->flags;
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

    if (r->flags & SLOT_FLAG_DUAL_RING) {
        /* Dual-ring fill = order ring length. That's the count of
         * cells the consumer will see across N pops. */
        uint32_t oh = atomic_load_explicit(&r->order_head, memory_order_acquire);
        uint32_t ot = atomic_load_explicit(&r->order_tail, memory_order_acquire);
        return (int32_t)(ot - oh);
    }

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
