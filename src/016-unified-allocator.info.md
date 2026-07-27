# 016-unified-allocator.c/.h — public surface

The one run-time allocator from issue 302's "Allocation strategy."
Before a run, graph analysis counts every distinct output size the
boxes declare and how many chunks each size needs; the allocator
pre-builds one free-list per size, stocked to that demand. During
the run, allocations pop a fitting chunk in O(1); frees are
refcount drops that eagerly fuse with free physical neighbors, so
adjacent freed memory recombines into larger chunks instead of
fragmenting. When no free-list can satisfy a request, a deep sweep
fuses stragglers first; only if that fails does the heap grow by
taking a new region from the operating system.

Two opaque handles: `ua_t` (the heap) and `ua_chunk_t` (one
allocation). One mutex covers the whole heap; per-class locking is
the documented next step.

## Lifecycle

- `ua_t *ua_create(const ua_class_decl_t *classes, size_t n_classes)`
  — builds a heap pre-warmed per the declared (size, count) pairs.
  Sizes need not be sorted; duplicates are merged with counts
  summed; a NULL/zero declaration list means no pre-warming.
  Returns NULL on allocation failure or any zero-size class.
- `void ua_destroy(ua_t *h)` — frees every region and the heap
  itself. Safe on NULL. Invalidates every chunk from this heap.

## Allocation

- `ua_chunk_t *ua_alloc(ua_t *h, size_t size)` — returns a chunk of
  at least `size` bytes with reference count 1. Lookup is "smallest
  free-list whose class size covers the request"; falls through to
  larger classes, then a mandatory sweep, then heap growth. NULL on
  failure, NULL heap, or zero size.
- `ua_chunk_t *ua_ref(ua_chunk_t *chunk)` — bumps the reference
  count; returns the chunk for chaining. No-op on NULL.
- `void ua_unref(ua_t *h, ua_chunk_t *chunk)` — drops the count;
  at zero the chunk returns to a free-list and the O(1) eager
  neighbor-merge runs. No-op on NULL.
- `void *ua_data(ua_chunk_t *chunk)` / `size_t ua_size(ua_chunk_t *chunk)`
  — the caller-usable payload pointer and its byte capacity (which
  may exceed the requested size). NULL/0 on NULL chunk.

## Sweep

- `uint32_t ua_sweep(ua_t *h)` — walks every region in address
  order and fuses runs of contiguous free chunks the cheap
  per-free merge couldn't catch. Returns the merge count. Runs
  automatically inside a failing allocation before the heap grows;
  callers may also invoke it at quiescence points (e.g. between
  dispatch batches).

## Stats (test / debug / future telemetry)

All read-only, safe on NULL heap:
`ua_bytes_in_use`, `ua_bytes_free`, `ua_bytes_total`,
`ua_region_count`, `ua_alloc_calls`, `ua_free_calls` (unrefs that
reached zero), `ua_neighbor_merges`, `ua_sweep_merges`,
`ua_grow_calls`, and `ua_class_free_count(h, size)` — how many
free chunks currently sit on the exact-size class's list.

## Who allocates here

Everything on a hot path, which is why this is the one heap:

- `009-slot-store.c` — slot cells, and the payload bytes of
  `SLOT_FLAG_LARGE_VALUE` slots. A push acquires a chunk; a pop
  copies the bytes out and drops the reference.
- `012-dispatch.c` — the task structs themselves, and the input /
  output buffers each fire reads and writes through.
- `008-pool-runner.c` — calls `ua_sweep` at the run's quiescence
  point.

This replaced `015-large-value-heap.c`, which had no reclamation.
That module is still in the tree but has no callers.

## What's NOT here (deferred)

- **Oversized-chunk slicing.** A small request that pops a large
  chunk keeps the whole chunk; the tail is not split off. The
  pre-warmed classes and eager merging cover the common case.
- **Per-class locks.** One allocator-wide mutex for now — simple
  and correct first. This is the documented next step and the
  most likely contention point under many workers.
- **Producer-declared output sizes into `ua_create`.** The graph
  loader enumerates size classes, but they are not yet all piped
  through to pre-warming, so some classes are learned by
  accretion rather than declared up front.

## Related

- Issue 302 — design (the 2026-05-19 rewrite).
- `src/009-slot-store.info.md` — the largest consumer.
- `docs/007-architecture.md` — where this sits in the stack.
