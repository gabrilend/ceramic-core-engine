# 015-large-value-heap.info.md

> **This module has no callers. It is not part of the running
> system.**
>
> It was written as the variable-size payload allocator for the
> slot store's `SLOT_FLAG_LARGE_VALUE` slots. The unified
> allocator (`016-unified-allocator.c`) then took that job — it
> refcounts and reclaims, which this heap cannot do — and
> `009-slot-store.c` was moved onto it. Nothing was left pointing
> here: no production file includes `015-large-value-heap.h`.
>
> It still compiles into `soramech-pool`, because the Makefile
> globs `src/*.c`, and its unit test still passes. That test is
> the only thing exercising it. A green test on an unreachable
> module is exactly how this stayed invisible.
>
> Removing it is unclaimed work. Anything below describes a
> component that is built but not wired in — do not reach for it
> when adding a feature; reach for `016-unified-allocator.c`.

Variable-size payload allocator. Designed in issue 302;
implemented 2026-05-19; superseded by the unified allocator.

## Mental model

A chunked arena. The heap is a singly-linked list of `malloc`'d
chunks. Each `lvh_alloc` either bump-allocates within the current
(head) chunk or, if the request doesn't fit, allocates a fresh
chunk and prepends it.

Allocations are **never individually freed**. The entire heap is
destroyed at run end (matching the slot store's lifetime).

Pointers returned by `lvh_alloc` are stable — chunks don't move
once allocated, so a consumer's pointer outlives any number of
later producer allocations.

## API

```c
lvh_t   *lvh_create(size_t default_chunk_size);   /* 0 → 64 KB */
void     lvh_destroy(lvh_t *h);

void    *lvh_alloc(lvh_t *h, size_t size);        /* stable, 16-byte aligned */

uint64_t lvh_total_allocated(lvh_t *h);           /* sum of chunk caps */
uint64_t lvh_total_used(lvh_t *h);                /* sum of used bytes */
uint32_t lvh_chunk_count(lvh_t *h);
```

## Behaviors worth knowing

- **Thread-safe.** A single internal mutex serializes `lvh_alloc`.
  The user's memcpy of payload bytes runs outside the lock —
  contention is brief.
- **Oversized allocations get their own chunk.** A 5 MB request
  with a 64 KB default chunk size produces a 5 MB chunk, not a
  bump within a 64 KB chunk. This keeps fragmentation predictable
  and stops one large value from pinning a chunk forever.
- **Zero-size allocations succeed.** They return a stable non-NULL
  pointer (1 byte rounded to 16 alignment), so callers don't need
  to special-case empty payloads. NULL strictly means "out of
  memory".
- **No free.** Long iterator loops that produce large values
  monotonically grow the heap. Track `lvh_total_used` to detect.
  A proper free-on-pop allocator is a future iteration.

## Out of scope

- Per-allocation free (`lvh_free`).
- Cross-process visibility (heap is in-process; out-of-process
  language specs receive bytes via existing socket plumbing).
- Concurrent chunk allocation with no mutex (thread-local chunk
  scheme is the planned upgrade path if profiling justifies it).

## Tests

`tests/015-large-value-heap-test.c`:
- `create_destroy` — round-trip with no allocations.
- `single_alloc_within_chunk` — write/read bytes through the
  returned pointer.
- `multiple_allocs_within_chunk` — independent pointers, no
  overlap.
- `oversized_alloc` — request larger than chunk size; verify it
  gets its own chunk.
- `pointer_stability_across_growth` — alloc, fill chunks past
  default size to force growth, original pointer still readable.
- `zero_size_alloc` — non-NULL return, no crash.
- `concurrent_alloc` — eight threads each doing 1000 allocs;
  every returned pointer is unique and writable.
- `stats_match_reality` — total_used / chunk_count line up with
  what was asked for.
