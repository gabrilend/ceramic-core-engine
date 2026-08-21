# 033-statics.c — constants on ports, from outside

A value that sits on one input port and is simply always there:
thresholds, paths, configuration. Always full, never consumed, never
part of readiness — so a station driven by its buffered side reads the
same constant on every one of its runs.

**A constant belongs to the port that reads it.** There was a numbered
table on the map, shared by every port that named an entry, and it is
gone. What it cost was out of proportion to what it bought: it was
map-level mutable state, so a process could hold only one running
program; its mutex was a second lock a claim had to take, nested inside
the station's; and an entry's bytes were shaped by whichever port bound
it first, so two ports of different types could read the same bytes
each their own way.

Two ports written from one entry in a file are now independent from the
moment they are written. Sharing, when it is wanted, is drawn — one
station holds the value and everyone who needs it has an arrow from it,
which costs a station and gains a wire somebody can see.

## Functions

**map_in_port_static_text(map, station, port, text)** — give a port a
constant written as text, and make it a static. Parses into the port's
own storage, shaped by the port's registry type: a number for
int/unsigned/float ports, a brace walk over the generated field table
for a struct port, the characters themselves for a `const char *` port
(claimed as a pointer to storage the port owns). Fatal, naming station,
port and field, on any mismatch: too many values, too few, a string
where a number belongs, an untyped port.

Parsed into scratch first and installed under the station's mutex, so a
malformed value never half-overwrites a working one and no concurrent
claim sees a value mid-parse. Runs the readiness check afterwards.

**map_deliver_argument_text(map, station, port, text)** — an argument
written as text, turned into the bytes that port wants and delivered
through the ordinary door. The constant reader pointed somewhere else:
somebody typing `{ 5, 2.0, "hey" }` on a command line and somebody
writing it in a map file are doing the same thing, so struct arguments
in brace syntax needed nothing built.

A string argument's characters are **never freed**, deliberately. The
value delivered for a string port *is* a pointer, and what it points
at has to outlive every box that might read it — which is the whole
run.

**map_deliver_command_line(map, argc, argv)** — the whole of it. A
program's arguments are the input ports of the stations it declared as
entrances, in station order and then port order. A count that does not
match is refused rather than half delivered, and a program that has
already finished is refused rather than handed arguments nobody will
run — see the standing-promise rule in `011-pool.h.info.md`.

**map_in_port_static_write(map, station, port, bytes, size)** — change a
constant mid-run. Size-checked against what the port holds, and the
station's own mutex — the lock the claim already takes — is held for
the length of the copy, so no invocation sees fields from two worlds.
Runs the readiness check afterwards.

Refuses a port that has never held a constant, because its shape is
unknown.

**in_port_constant_text(port, out, room) → wanted** — a constant turned
back into the text a map file would use, the exact mirror of the
reader, walking the same field table the other way. Writes at most
`room` bytes including the terminator and returns how many characters
it wanted, so a caller can ask again with a bigger buffer. Floats get
enough significant digits to read back as the same value.

Nothing needed this until the table went: the table kept the original
string a file gave it and the dump echoed that string, which had a hole
in it — a value changed while the program ran was not re-serialized, so
the dump printed what the file said rather than what the engine held.

**in_port_constant_free(port)** — internal: the constant and, for a
string, the characters it points at.

## What a box may no longer do

**Write a static.** There used to be a bare-name call taking an entry
number, reachable from inside a box. It was always described as
deserving a global variable's suspicion — a box could stash a value and
read it back next run, invisible in the wiring — but the cost that
settled it was structural: reaching a map from inside a box needs a
process-wide map pointer, and that pointer is what limited a process to
one running map. A box that needs to affect something later returns a
value, and the value is wired somewhere.
