/* src/009-slot-store.h — per-input-port slot store, public API.
 *
 * What it is, in a sentence: a thread-safe pool of ring-buffer
 * slots, allocated at graph load time, used by the phase 3
 * dispatch layer to route values between boxes.
 *
 * Slots belong to input ports, not tasks. Each box has one slot
 * per input port; the slot persists for the whole run. A wire is
 * a routing declaration — when a producer's task finishes, the
 * dispatch layer pushes copies of the output into every
 * downstream input slot. Fan-out is N pushes; fan-in is N
 * producers pushing into one slot. Designed in issue 302.
 *
 * Three slot modes, chosen via flags at slot_alloc:
 *
 *   1-cell peek (n_cells == 1, no flags)
 *     — for literals and wires from producers that run exactly
 *       once. Push once at startup. Every consumer task peeks
 *       without draining; the cell stays filled.
 *
 *   N-cell pop (n_cells > 1, no flags)
 *     — for wires whose producer runs many times (iterators,
 *       queued inputs). Push appends to tail; pop drains head
 *       FIFO. Consumer task pops one cell per spawn.
 *
 *   N-cell pop, tagged (SLOT_TAGGED)
 *     — same shape as N-cell pop, but pop returns the cell with
 *       the lowest tag, not the head. Used downstream of parallel
 *       iterators where push order is non-deterministic but
 *       consumption order must follow the iterator counter.
 *
 *   atomic counter (SLOT_ATOMIC_COUNTER, n_cells and cell_capacity
 *     ignored)
 *     — single atomic uint32. Read-and-increment via
 *       slot_read_inc(). Used for iterator / randomizer /
 *       weighted-routing counters. Concurrent reads each get a
 *       distinct value mod the caller-supplied bound.
 *
 * Synchronization is per-slot. There is no allocator-wide lock on
 * the hot read/write path. The atomic-counter slot uses
 * atomic_fetch_add; the ring-buffer slots use a per-slot spinlock
 * around header mutation (one byte, atomic_flag).
 *
 * Lifetime is the run. Slots are freed when the store is
 * destroyed at the end of the run — there is no per-slot refcount
 * and no end-of-stream propagation. Run termination is governed
 * by the pool's active-task counter (issue 301).
 *
 * Errors: a return of -1 from a read/write op means the operation
 * could not be performed (empty pop, buffer too small, type
 * mismatch). The store does not assert or abort; the dispatch
 * layer is expected to check return codes. The hard-crash policy
 * applies at the language-spec layer, not here.
 *
 * What's NOT in this version:
 *  - Size-class free lists and coalescing. The design in issue 302
 *    describes a segregated allocator with eager-coalesce; this
 *    initial implementation uses plain malloc per slot. The pattern
 *    is "allocate once at graph load, free at run end", so free-list
 *    optimization buys nothing for the current usage. Lands as a
 *    follow-on if profiling justifies it.
 *  - Large-value heap for variable-size payloads. Slots store
 *    fixed-size cells; very large outputs (LLM responses) will
 *    use a separate two-tier scheme. Follow-on within 302.
 */

#ifndef SORAMECH_SLOT_STORE_H
#define SORAMECH_SLOT_STORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* {{{ Types & flags */
typedef struct slot_store slot_store_t;
typedef int32_t           slot_id_t;

#define SLOT_INVALID  ((slot_id_t)-1)

/* slot_alloc flags */
enum {
    SLOT_FLAG_NONE           = 0,
    SLOT_FLAG_TAGGED         = 1 << 0,
    SLOT_FLAG_ATOMIC_COUNTER = 1 << 1,
};
/* }}} */

/* {{{ Store lifecycle */
/* Construct an empty store. Returns NULL on allocation failure. */
slot_store_t *slot_store_create(void);

/* Free every slot and the store itself. Safe to call on NULL. */
void          slot_store_destroy(slot_store_t *s);
/* }}} */

/* {{{ Slot lifecycle */
/* Allocate a slot.
 *
 *   cell_capacity — bytes per cell. Ignored for SLOT_FLAG_ATOMIC_COUNTER.
 *   n_cells       — ring size (1 for peek slots, >1 for pop). Ignored for
 *                   SLOT_FLAG_ATOMIC_COUNTER.
 *   flags         — zero, SLOT_FLAG_TAGGED, or SLOT_FLAG_ATOMIC_COUNTER.
 *
 * Returns the new slot's id, or SLOT_INVALID on allocation failure or
 * invalid arguments. Ids are stable for the lifetime of the store. */
slot_id_t     slot_alloc(slot_store_t *s,
                         int32_t cell_capacity,
                         int32_t n_cells,
                         int32_t flags);
/* }}} */

/* {{{ Ring-buffer ops */
/* Push a value into the slot's next free cell.
 *
 *   data, size — value bytes. size must be <= cell_capacity.
 *   tag        — ordering tag. Carried only on SLOT_FLAG_TAGGED slots;
 *                ignored otherwise.
 *
 * Returns 0 on success, -1 if the slot is full or arguments invalid.
 * Not valid on SLOT_FLAG_ATOMIC_COUNTER slots (returns -1). */
int           slot_push(slot_store_t *s, slot_id_t id,
                        const void *data, int32_t size, uint32_t tag);

/* Peek at the head cell without draining it.
 *
 *   buf, buf_size — destination. The number of bytes copied is min of
 *                   the cell's filled_size and buf_size.
 *
 * Returns the cell's filled_size on success, -1 if the slot is empty,
 * the buffer is too small, or the slot is the wrong type. The cell
 * remains filled and may be re-peeked by subsequent tasks (1-cell peek
 * mode). */
int32_t       slot_peek(slot_store_t *s, slot_id_t id,
                        void *buf, int32_t buf_size);

/* Drain one cell from the slot.
 *
 *   buf, buf_size — destination.
 *
 * For untagged slots, returns the head cell and advances head FIFO.
 * For SLOT_FLAG_TAGGED slots, returns the cell with the lowest tag
 * among the filled cells and marks that cell empty (head advances as
 * front-of-ring cells empty).
 *
 * Returns the cell's filled_size on success, -1 on empty / wrong type
 * / buffer too small. */
int32_t       slot_pop(slot_store_t *s, slot_id_t id,
                       void *buf, int32_t buf_size);

/* Atomic read-and-increment on SLOT_FLAG_ATOMIC_COUNTER slots.
 *
 *   mod — modulus for the returned value. Pass UINT32_MAX for "no
 *         modulus".
 *
 * Returns the pre-increment counter value mod the supplied bound.
 * Concurrent readers each get a distinct value (the underlying
 * counter is incremented atomically). Returns UINT32_MAX on type
 * mismatch — the dispatch layer is expected to call this op only
 * on slots it allocated with SLOT_FLAG_ATOMIC_COUNTER. */
uint32_t      slot_read_inc(slot_store_t *s, slot_id_t id, uint32_t mod);

/* Check whether the slot has any filled cell available for read.
 * Used by the dispatch layer's spawn-on-input-ready rule (issue 304).
 *
 * Returns 1 if a value is available, 0 otherwise. For ATOMIC_COUNTER
 * slots, always returns 1 (the counter is always readable). */
int           slot_has_value(slot_store_t *s, slot_id_t id);
/* }}} */

/* {{{ Inspection (test / debug) */
/* Number of filled cells. For ATOMIC_COUNTER slots, returns 0. */
int32_t       slot_fill_count(slot_store_t *s, slot_id_t id);

/* Slot count in the store. */
int32_t       slot_store_size(slot_store_t *s);
/* }}} */

#ifdef __cplusplus
}
#endif

#endif /* SORAMECH_SLOT_STORE_H */
