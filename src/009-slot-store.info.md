# 009-slot-store.c — public surface

Per-input-port slot store. Allocated once at graph load; each box
input port owns one slot, freed when the run ends. The dispatch
layer reads and writes through this API; language specs never see
slot IDs (issue 303 keeps that boundary clean).

## Lifecycle

- `slot_store_t *slot_store_create(void)` — empty store; NULL on OOM.
- `void slot_store_destroy(slot_store_t *s)` — frees every slot.

## Allocation

- `slot_id_t slot_alloc(store, cell_capacity, n_cells, flags)`
  - Returns `SLOT_INVALID` on bad args / OOM.
  - Flags: `SLOT_FLAG_NONE`, `SLOT_FLAG_TAGGED`, `SLOT_FLAG_ATOMIC_COUNTER`.
  - `cell_capacity` and `n_cells` are ignored for atomic-counter slots.

## Ring-buffer ops (push / peek / pop / has_value / fill_count)

- `int slot_push(store, id, data, size, tag)` — 0 on success, -1 on
  full / wrong type / size too big.
- `int32_t slot_peek(store, id, buf, buf_size)` — returns the cell's
  filled_size on success; -1 on empty / wrong type / buf too small.
  Does not advance head. Used for 1-cell peek slots.
- `int32_t slot_pop(store, id, buf, buf_size)` — drains. Untagged
  pops head FIFO; tagged pops the lowest-tag filled cell.
- `int slot_has_value(store, id)` — 1 if a value is available.
- `int32_t slot_fill_count(store, id)` — number of filled cells.

## Atomic-counter slot

- `uint32_t slot_read_inc(store, id, mod)` — fetch-and-add. Returns
  the pre-increment counter value mod the supplied bound.
  `UINT32_MAX` on wrong type or `mod == 0`.

## Thread safety

- The atomic-counter slot uses `atomic_fetch_add_explicit`; no lock.
- Ring-buffer slots use a per-slot `atomic_flag` spinlock around
  header + cell mutations.
- The store itself is single-threaded for allocation (graph-load
  phase); after that, the slot array is read-only and per-slot
  operations run concurrently.

## What's NOT here (deferred)

- Size-class free lists / coalescing — plain `malloc` per slot for
  now. The current usage pattern doesn't reuse slot memory, so
  optimization buys nothing yet.
- Large-value heap for variable-size payloads. Follow-on within
  issue 302.

## Related

- Issue 302 — design.
- Issue 304 — the dispatch layer that calls this API.
- Issue 301 — the pool whose workers contend on these slots.
