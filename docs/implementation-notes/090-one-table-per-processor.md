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

**A station table belongs to one processor, and so does the thread pool
that runs it.**

Not to one machine and not to one core. To one physical processor — the
package, the socket, the thing with its own memory controller. Every core
inside it shares that processor's fast memory, and a station table living
there is reachable by all of them at the speed the design assumes.

Two processors in one machine *can* share a table, if the memory holding
the stations is reachable from both. That is the slower path and the
exception.

## What the engine already does about it

**Nothing, and that is the point.** Every mechanism this needs is already
here, built for other reasons.

**A wire cannot leave a table.** Not by policy — there is no rule to
enforce and nothing that would refuse you. An index simply means
something else in a different table, so a request that looks like it
crosses draws an ordinary wire at home instead. The software invariant
and the hardware boundary are the same boundary.

**A marked port can be crossed.** Delivering into a program's argument
port from outside is the one way anything reaches a program it is not
part of, and it asks nothing about where the caller is — the same call
whether that is the next core or the next socket.

**Two programs in one process are two programs**: several tables, each
its own everything, reached only through their marked ports. One program
per pool, too.

So a program spanning two processors is not one program with a long wire.
It is **two programs, one per processor, talking through their marked
ports.**

## What follows for composing

Bringing a program inside another produces one station table, so under
this note that is also a statement about placement: **things composed
into one table run on one processor.** Composing is the right shape for a
subgraph you want close and the wrong shape for work you want spread
across sockets — for which the answer is a second program with a second
table, fed through its argument ports.

That gives the choice between composing and starting beside a second
axis. It was about *isolation*: a composed program shares a fate. It is
also about *locality*: a composed program shares a processor's memory.

## What is not decided here

**Nothing pins a table to a processor yet.** There is no affinity call,
no allocation on a particular node, no placement of workers on particular
cores; the engine allocates with the ordinary allocator and the operating
system decides where everything lands.

This note is the intent, so that when any of it is built it is built
toward something — and so nothing is added meanwhile that would make it
impossible. The thing that would is a wire crossing tables, and there is
a standing reason not to have one.
[108](../../issues/108-choosing-where-a-box-runs.md) takes this as its
starting point and answers the question below.

---

## Answered: "shared" means one process, several threads

Two readings were possible, and they asked for different engines.

**One process, several sockets — this is the one meant.** Every core is
in one address space already; a station table allocated with the ordinary
allocator is reachable from all of them, and what differs between sockets
is only how *fast*. Mutexes are ordinary mutexes, pointers are pointers,
and a box is a function in this binary. Nothing in the engine changes
shape; what changes is where memory is allocated from and which cores the
workers run on.

**Several processes, one machine — not this.** Then the stations would
have to live inside a mapping rather than on the heap, which reaches
further than it sounds: a station holds a mutex, which would have to be
created shareable between processes; it holds pointers to its port array
and its slot pages, which would all have to live in the mapping and be
addressed as offsets, because two processes need not map it at the same
place; and a box is a function pointer, which means nothing in another
process unless both loaded the same binary at the same address.

**"No main thread" belongs to the first reading too.** It means there is
no privileged owner among the workers — nobody creates the pool, decides
its shape, and hands placements to the others. The consequence lands in
[108](../../issues/108-choosing-where-a-box-runs.md): a worker announces
where it lives rather than being told, and the pool's reach is the union
of what its members announced. A central placement policy would have
needed a centre, and there is not one.
