# TEMPORARY — the dollar sign points one level up

> **This file is not a blueprint and does not belong to a phase.**
> It is a scratchpad for a design argument that is still in progress.
> Delete it when the argument is settled. Do not count it in any phase
> progress page, do not move it to `completed/`, and do not reference
> it from a document that is meant to last. The blueprints it touches
> — the ones about a door being a port, about results going where the
> caller says, and about one receipt for a box and a map — get
> rewritten as part of resolving this, and *those* are the permanent
> record.

## Where the argument started

The word **door** is being retired. It came from an older design in
which the mark sat on a whole station and the file said `entry` and
`result`. Now the mark sits on a port, so there is nothing left for a
second word to name. They are **input ports** and **output ports**, and
in a map file they are ports that name an outside source or an outside
destination.

A **mark** is the dollar sign. `in 0 - 0$` says *input port 0 of this
station is this map's argument 0*.

## Current behavior

**The mark lives on the port at runtime.** Each input port record
carries an integer saying which of the map's arguments it is, and each
output port record carries one saying which result it is; both hold a
"not a door" sentinel when they are neither, which is the usual case.

**Placing a map keeps its marks.** The stations that arrive from a
placed map carry their marks into the parent's table unchanged.

**Bring-up decides whose marks count by walking the receipt table.**
Before the program starts, a check asks of each marked station whether
it arrived as part of some placing; if it did, its marks are ignored,
so only the outermost map's marks are treated as argv slots. This is
the receipt-scoping rule.

**Dumping renumbers on collision.** Writing a map to a file writes
every marked port, placed ones included. Two copies of the same placed
map therefore both want to be argument 0, so the writer keeps the first
one's number and gives the second the next free one — the same rule
that turns the second copy of a station named `twice` into `twice~2`.

**Station names exist at runtime but nothing searches them.** The map
holds a parallel array of strings, one slot per station. A name goes in
when a map file is loaded and comes back out when a map is dumped. In
between there is no way to ask the map which station is called what.
The `~2` suffix that disambiguates a repeated name is invented at the
moment of writing, so the name in the file and the name in memory are
different strings.

**Referring to a station means holding its table index.** Every
composition and rewiring call takes the index. A live wire takes four
integers: source station, source port, destination station,
destination port.

## Why this is wrong

The mark should mean **one level up**, not **argv**.

The outermost map's marks reach the command line because nothing placed
that map. An inner map's `$0` should mean *whoever placed me delivers
here* — which for a map placed inside another map is the enclosing map,
not the shell. Reading a map file at load time and placing a map at run
time should mean the same thing, because a map standing where a box
stands is the whole point.

Receipt-scoping gets the right answer for the outermost map by
accident: it silences inner marks rather than redirecting them. The
inner mark should be *consumed* — turned into the parent's wire or into
the parent's own claim — not ignored.

Once it is consumed, three other things fall out:

- **Renumbering on collision disappears.** Nothing collides, because a
  placed map's marks never enter the parent's numbering. Renumbering
  reads like renaming a variable because you imported a library that
  used the name, which is not how C behaves and not how this should.
- **The receipt walk at bring-up disappears.** There is nothing to
  scope, because a placed station's ports are ports.
- **A parent that wants a placed port as its own interface claims it**
  — one explicit call naming a number the parent's author chose. Not
  inheritance, and not an extra identity box in between.

## The knot that is not untied

Everything lives in **one flat station table**. There are no levels at
run time, and that flatness is a deliberate property of the design, not
an accident to be worked around. But "one level up" is defined in terms
of levels.

The tentative reconciliation, which has not been checked against the
code:

> **The levels exist only during placement.** Reading a map builds its
> stations and notes their marks. Placing that group into a parent
> resolves each mark immediately — into a wire the parent draws, or
> into a claim the parent makes — and erases it. Afterwards there is
> one flat table with no levels in it, and the only marks left standing
> are the outermost map's, because nothing ever placed that one.

If that holds, the mark is a **load-and-place-time instruction**, not a
runtime property, and it survives into memory only for the map at the
top.

## Open questions

These are the reason this file exists. None of them are answered.

1. **What happens to an inner map's mark the parent neither wires nor
   claims?** Refuse the placement, leave the port unfed and let the
   station starve, or treat it as a deliberate free end that something
   will deliver to later?

2. **What does "the parent" mean for a map added to an already-running
   program?** There is no enclosing load to resolve against. Does the
   caller supply the resolution at the moment of placing?

3. **Does dumping a composed map reproduce the inner map's marks, or
   the flattened result?** Flattening loses the composition across a
   round trip. Dumping and reloading is stated to be the main use case,
   not inspection, so a lossy round trip is a real cost. There is no
   agreed answer and the previously-proposed fix — a fourth kind of
   line in the file recording which stations arrived as one placing —
   was rejected.

4. **If the outermost map's marks are the only ones left, how does a
   map know it is outermost?** Today the receipt table answers this.
   If placement erases marks, does anything still need to ask?

5. **If names become unique when a station is placed rather than when a
   map is dumped, what happens to a map file that names two stations
   the same thing?** Refusal at load, or the same suffix rule applied
   earlier?

## The other thread: naming a station at run time

This came out of walking the workflow for splicing a station into a
running map, which is the case that matters.

**The workflow today.** Ask the map for a slot, and it hands back an
integer, growing the table if it was full. Put a box in the slot,
saying how many input ports it has and how wide their values are.
Optionally give it a name, which nothing will ever read. Then draw the
wires.

**Only half of the last step is a problem.** The new station's number
was handed to you, so it is trustworthy. The *far end* of the wire is
not: to splice a station into a pipeline you need the number of a
station that is already there, and there are exactly three ways to have
it — you kept it in a variable when you created it, it came back in a
receipt from a placing, or you know the load order and counted. A
program that loaded a map from a file and now wants to splice something
into the middle of it has only the third, and counting positions in a
table is indistinguishable from writing a pointer by hand.

**Looking a station up by name is the fix**, and it is the only
candidate that is. The alternative that was considered first —
attaching a generation counter to each table slot, bumped whenever the
slot is reused, so that a reference held across a removal is refused
rather than silently followed — solves a different and later problem.
It makes a reference you already hold safe; it does not manufacture one.
Splicing rarely removes anything, so it is not the pressing case.

**What a lookup needs.** Names are not unique in memory today. A lookup
must either refuse an ambiguous name, return the first match and leave
the second unreachable, or make names unique at the moment a station is
placed. The third is preferred, because it makes the round trip exact:
the name you would type in code is the name written in the file, and
reloading gives back a map where the same lookup finds the same
station. See open question 5.

## What to work out before writing code

In order:

1. Settle where a mark is resolved — at placement, or at bring-up. Open
   question 1 decides the shape of the refusal.
2. Settle what a dump of a composed map contains. Open question 3 is
   the one with a stated use case behind it and no agreed answer.
3. Then the mark can move off the runtime port record, and the
   collision-renumbering and the receipt walk at bring-up can both come
   out.
4. Separately and independently, add the name lookup and move name
   uniquing to placement time.

## The word is out of the documents but not out of the code

The prose in every `docs/` page and in the engine's companion document
now says **input port**, **output port**, and **marked port**. The word
*door* survives in three places, all of them names rather than prose,
and renaming a name is a code change rather than a documentation one:

- A constant meaning "this port carries no mark", spelled with `DOOR`
  in it, in the public header.
- The call that answers *where is this part's nth argument or result*,
  which has `door` in its name and is part of the published surface.
- Two internal helpers in the engine: the one that picks a number for a
  mark when the author's choice is already taken, and the one that used
  to hold the station-level mark.

There are roughly seventy mentions across the engine's source comments,
the map parser, the map writer and the generator. Most are prose inside
comments and are safe to reword; the rest are identifiers, and one of
those is public.

Do this after the argument above is settled, not before — if placing
comes to spend a mark rather than keep it, some of these names describe
machinery that will not exist.

## Related documents

- The map file format document — describes the dollar sign and the
  three kinds of line.
- The guarantees page — anything that changes when a mark is resolved
  answers to it.
- The implementation note about a box and a map being one thing —
  records the designs that were considered and rejected on the way
  here.
