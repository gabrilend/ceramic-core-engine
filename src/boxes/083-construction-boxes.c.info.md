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

| Function | Takes | Gives | Does |
|---|---|---|---|
| `program_add_station` | a program, a box name | the station's index, or -1 | Adds a station running that box. An index is what every wire is made of, so this is the value the others take. |
| `program_wire` | a program, two stations and two ports | 1 drawn, 0 refused | Draws a wire, applying every rule the engine applies anywhere else. |
| `program_set_constant` | a program, a station, a port, text | 1 taken, 0 refused | Gives a port a constant. Text rather than bytes, because bytes are exact only against the build that wrote them while text resolves its layout when it is read. |
| `program_name_station` | a program, a station, a name | 1 taken, 0 refused | Names a station, so the program can be written out as a file that reads back. |
| `program_set_door` | a program, a station, which way it faces (1 in, 2 out) | 1 marked, 0 refused | Marks a station as a door. One operation for both directions, because the two are one design seen from either side. |

**The door operation is what turns a graph into a program**, and it
had to exist the moment a program was required to say where its
results come from (issue 209). The four operations above it can
assemble any shape; none of them could produce something that would
*run*, because the thing that turns reachable internals into a surface
is the mark. A map that could build only graphs could build nothing.

It takes a number rather than a word for the direction, because a wire
carries values: a box taking a word would need the word to have come
from somewhere, and the somewhere would be a constant nobody reading
the map can see.

A box returns one value, so a refusal's reason cannot come back beside
the answer — it is printed, and a map that wants to react reacts to
the zero.

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
