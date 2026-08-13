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
during the copy, because the mutex was held for the whole of it — and
after [210d](210d-the-claim-takes-no-lock.md) there is no such mutex
on that path any more. So this is not merely an improvement; **the
existing growth stops being correct** once the claim goes lockless,
and the two cannot be separated by very long.

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
   as [210b](210b-the-port-record.md) already does, and a link for
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

- Every page after the first could be the same size as the first, or
  each could be larger than the last. Equal pages make the scan's page
  arithmetic trivial and make a very deep backlog into a very long
  list; growing pages keep the list short and make the arithmetic a
  search. The old buffer doubled, so a deep backlog cost few
  allocations, and that property is worth not losing by accident.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210d — The claim takes no lock](210d-the-claim-takes-no-lock.md),
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
