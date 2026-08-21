# 083-construction-boxes.c — building a program from inside one

The construction surface as ordinary box functions, so that assembling
a program is something a **map** can do and not only something C can
do (issue 212). A station running one of these adds a station to
another program; another draws a wire in it. Nothing was added to the
engine to permit this — these are box sources like any other, and the
engine has no idea they are special.

## How a box says which program

**By its address, carried as a number.** A box takes its arguments by
value and may not reach anything ambient; the last global pointer to a
map was deleted on purpose, so that two programs could run in one
process without seeing each other. So a box acting on a program has to
be handed one, and where it lives is the only way to say *that
particular program* as a value.

The address travels inside a one-field struct with its own name, which
buys nothing from the engine — a wire is checked by width — and
everything from the person reading a map. A port typed `program` is
one somebody had to mean.

## The functions

Everything here speaks in **parts**. A part is where values go in and
where they come out — for a map brought inside this one, the stations
it declared as doors; for a single box, the same station, because a
box's own input ports are its way in and its own output port is its
way out.

| Function | Takes | Gives | Does |
|---|---|---|---|
| `program_add` | a program, a name | a part | **Adds a box, or adds a map. One operation.** A map is a list of boxes and the wiring between them; a box is a list of one. |
| `program_connect` | a program, two parts and two port numbers | 1 | A wire from one part's way out to another's way in. For two boxes this is the ordinary wire; for two maps it crosses what used to be a seam and finds nothing there. |
| `program_set_constant` | a program, a part, a port, text | 1 | Gives a port a constant. Text rather than bytes, because bytes are exact only against the build that wrote them while text resolves its layout when it is read. |
| `program_set_door` | a program, a part, which way it faces | 1 | Marks a part as one of this program's own doors. |
| `program_name_station` | a program, a part, a name | 1 | Names a part's way in, so the program can be written out as a file that reads back. |

**A part is never taken apart**, and that is the design rather than an
accident. Everything above that consumes one takes it whole, so
nothing exists here whose only job is to pull a field out of a
handle — which is the thing this project calls *a function written to
fit the engine* and refuses to make anybody write. A part travels on a
wire exactly the way a program handle does.

**Which kind a name refers to is resolved, not guessed.** A box lives
in the binary and a description lives on disk; both are looked for.
Finding both ends the program as ambiguous rather than settling it by
an order nobody can see, and finding neither ends it naming both
places that were searched.

A refusal ends the program (issue 106). These used to print a reason
and return zero, and a box's caller is a **wire**, which ignores
everything it is not attached to — so a map that never wired the zero
anywhere would carry on believing it had edited a program it had not.

## The risk this file makes real

**A wire is legal when both ends count the same bytes**, which is
right for data: two boxes may spell one shape differently and mean the
same thing. An address is eight bytes on the machines this runs on,
and so is a `double`, a `long`, and a file offset.

So the engine will accept a wire feeding any eight-byte value into the
program argument of anything here, and what follows is not a wrong
answer — it is a write through whatever those bytes were.

That was decided rather than discovered: issue 309 recorded the hazard
when it chose width comparison and said the decision would fall due
the moment a program could build a program. It fell due here.
[058](../../docs/058-guarantees.md) states the residue in its own
words.

**What is checked is a null**, before anything is touched, because a
port never given a value delivers zero and that is the likeliest
mistake by a wide margin. Past null nothing distinguishes a real
address from any other eight bytes, and nothing here pretends to.
