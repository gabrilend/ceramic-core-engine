# 052-rewire.c — changing the shape while it runs

Drawing and cutting wires, and taking a station out, on a program that
may have workers in flight. The feature the whole design was quietly
preparing for: wires hold station numbers rather than addresses,
stations never move, and a delivery walk reads a destination list
without a lock — each of those was chosen partly for this, and this
file is the debt being redeemed.

## The one lock, and what it is for

**Checking an edge and installing it are one operation.** Two threads
each adding an individually legal wire can produce an illegal pair, so
the check and the insertion are never separated — one rewiring lock
holds both. List surgery additionally happens under the owning
station's own mutex, so nobody is left holding something freed.

## The functions

| Function | Takes | Gives | Does |
|---|---|---|---|
| `map_wire` | a program, a station and output port, a station and input port | NULL, or a sentence | **The one wiring operation.** Draws a wire at any moment — while a program is being assembled or while it runs — applying every rule. |
| `map_rewire_connect` | the same | 0, or -1 | The same operation, printing the refusal and returning a code. |
| `map_rewire_disconnect` | the same | 0, or -1 | Cuts one wire, by rebuilding the destination list without it. |
| `map_remove_station` | a program, a station | 0, or -1 | Takes a station out and frees its place for reuse. |

**And having one of them is what found a hole in it.** The width check
sat behind a condition asking whether *both* stations had input port
arrays. The destination always does, or the port index would have been
refused already; the source frequently does not, because a box that
takes no arguments and returns a value is how most programs begin.
Every such station could be wired into a port of any width at all with
nothing said. It stayed invisible while the loader carried a width
check of its own — every program read from a file met that one first —
and it opened the moment a program was built by calling the surface.
Taking the loader's copy away is what made it show, which is the
argument in miniature: the second check was not redundancy, it was
concealment. There is a test for it now with the other width scenes.

**There is one implementation and three faces on it**, differing only
in what a caller wants done with a refusal: hand it back, print it and
return a code, or stop the program (which is what the construction
call in the station layer does). Construction and live editing used to
have an implementation each, and construction's was quietly the
weaker — it never asked whether the destination could hold a value and
never compared the widths — so a program built by hand could contain a
wire the same program read from a file would have been refused. Which
face you want is a property of the caller, and it is the only thing
that ever legitimately varied.

## What removal does, and why in that order

**The wires that name a station are cut before the station goes.**
A wire lives only as a destination record on some other station's
output port, so one walk over every station finds all of them. Doing
it first is what makes reusing the place safe *without* putting a
version number on every wire — the walk is paid once, rarely, instead
of on every delivery.

Each affected list is rebuilt whole and published by one write, so a
walker sees the before or the after and never a partial cut. The
station is marked removed first, so a value already in flight toward
it is discarded on arrival rather than delivered into a station being
dismantled — which is what this engine already does with a value that
has nowhere to go.

**The place does not come free immediately.** A task is built from a
station's fields after the readiness check released its mutex, so a
worker may still be inside one. A quiescence sweep decides when nobody
can be, using a per-worker counter that is odd inside a task and even
outside it.

## Refusal, decided rather than defaulted

A loader that dies serves its author. A running engine that dies
because a control surface sent one bad instruction takes the plant
down with it. So the operations here hand a refusal back rather than
stopping, and the caller decides — which is also what lets somebody
reading a file collect every mistake in it and present them together
instead of one per run.

## What is gone

A gather cycle check that ran when a connection was made rather than
when it was traversed, and an operation that repointed a gather wire
while the program ran. Nothing is pulled any more, so what remains is
connecting, cutting, and removing.
