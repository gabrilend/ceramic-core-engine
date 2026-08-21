# 213 — The input station

The other door. [209](completed/209-map-output-collection.md) names where a
program's results come from; this names where its arguments arrive.
They are one design and it is worth reading them together.

## Current behavior

**A program has a surface.** A station can be declared as the entrance
— the ports the outside is allowed to deliver to — and delivering from
outside refuses any station that is not one.

**That refusal is the whole of it.** Underneath, delivering an
argument is the ordinary delivery down an ordinary wire; nothing was
added to the delivery path. What changed is *who may use it*. A caller
reaching an interior station is reaching inside, and the point of the
declaration is that reaching inside stops being possible by accident.
A surface nobody has to respect is not a surface.

**One mark, two directions**, shared with
[209](completed/209-map-output-collection.md), because the two are one design
seen from either side. A station is neither door, or one of them.
Being both is refused: a program whose entrance is its exit is
somebody having named the wrong station.

**The size is checked here and only here.** Inside the graph a wire
was checked when it was drawn; from outside there is no wire, so this
is the only moment anything can be.

**A declared entrance stopped triggering a false alarm.** The
whole-program pass warns about a station whose buffered inputs no
arrow feeds, and that warning has always ended *"unless something
outside delivers into it"*. A declared entrance is precisely such a
station, so the sentence's own escape clause became checkable — and
warning about it would be telling somebody that the thing they just
declared might not happen.

**What the test would not assert.** Results are held oldest-first, so
they come out in the order they *arrived* at the output station —
which is not the order the arguments went in. Three values fed to a
program with two workers are in flight at once, and which finishes
first is a schedule. The test checks the set and says why, because
asserting the sequence would be asserting the scheduler.

**The map file spells both**, and both survive a round trip.

**Two refusals gained the same escape clause, and neither was
foreseen.** The whole-program pass warns about a station whose
buffered inputs no arrow feeds, and reading a file refuses a program
in which nothing can start. Both were written when there was no way
for a program to *say* it expected to be fed, so both assumed the
worst — and a declared entrance is exactly the case they were assuming
away. Warning about one, or refusing to run it, would be telling
somebody that the thing they had just declared might not happen.

Still to come: deriving a port's type from what it feeds, the
command-line path, and one program used as a box inside another.

### What stood before

A program has no declared entry.

Values get into a running program in exactly one way that anybody
outside it can arrange: somebody with a pointer to the map calls the
delivery entry point naming a station and a port. That works, and it is
what the seed and the tests both use, but it means **the caller has to
know the program's insides.** There is no port that belongs to the
program rather than to one of its stations.

Three consequences:

**A program cannot be composed.** A parent wanting to feed a sub-program
has to reach in and name a station by whatever the sub-program's author
called it. Rename that station and every parent breaks. That is not
encapsulation; the sub-program has no surface, only internals that
happen to be reachable.

**A program cannot take arguments.** Nothing corresponds to a
parameter list. A program run from a shell gets whatever its statics
say and nothing else, so "run this with a different input file" means
editing the box file rather than passing something.

**A leaf box and a composite box do not look alike.** A C function
announces what it takes and what it returns in its signature. A program
announces neither. So the claim that a program can be used as a box is
aspirational — there is nothing on the outside of a program for a
parent to wire to.

## Intended behavior

**A program's input station is an ordinary station, designated — and
its input ports are the program's input ports.**

Same shape as anything else: input ports, one output port, and a box
that may or may not be there. What makes it an input is the
designation, which says that **these are the ports the outside world is
allowed to deliver to.** Everything after that is an ordinary delivery
travelling an ordinary wire.

**A box here is optional.** With none, the station has one input and
one output and the value crosses unchanged. With one, arguments can be
checked, combined, or reshaped on the way in — using a function
somebody would have written anyway rather than one written to satisfy
the engine.

**One output port, so one station per argument group.** A box returns
one value; a station therefore has one output port; so a program that
takes several unrelated arguments has several input stations rather
than one station with several outputs. The alternative needs a C
function returning several values, which does not exist — and faking it
with a struct that something downstream takes apart means **a special
function written to fit the engine**, which is exactly what a person
adopting this should never have to do.

Fan-out is not the same thing and is already free: one input station's
single output port may feed as many interior stations as you wire it
to, because every output port has always had a list of destinations.
One argument reaching six places needs no mechanism at all.

### Values that must stay together must travel together

**Two values delivered to two ports of the same station are not a
pair**, and this is the trap the door opens onto. A station with an
object port and a colour port, fed by two callers who each want their
own object painted their own colour, will pair them **arbitrarily** —
the first caller's object with the second caller's colour is a
perfectly legal outcome.

The reason is already written down and is deliberate:
[210d](completed/210d-the-copies-leave-the-lock.md) states that values may leave a port in
a different order than they arrived, because with positions gone a
reader takes the first ready slot its scan finds, and rollback opens
gaps wherever it happens. If order within one port is not promised,
correspondence across two ports certainly is not.

**So anything that must arrive as a unit is one struct on one port.**
The engine already supports this all the way down: the generator emits
a field table per struct, the reader turns brace text into correctly
laid-out bytes using compiler-computed offsets, and command-line
arguments in brace syntax come along free. The object and its colour
are one value, indivisible by anything the scheduler does.

The alternative — one input station per caller — also works and needs
no argument, but it scales with callers rather than with the interface,
which is the wrong axis.

**This is not a defect and is already written down.**
[058](../docs/058-guarantees.md) states it twice over: a value carries
no relationship to any other value, and a station pairs whatever is at
the head of each of its ports. What the door adds is only a new place
for somebody to walk into it, since a caller who has just written two
arguments feels much more like they sent *a pair* than a graph author
splitting one value down two paths does.

The reason it must be this way is the bargain that page now opens with:
pairing across ports would mean a ready value waiting for its partner,
and a waiting value is a worker not running one of the ten things that
are ready. **Values here stand on their own, interchangeable with any
other of their type at the same port** — that is what lets whichever
worker is free take whatever is ready, and it is what the engine is
spending order and pairing to buy.

**Where "outside" is depends on who is running it, and the mechanism is
the same either way.** For a sub-program, the outside is the parent map,
delivering typed values down an ordinary wire. For a program started
from a shell, the outside is the runner, delivering the command line.
For a program embedded in a C application, the outside is that
application, handing over typed values directly.

**Command-line arguments cost nothing to build.** They arrive as text,
and the engine already has a text-to-bytes reader — the one that turns
a static's `{ 5, 2.0, { 0, 0, 0 }, "hey there", 2 }` into correctly
laid-out bytes by walking the field table the generator emitted. Point
it at the argument list instead of at a statics line and it is the same
code, the same compiler-computed offsets, and the same error messages
naming the field that was wrong. Struct arguments on the command line
come along for free, in the brace syntax that already exists.

**The input ports need no declared types.** If input port zero feeds a
station whose box takes an `int` there, then input port zero is an
`int`, resolved from the registry and checked by the ordinary wire
check. A port fanning out to two stations that expect different types
is caught the same way. Nothing new goes in the file, which keeps the
format's standing rule that it carries no types anywhere.

**A program with no arguments has an input station with no ports**, and
that is not a special case needing special handling. It has nothing to
receive, so nothing arrives, and the program starts the way any program
starts: the statics bound during construction are writes, and a write
runs the readiness check on the station holding it
([004](../docs/004-datapath-statics.md)). The door is for arguments;
starting is a separate thing that happens everywhere.

**And a leaf box and a composite box finally look alike from outside.**
One announces its interface in a C signature, the other in two
stations. A parent wiring to either cannot tell, and has no reason to
want to.

## Suggested implementation steps

1. Box file syntax for declaring which station is the input, alongside
   the output declaration, and reader support for both.
2. **Done.** One mark on an ordinary station saying it is a door and
   which way it faces, shared with
   [209](completed/209-map-output-collection.md).

   The station that runs no box was **not** built and turned out not
   to be needed: a door that shapes nothing is a station running an
   identity box, which is an ordinary function somebody was going to
   write anyway rather than a new kind of station. That keeps the
   count of things this engine has at what it was.
3. Deriving each input port's type from what it feeds, and refusing a
   port whose destinations disagree.
4. **Done.** A call naming a program, a station, a port and a value,
   refusing any station that is not a declared entrance and checking
   the size — which is the only moment it can be checked, there being
   no wire to have checked it when it was drawn.
5. The command-line path, reusing the statics text reader, with a test
   passing a struct argument in brace syntax.
6. A test that one program is used as a box inside another — wired to
   its input ports, read from its output ports, with the parent never
   naming anything inside it.

## Open questions

**Answered:**

- *Several input stations, or one with several ports?* **Several
  stations, each with one output port** — and the reason is not the one
  the output side recorded. Readiness is a property of input ports, and
  an input station's ports on the graph side are output ports, which
  the engine has always allowed many of; a comparator has three and an
  iterator has as many as you wire. So the subset-readiness argument
  does not reach this side at all.

  What decides it is C. A box returns one value, so a station has one
  output port. Several outputs would mean a box returning several
  values, and faking that with a struct built by one box and taken
  apart by another is **a function written to fit the engine** — the
  one cost this design refuses to impose. Somebody bringing existing C
  to this should be able to bring it unchanged.

  Written up above, along with the reminder that fan-out already covers
  the case people usually mean: one argument reaching many interior
  stations is one output port with many destinations, which has always
  worked.

- *Does an input port accept several arrows, like any other port?*
  Yes, and nothing about it is special: several callers delivering to
  one program input is several arrows into one port, which every port
  in the engine has always allowed. They interleave, exactly as two
  upstream stations feeding one port already interleave.

  **The engine does not remember who called, and does not need to.**
  Where a result should go travels *in the values*, not in the identity
  of the sender. A shared factory taking an object and a colour returns
  the blue one down the blue wire because a routing station read the
  colour, not because the engine tracked which caller sent it. This is
  the same principle as ordering being wiring: everything the engine
  might have been asked to remember is instead something the graph can
  be asked to carry.

- *Does a program dump that cannot mention its outside callers still
  round-trip?* Yes. A program's file records the program's shape, and
  who is entitled to deliver into it is not part of that shape any more
  than a C function's callers are part of its signature. The dump
  round-trips the program; it was never claiming to round-trip the
  world around it.

- *A program whose command line does not fill every input port?* **It
  waits**, and that is simply what happens rather than a condition
  needing a name. A port not yet filled cannot be told apart from one
  that will be filled in a minute, and the answer above forces this: if
  several callers may deliver to one input port, then "the shell did
  not fill port three" is not a mistake, it is port three not filled
  *yet*. Refusing would mean assuming the command line is the only
  caller there will ever be, which this door was specifically built not
  to assume.

  **Waiting needs nothing built.** The stations behind those ports
  never become ready, by the ordinary check, and there is no state
  called "waiting for arguments" distinct from any other station
  waiting for a value.

  **What decides whether it waits forever or ends is already in the
  pool.** [104](completed/104-termination-by-last-sleeper.md) gave an
  outside submitter a way to register a standing promise that more work
  may come, and termination waits while any such promise is held. A
  control socket still open holds one, and the program waits. The shell
  runner holds one while it delivers the command line and drops it
  afterwards. **If nobody is promising to deliver, nothing more can
  arrive, and the program ends by the ordinary rule** — having run
  whatever it could and left the rest unrun.

  So incomplete arguments produce a finished program rather than a hung
  one, and the diagnosis is already legible: the stations that never
  became ready carry a completed-run count of zero, which names exactly
  what did not happen and why nothing downstream of it did either.

## Related

- [209 — The output station](completed/209-map-output-collection.md), the same
  design pointed the other way, and where the pass-through kind is
  described
- [212 — One way to build a program](completed/212-one-way-to-build-a-program.md),
  which is what makes a program constructible and therefore composable
- [004 — Statics and recalculation](../docs/004-datapath-statics.md),
  where starting a program is explained without reference to this
- [402 — Struct constants](completed/402-struct-constants.md), whose
  text reader the command-line path reuses unchanged
- [008 — Map file format](../docs/008-map-file-format.md), which gains
  one declaration and a note about arguments
