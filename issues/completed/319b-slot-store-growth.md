# 319b — Slot store growth primitive

## Status
complete

## Parent issue
First foundational slice of issue 319's runtime side. Full
design in parent issue 319's "Q1" resolution.

## Current behavior

`src/009-slot-store.c` uses a two-level chunked-append index for
slot pointers. Each chunk holds 64 slot pointers; the top-level
chunk-pointer table starts at 16 entries (1024 slots before
top-level growth) and doubles when it fills. Top-level growth is
copy-on-grow with defer-free: the old top-level array is parked
on a `stale_tops` list so any worker that loaded a stale
top-pointer mid-growth completes its lookup safely against it,
then the parked arrays are freed at `slot_store_destroy`.

`slot_alloc` is serialized by `pthread_mutex_t alloc_lock`.
The hot read/write path (`slot_push`, `slot_pop`, `slot_peek`,
`slot_flags`) takes no allocator-wide lock — the per-slot
`atomic_flag` spinlock remains the only hot-path synchronization.
Readers acquire-load `count`; the matching release-store at the
end of `slot_alloc` orders the slot publish before the reader
can see it.

Slots and chunks themselves never move. Once `slot_alloc`
returns an id, the underlying slot record's address and the
chunk it lives in are stable for the run.

## Intended behavior

Replace the flat `slots[]` array with a two-level chunked-append
structure:

- **Chunk size**: 64 slot-pointers per chunk (cache-friendly).
- **Top-level array**: holds chunk pointers; starts at 16
  entries (1024 slots without top-level growth). Doubles via
  copy-and-publish with defer-free when it fills.
- **Atomic count**: tracks total slots ever allocated. Readers
  use acquire-load to see slots published with release-store.
- **`pthread_mutex_t alloc_lock`**: serializes `slot_alloc`.
  Readers (push/pop via `get_slot`) take no lock here — the
  per-slot spinlock is still the only hot-path synchronization.
- **Stale-top free list**: top-level arrays from prior growth
  events stay alive on a linked list until `slot_store_destroy`.
  Any worker that cached a stale top-pointer mid-growth can
  finish its lookup safely; the chunks the stale top points to
  are still valid because chunks never move.

Slot lookup: `top[id/64][id%64]`. One extra indirection per
access vs the old flat array, in exchange for the runtime-safe
growth.

## Suggested implementation

1. Change the `slot_store` struct definition (private in the
   .c file; header has only the opaque forward declaration so
   no public API change).
2. Add `SLOT_CHUNK_SIZE` (64) and `INITIAL_TOP_CAPACITY` (16).
3. Rewrite `slot_store_create` to allocate the initial top-level
   array, init the mutex, init the atomic count.
4. Rewrite `slot_store_destroy` to iterate over chunks, free
   slot records and each chunk, then free the stale-top list.
5. Replace `grow_store_if_needed` with `grow_top_level` (called
   under `alloc_lock`).
6. Rewrite `slot_alloc`: take the lock, claim id, ensure chunk
   exists, publish slot, release-store the count.
7. Rewrite `get_slot` to walk the chunked structure.
8. Rewrite `slot_store_size` to acquire-load the atomic count.
9. Add a test that forces multiple top-level growth events
   (allocate ≥ 2048 slots to require at least one growth).
10. Run the full slot-store unit suite and the integration
    suite — all 22 + 12 must still pass.

## Relevant files

- `src/009-slot-store.c` — the structure change.
- `src/009-slot-store.h` — header stays unchanged (opaque type).
- `tests/009-slot-store-test.c` — add growth test.

## Not in scope

Ring buffer cell growth (when a single slot's ring fills with
pushed values waiting for consumption) — separate concern, same
double-buffer pattern but under the per-slot lock. Tracked
implicitly under the slot store's roadmap; punt to a follow-on
sub-issue.

## Parent issue
Sub-issue of 319 (built-in library for map self-construction).
The design for this slice is fully specified in 319's "Q1 —
When does the new box exist?" resolution section.

## Current behavior

`src/009-slot-store.{c,h}` allocates all slots at graph load.
The store has no growth primitive; slot pointers are stable for
the run because the array never moves. The header explicitly
states: "Lifetime is the run. Slots are freed when the store is
destroyed at the end of the run."

## Intended behavior

The slot store grows at runtime via a three-layer chunked-append
+ RCU-swap structure (see 319's Q1 resolution for the full
diagram and rationale). Slots themselves never move; chunks
themselves never move; only the top-level index array swaps when
it fills, and the swap uses double-buffer-with-defer-free.

## Suggested implementation

To be expanded when this sub-issue is picked up. The design is
already final in 319.

Touches: `src/009-slot-store.{c,h}`, `tests/009-slot-store-test.c`.

## Not in scope

Per-slot ring buffer growth (separate concern — same double-buffer
pattern under the per-slot lock, but a different code path).
