# 210e — Growth adds a page

Fifth child of [210](210-input-port-record.md), and the one that gets
easy rather than being made easy. Two children earlier removed the
premise that made growing a buffer hard; this one collects.

## Current behavior

A ring buffer grows by allocating a larger array, copying the values
across, unwrapping them so the oldest sits at index zero, and freeing
the old storage — all under the station's mutex.

**It has an ordering problem with no correct answer.** Copy the values
across and then publish, and a value popped from the old storage
during the copy exists in both places and gets delivered twice.
Publish first and then copy, and readers see an empty buffer while it
fills. Neither order is safe.

What actually made it safe was that nothing else could happen at all
during the copy, because the station's mutex was held for the whole of
it — and after
[210d](210d-the-copies-leave-the-lock.md) that is no longer true. The
lock still exists, but it covers only the cell states: a worker copying
bytes out of a cell it has claimed holds **no lock at all**, because a
claimed cell belongs to it alone and needs no exclusion from anybody.
Relocating that cell underneath it is exactly the thing that
protection does not cover. So this is not merely an improvement;
**the existing growth stops being correct** once the copies leave the
lock, and the two cannot be separated by very long.

## Intended behavior

**A ring buffer grows by allocating another page of cells and adding
it to a short list.** The same shape the station table uses, for the
same reason. Nothing is copied, no existing cell moves, and there is
no window to get right.

Because nothing computes a location from the capacity, changing the
capacity disturbs nothing. That sentence is the whole issue. A reader
scanning from a hint does not care how many cells exist or where they
live; it walks what is there. Adding cells is therefore a matter of
making them findable, not of rearranging anything.

**With nothing copied, there is nothing to protect.** The real
requirement was never an ordering between copy and publish — it was
that nothing else happen during the copy. Removing the copy removes
the requirement rather than satisfying it.

**Growth stays under the station's mutex**, as one of the four rare
structural operations, because two threads noticing a full buffer at
the same moment should add one page rather than two. That is
contention on a rare event, which is what a mutex is good at.

**The growth story stays readable.** How many times a port has grown
and the deepest its backlog reached are what phase 7's buffer report
speaks, and they are how a person learns that one input side is
outpacing its siblings. Paging changes how growth happens, not that it
is worth reporting — but "capacity" becomes a sum across pages rather
than a single number, and the report should say the sum.

## Suggested implementation steps

1. The page list on the port: a first page allocated at instantiation
   as [210b](completed/210b-the-port-record.md) already does, and a link for
   the rest.
2. The scan walks pages rather than one array. This is where the cost
   of paging lands and it should be looked at honestly: a scan that
   crosses a page boundary follows a pointer, where before it
   incremented an index.
3. Growth appends a page under the station's mutex, checking after the
   lock is taken that another thread has not already grown it.
4. Retire the copy-and-unwrap path.
5. The buffer report's capacity becomes the sum across pages, and its
   growth count keeps meaning how many pages exist beyond the first.
6. A test that grows a port deep while readers and writers are both
   working it, asserting no value is lost and none is delivered twice
   — which is exactly the failure the old ordering problem produced.

## Open questions

**Answered:**

- *Every page after the first could be the same size as the first, or
  each could be larger than the last.* **Every page is the same size.**
  Turning a cell's ordinal position into a page and an offset is then a
  divide and a remainder — arithmetic the scan does on every step,
  where growing pages would have made it a walk down the list
  comparing ranges. The scan is the hot path here and the growth is
  not, so the cheap operation belongs on the side that repeats.

  The old buffer doubled, and losing that is the real cost: a backlog
  ten thousand deep is a thousand pages rather than ten, and a scan
  crossing all of it follows a thousand pointers. That is accepted
  with two things in mind. The first is that a port that deep is
  already a report phase 7 shouts about — one input side outpacing its
  siblings — so the long list is a symptom of a problem the engine is
  supposed to be complaining about rather than quietly absorbing. The
  second is that the page size is the same named constant the first
  page uses, so a program that genuinely wants deep buffers raises the
  starting depth and gets large pages everywhere, which is the same
  lever pointed at the same problem.

  If a measurement ever shows the walk mattering, the move that keeps
  this decision is a page-lookup array beside the list — an index into
  pages rather than a chain through them — which restores constant-time
  addressing without making the pages unequal.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210d — The copies leave the lock](210d-the-copies-leave-the-lock.md),
  which must land first, and which this must follow closely because it
  is what makes the existing growth unsafe
- [203 — Slot buffer growth](completed/203-slot-buffer-growth.md), the
  copy-and-unwrap this retires, and whose premise the parent removes
  rather than fixes
- [211 — Growing the station table](211-growing-the-station-table.md),
  the same paging shape one level up, and the place its reasoning is
  written out in full
- [701 — Buffer growth reporting](completed/701-buffer-growth-reporting.md),
  which speaks the numbers this changes the meaning of
