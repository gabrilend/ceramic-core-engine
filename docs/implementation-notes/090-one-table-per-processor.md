# 090 — One station table per processor

A wire is a pair of indices and an index means something only inside
one station table. That rule was chosen for a reason about *software*
— stations never move, so a wire can be a number rather than a
pointer, so buffers can grow and stations can be added without
anything dangling.

This note records the reason about *hardware* that it turns out to
have been all along, because the two agree and nobody arranged for
them to.

---

## The claim

**A station table belongs to one processor, and so does the thread
pool that runs it.**

Not to one machine and not to one core. To one physical processor —
the package, the socket, the thing with its own memory controller.
Every core inside it shares that processor's fast memory, and a
station table living there is reachable by all of them at the speed
the design assumes.

Two processors in one machine *can* share a table, if the memory
holding the stations is reachable from both. That is a slower path and
it is the exception. The ordinary arrangement is one table per
processor, and one pool per processor to run it.

---

## What the engine already does about it

**Nothing, and that is the point.** Every mechanism this needs is
already here, built for other reasons:

**A wire cannot leave a table.** Not by policy — there is no rule
about programs to enforce and nothing that would refuse you. An index
simply means something else in a different table, so a request that
looks like it crosses draws an ordinary wire at home instead. The
software invariant and the hardware boundary are the same boundary.

**A door can be crossed.** Delivering into a program's entrance from
outside it is the one way anything reaches a program it is not part
of. It takes a value and a place to put it and asks nothing about
where the caller is — the same call whether the caller is on the next
core or the next socket.

**A program can be started beside another, sharing the workers.** That
is the *within* a processor case: several programs, one pool, one
processor's cores. Its own table, its own everything, reached only
through its doors.

So a program spanning two processors is not one program with a long
wire. It is **two programs, one per processor, talking through their
doors** — which is what the engine makes easy and what it would have
made easy anyway.

---

## What follows for composing

Bringing a program inside another produces one station table
([217](../../issues/completed/217-a-program-inside-another.md)). Under this note
that is also a statement about placement: **things composed into one
table run on one processor.** Composing is therefore the right shape
for a subgraph you want close, and the wrong shape for work you want
spread across sockets — for which the answer is a second program with
a second table, fed through its entrance.

That gives the choice between composing and starting beside a second
axis it did not have. It was about *isolation*: a composed program
shares a fate, a started one does not. It is also about *locality*: a
composed program shares a processor's memory, a started one need not.

---

## What is not decided here

**Nothing pins a table to a processor yet.** There is no affinity
call, no allocation on a particular node, no placement of workers on
particular cores. The engine allocates with the ordinary allocator and
the operating system decides where everything lands.

This note is the intent, so that when any of that is built it is built
toward something rather than invented on the spot — and so that
nothing is added in the meantime that would make it impossible. The
thing that would make it impossible is a wire that crosses tables, and
there is a standing reason not to have one.

---

## Open question: what does "shared" mean when two processors share a table?

Two readings, and they ask for different engines.

**One process, several sockets.** Every core is in one address space
already; a station table allocated with the ordinary allocator is
reachable from all of them, and what differs between sockets is only
how *fast*. Nothing in the engine changes; what would eventually
change is where the memory is allocated from and which cores the
workers run on. This is the reading the note above is written under.

**Several processes, one machine.** Then "the area in shared memory
where the stations actually live" is a mapping — the project's own
`/dev/shm` tier, or something like it — and a station table would have
to be built inside it rather than on the heap. That reaches further
than it sounds: a station holds a mutex, which would have to be
created shareable between processes; it holds pointers to its port
array and its slot pages, which would all have to live in the mapping
too and be addressed as offsets rather than as addresses, because two
processes need not map it at the same place; and a box is a function
pointer, which means nothing at all in another process unless both
loaded the same binary at the same address.

The second reading is a real design and a large one. The first costs
nothing and is what everything here currently assumes. **Which one is
meant has not been asked yet**, and it should be before anything is
built toward either.
