# Grow by adding, never by replacing

**When a collection has to get bigger while other threads are using it.**

## The pattern

The obvious way to grow something is to allocate a larger one, copy
everything across, and switch to it. That move has three costs and they
are all worse than they look:

- **Everything moves.** Anything that held an address into the old
  storage is now holding a dangling pointer, and anything that *is* an
  address — a lock, in particular — has quietly changed identity.
- **Somebody has to stand still.** The copy is long and nobody may
  touch the collection during it, so every other thread waits for the
  whole thing.
- **The window has to be got exactly right.** Copy then publish, and a
  value taken out of the old storage during the copy exists in both
  places. Publish then copy, and readers see an empty collection while
  it fills. Neither order is safe, because the real requirement is that
  *nothing else happens at all* during the copy.

Instead: keep the old storage exactly where it is and **add another
block beside it.** The collection becomes a short list of blocks. Item
*n* lives at position *n within a block*, in block *n divided by the
block size* — one shift and one mask when the size is a power of two.

Growing is now one allocation and one pointer write. Nothing is copied,
nothing moves, no address changes identity, and the only thing another
thread can observe is that there is now more room than there was.

## What it costs

One extra memory hop to reach an item, and some unused room at the end
of the last block. Both are small and neither scales with how often you
grow.

The block size barely matters, which is worth knowing because it looks
like it should. Small blocks mean the list of blocks grows more often —
and that list holds *addresses*, so growing it by copying is safe and
fast, since moving a pointer does not move what it points at. Large
blocks mean a small collection pays for room it never uses. Pick a
number, name it once, move on.

## Where it applies, and the one condition

Anywhere addressed by a stable integer index. In minimal-soramech that
turned out to be almost everything: the station table, the pool's task
queue, the per-station locks, the registry once boxes can be compiled
at runtime, and eventually a port's ring buffer cells.

**The condition is that the index arithmetic must not depend on the
total size.** A ring buffer looked like an exception for exactly this
reason — its read and write positions are taken modulo the capacity, so
changing the capacity moves where every existing value lives, and the
copy is doing real work rather than just relocating. It stopped being
an exception the moment its cells started carrying their own state and
readers scanned for a usable one instead of computing where it must be.
The moment nothing computes a position from the size, nothing cares
that the size changed.

## The rule underneath it

**Fixed-size things live in blocks and never move. Variable-size things
live at the end of a pointer.**

A record whose size is the same for every instance can sit in a block
and be found by index forever. A thing whose size depends on the
workload — a buffer, a list, an array of destinations — is reached
through a pointer stored in that record, so it is free to move without
disturbing anything that refers to the record.

And once the variable-size thing can itself be blocked, it stops moving
too, and there is nothing left to reclaim.

## Related

- `raise-your-hand.md`, the pattern this displaced: before building
  machinery to let people contribute during a long exclusive operation,
  ask whether the operation can stop being long.
- `issues/211-growing-the-station-table.md`, where the mutex-cannot-move
  argument forced the question in the first place.
