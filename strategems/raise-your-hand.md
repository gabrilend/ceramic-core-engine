# Raise your hand

**When one thread has to do something long and exclusive, and the
people who arrive during it have work that would fit inside it.**

## The pattern

A shared thing is full and has to be rebuilt bigger. The thread that
discovers this becomes the **builder** and publishes that a rebuild is
underway.

A second thread arrives wanting to add something. It does not block
until the rebuild finishes. It puts itself in a small ring of waiting
contributors — it raises its hand — and waits only until the builder
reaches a point where it can be interrupted. The builder then lets it
add its own entry directly into the half-built thing and update the
running counts, and carries on. The builder is finished when no hands
are up.

The manners are the specification. You show up, you raise your hand,
you say *excuse me, can you do this thing for me?*, they say *sure, put
it over there*, they update their count, and then they **trust you** —
they do not check your work, and they do not wait for you to finish
before continuing their own.

## What it costs and what it buys

It buys latency for the arrivals. Under a plain exclusive rebuild,
everyone who shows up pays the full length of the rebuild; here they
pay only the distance to the builder's next interruptible point, and
their contribution lands in the new thing rather than having to be
redone against it.

It costs a queue of waiting parties, a notion of "a point where I can
be interrupted," and the discipline that the builder does not validate
what a contributor did. That last one is not laziness — validating
would serialise exactly the thing the pattern exists to overlap.

**The bound worth checking:** if contributors arrive faster than the
builder can admit them, the builder never finishes. It is a livelock
with good manners. Whether that can happen depends on how long the
builder's uninterruptible stretches are and how many threads can
plausibly arrive.

## Where it does not apply, and why that matters

It has no customer in minimal-soramech, and the reason is worth
recording because it is the same reason it will have no customer in
most places: **there is usually a way to make the long operation
short.**

The station table and the task queue both looked like candidates —
both grow, both were going to grow by allocating something larger and
copying everything across, and both would have made everyone wait for
that copy. Growing by *adding a page* instead of *replacing the whole*
makes the operation a single pointer write. Nothing is copied, so there
is no long stretch, so there are no hands to raise.

So the first question to ask when reaching for this pattern is whether
the exclusive operation can be made short instead. If it can, do that;
it is faster and there is less of it. This pattern is for the cases
where the work is genuinely irreducible — a rebuild that must touch
every element, a migration that must transform as it copies, a
compaction that must decide what to keep.

## Related

- The paging shape that displaced it here: grow by adding a fixed-size
  block and indexing in two steps, so nothing already placed ever
  moves. Used for the station table, and applicable to any table
  addressed by a stable integer.
- `issues/211-growing-the-station-table.md`, where both were weighed.
