# 319a — Remove dispatch input cap (prerequisite for create_box)

## Status
complete

## Parent issue
First slice of issue 319 (built-in library for map
self-construction). The full design lives in 319; this sub-issue
implemented the prerequisite from 319's "Prerequisite — remove
the dispatch input cap" section.

## Current behavior

`src/012-dispatch.c` sizes its per-call input arrays to the box's
actual input count using C99 VLAs, bounded by a stack-frame
sanity ceiling `DISPATCH_MAX_INPUTS_PER_BOX` (currently 4096). A
box with N inputs gets five parallel arrays of length max(N, 1)
(the size-1 floor avoids the zero-sized VLA UB; the extra slot
is never touched because the read loop runs exactly N times).
Both `invoke_box_impl()` and `do_write_box()` use the pattern.

The five branch-name buffers used by the routing kinds
(comparator, iterator, randomizer, weighted, distributor) share
a single named constant `MAX_BRANCH_NAME` (32 bytes — well above
the worst-case `"out_-2147483648\0"` = 16 bytes). Any future
format-string change touches one definition.

Test fixture `tests/maps/319a-many-inputs/` exercises a 20-input
Lua call box end-to-end; output `count=20 sum=210` confirms all
20 values reach the spec.

## Intended behavior

The five parallel input arrays size themselves at call time to
the box's actual input count, with a sanity ceiling that lets us
reject pathological values (a box with a million inputs is almost
certainly a graph-loader bug, not a real use case). No fixed
cap of 16. A box with 17 inputs runs the same as a box with 7.

The five branch buffers use a single named constant
`MAX_BRANCH_NAME` whose size is provably sufficient for the
`"out_%u"` format. The constant lives at file scope so any
future format-string change touches one definition.

## Suggested implementation

1. **Choose the array-sizing approach.** Two acceptable shapes:
   - **VLA**: `char *bufs[n > 0 ? n : 1]; memset(bufs, 0, sizeof bufs);`
     for each of the five arrays. C99 standard. Zero stack cost
     for typical n. Cost: each array gets a tiny init loop;
     zero-size VLAs are UB so the `n > 0` guard is required.
   - **SBO**: keep the `[16]` stack arrays as a fast path; fall
     back to `calloc` when `n > 16`; free at end. Common case
     pays zero overhead; rare case pays five small allocations.

   Recommended: **VLA** for simplicity. The codebase is C and
   has no aversion to C99 features. SBO is a fallback if profiling
   ever shows VLA stack pressure.

2. **Add a sanity ceiling.** Hard-cap n at 4096 (or similar large
   value). Keep the existing error path; just update the message
   to name the new ceiling and call out that it's a sanity guard,
   not a feature limit.

3. **Apply the same change in `do_write_box`** — the parallel
   arrays there have the same shape, the same cap, and need the
   same fix.

4. **Introduce `MAX_BRANCH_NAME`** at the top of `dispatch.c`
   (or in a shared header if it's referenced elsewhere). Replace
   the five `char branch[24]` / `char want[24]` declarations with
   `char branch[MAX_BRANCH_NAME]`. Value chosen so that
   `"out_4294967295\0"` (15 bytes) fits with margin (16 or 32).

5. **Test.** Either:
   - Add a unit test under `tests/319a-dispatch-vla-test.c` that
     constructs a box record with > 16 inputs and runs it through
     the dispatch invocation path, or
   - Add a test map under `tests/maps/319a-many-inputs/` with a
     box whose function accepts 20 inputs, exercised by the
     existing test runner.

   The first option is more isolated; the second is more
   end-to-end. Probably do both — the unit test catches the change,
   the map confirms nothing else along the path silently caps.

6. **Build the full test suite** and confirm no existing tests
   regress.

## Relevant files

- `src/012-dispatch.c` — the five-array allocation in
  `invoke_box_impl()` and `do_write_box()`; the five `branch[24]`
  sites.

## Not in scope for this sub-issue

- Slot store growth (319b).
- Box id generator / compile cache (319c).
- The `create_box` / `connect` runtime built-ins themselves
  (319d, 319e).
