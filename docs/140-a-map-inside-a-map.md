# 140 — A map inside a map

A box is a C function. A map is a description of stations wired
together. **Putting either one into a program is the same operation,
and hands back the same thing** — which is what makes a map able to
stand where a box stands.

This page is what that costs and what it buys.

## Placing anything hands back a receipt

```c
typedef struct map_instance {
    int *station;
    int  count;
} cera_map_instance_t;
```

**A receipt is the list of stations one placing created**, in the order
the description declared them. Placing a box creates one station and
the receipt names one; placing a map creates as many as the description
has and the receipt names all of them.

The caller cannot tell which kind it is holding, and does not need to.
That is the whole point: a caller composing two things asks for the
first thing's result and the second thing's argument and draws a wire,
and whether either one is a single C function or a fifty-station
pipeline never comes up.

**Reading a file into a fresh program is this with the program fixed at
"a new empty one"**, which is what it always was. What changed is that
the program may already have stations in it, so a description is a
**template being instantiated** rather than a program being merged. One
description can be instantiated as many times into one program as
anybody likes, with nothing shared between the copies.

## Nothing that already exists is renumbered

The invariant everything rests on — **an index means what it meant** —
is never approached.

What a description says and where its stations land are two different
numbers, related by a table rather than by an offset. Adding a station
hands back a *freed* place before it grows the table, so a program that
has had removals gets whatever holes exist, in whatever order. The
offset is what the translation degenerates to when nothing has been
removed, which is why it looks like an offset in every simple case and
is not one.

## There is one table, and no levels in it

After placing, a sub-map's stations are stations in the parent's table
with indices like any others. There is no seam to cross, no nesting to
walk, and nothing at delivery time that knows a value crossed a
boundary.

**This flatness is the design, not an implementation detail.** It is
what makes the delivery path the same path for every wire, and it is
why a map standing where a box stands costs nothing at run time.

## A part is an index

```c
const char *cera_map_add_part(cera_map_t *m, const char *what, int *part);
```

The map keeps a table of the receipts it has issued, and **a part is an
index into that table**. It is an index rather than a pointer because a
part travels on a wire when a map builds a map, and a wire carries
values rather than pointers.

Entries are never moved, so an index means what it meant. An ended part
is left **empty rather than removed**, for the same reason.

## Where a part's interface is

```c
int cera_map_part_door(cera_map_t *m, int part, int nth, int facing_in,
                       int *station, int *port);
```

Where a part's nth argument or result is, as a station and a port.

**A box's interface is its ports.** A part naming one station whose
ports carry no marks is a box: its argument N is input port N, and its
result N is output port N, because a box's ports are already numbered
and marking them would be writing down what counting already says.

**A map's interface is its marks** — the `$N` on ports in the map file
— because a map's ports are scattered across several stations and
nothing about their position says which argument is which.

Those are not two rules with a fallback between them. They are one
rule: *the interface is wherever the description put it*, and a
description of one station puts it on that station.

## Joining two parts

```c
const char *cera_map_join(cera_map_t *m, int from, int result,
                          int to, int argument);
```

A wire from one part's nth result to another part's nth argument, and
**it is the only wire a composing caller ever needs to draw.**

For two boxes it resolves to the ordinary wire, because a box's
interface is its ports. For two maps it resolves to a wire between two
stations that both happen to be inside the parent's table already.

## A placed map's marks are its own

They say *this port is a useful place to put values in or take them out
of this description*, and **nothing about the program that placed it.**

So placing one description twice does not give the parent two argument
zeros. And ignoring a placed map's result costs nothing — there is no
sink to wire up and nothing to remember to prune before attaching
something real later.

The engine knows which stations are which without being told: loading a
description makes no receipt and placing one does, so a station belongs
to a part or it does not.

**A parent that wants a placed port as part of its own interface claims
it**, explicitly, with a number the parent's author chose. It is a
claim rather than inheritance, and that distinction is what keeps two
copies of one description from fighting over argument zero.

## Ending a part is pruning its stations

```c
const char *cera_map_end_part(cera_map_t *m, int part);
```

**This is what the receipt was kept for.** Ending a program used to be
a separate idea; it turned out to be exactly "remove the stations this
receipt names", which the receipt already lists.

One sweep of the table cuts every wire naming any of them, **interior
wires included** — a wire from one member to another is named by a
member like any other, so nothing has to know it was interior.

Ending a part twice is refused rather than silently doing nothing.

## What is still unsettled

**What a placed map's marks should mean is under active argument.**
Today they are kept on the placed stations and ignored at bring-up by
checking whether a station arrived as part of a placing. The case being
made against that is that a mark should point exactly **one level up** —
to whoever placed this map — and therefore ought to be *consumed* by
placing rather than merely silenced.

Nothing on this page describes behaviour that would change if that
argument is settled the other way, except the last two sections, which
would gain a step.

## Related

- [008 — Map file format](008-map-file-format.md), where the `$` marks
  are written.
- [009 — Loading](009-datapath-load.md), what happens between a file
  and the first task.
- [135 — A box and a map are one thing](implementation-notes/135-a-box-and-a-map-are-one-thing.md),
  the five designs that were worked out and turned down on the way to
  one receipt.
