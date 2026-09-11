# 001 — Overview

**The ceramic core engine is a C library, in one source file, that runs
your program on every core of the machine automatically.**

You write ordinary C functions. You write a second file saying which
function feeds which, and the engine works out how to run your design
across the cores it finds.

It is two files to add to a project (`cera.c` and `cera.h`), it needs a
C compiler and nothing else, and it is licensed AGPLv3.

---

Here's an example. These are all simple C functions, you can probably
guess what they do. It's not important.

```
   input() ─┬─ twice() ──┐
            │            ├─ total() ── print()
            └─ plus() ───┘
```

`twice` and `plus` run at the same time. `total` waits until the
results from both have arrived before `print`ing.

That's the whole idea. You write C functions, you say what feeds what,
and the shape executes itself across every core on the machine.

## The pieces

**A box** is a plain C function you write:

```c
int add(int a, int b) { return a + b; }
```

It takes its arguments by value, returns one value, and **may not
remember anything between calls** — no statics, no globals.

**A station** is one placement of that box: its own **input ports**,
its own **output ports**, and its own wiring saying where each output
goes and where each input comes from. Two stations placing the same box
are two independent things — same compiled code, different ports,
different neighbours. That is why the word exists: `add` is a function,
but *this* `add`, fed from here and sending there, is a station. Field
by field, that is [002](002-stations-and-ports.md).

**A task** is one invocation made concrete: a copy of each input value,
a pointer to the function to run, and somewhere to put what it returns.

**The thread pool** is a queue of tasks. A worker thread with nothing to
do asks the pool for one, takes ownership of it, runs the function
inside it, and delivers the returned value into whichever input ports
the wiring names. A returned value can only land in an input port;
there is nowhere else for one to go.

**A readiness check** happens at the instant a value is delivered — by
the worker that just delivered it, before it goes back to its own
business. It looks at the receiving station's other ports. If every one
of them now holds a value, it takes one from each, builds a task, and
puts it on the pool.

Those two paragraphs are the engine. Everything else is detail.

**A task is not free**, and that is the trade rather than a footnote.
Every invocation allocates, copies each input, goes on a queue, and is
freed at the end. For work measured in nanoseconds that loses to a
plain function call and always will.

| | lives | as long as |
|---|---|---|
| box function | compiled into the binary | forever |
| station | one placement in a map | the program |
| task | one invocation | microseconds |

## The one rule

> A station can run only when each of its input ports holds a value.

Everything else follows from that sentence, including things that look
unrelated to it. **A box may not remember anything** because two
invocations of one station can be in flight on two threads at once — so
anything a box stored would be shared between them, and the rule that
lets both run is the rule that forbids the storage.

### Nothing looks for work

There is no ready-queue to scan, no work-stealing search, no thread
waking to look around, no polling of any kind.

The check is the tail end of a write. Whoever delivered a value into a
port then looks at *that station's* other ports; if all are occupied, it
takes one value from each, builds a task, hands it to the pool, and goes
back to its own business. It never examines a station that did not just
receive something.

So scheduling costs nothing, because nothing schedules. The act of
finishing is the act of discovering what runs next.

## Do two values stay together?

**No.** This is the thing most worth understanding before you build
anything, and the answer surprises everybody.

Send two values into a graph that splits and rejoins:

```
            slow()
        ┌── 0    0 ──┐
input() ┤            ├── meet()
        └── 0    0 ──┘
            fast()
```

The two paths run on different threads at different speeds. `meet` takes
whatever is waiting at each of its ports and runs. So it can pair the
**first** value's result from `slow` with the **second** value's result
from `fast`, and the answer belongs to neither.

That is not a defect. It is the same independence that let the two paths
run at once without anyone asking them to.

**Things that must stay together have to *be* one value** — a struct on
one wire, not two values that happen to arrive near each other.

## What it buys, and what it costs

Parallelism is not arranged. Two stations whose inputs are satisfied are
two tasks in the pool, and whichever workers are free take them. A map
with wide fan-out is parallel because it is wide.

**State lives on wires.** To count, wire a box's output back into its own
input — the running total travels round the loop. Two functions are the
whole of the C:

```c
int keep(int x)         { return x; }        /* hands a value on unchanged */
int add(int a, int b)   { return a + b; }    /* the arithmetic */
```

and the map is [`maps/132-the-accumulator.map`](../maps/132-the-accumulator.map):

```
station feed src/boxes/029-demo-boxes.c:keep p
  in 0 - 0$
  out 0 - total.0

station total src/boxes/029-demo-boxes.c:add p
  in 0 - feed.0
  in 1 - total.0
  out 0 - total.1     # back into itself: the running total
  out 0 - seen.0      # and out to be collected

station seen src/boxes/029-demo-boxes.c:keep p
  out 0 - 0$
  in 0 - total.0
```

Every part of that notation — the file-and-function address, both ends
of every wire, the `$` that marks the map's edge — is
[008](008-map-file-format.md). What matters here is that `feed` and
`seen` place the *same function* and are still two separate stations.

`feed` and `seen` place `keep`, a function that does nothing, and **they
are a convenience rather than a requirement.** The marks sit on ports,
so `total`'s own ports can carry them and the middle station can be the
whole program:

```
station total src/boxes/029-demo-boxes.c:add p
  in 0 - 0$
  in 1 - total.0
  out 0 - total.1
  out 0 - 0$
```

That is the same accumulator in one station instead of three. The file
keeps the longer version because the walk-through below is easier to
follow with the way in and the way out having names.

### Following one value through

Deliver a `0` to `total.1` to prime the loop, then send in a `1`:

1. `1` arrives at `feed`'s input port. `feed` has one port and it is now
   full, so the readiness check passes and a task goes on the pool.
2. A worker runs `keep(1)`, which returns `1`. The wiring says exit 0
   goes to `total.0`, so the worker writes it into that port's ring
   buffer and then checks `total`.
3. `total` has two ports. Port 0 now holds `1`; port 1 holds the `0` we
   primed it with. **Both are full**, so the worker takes one value from
   each, builds a task, and pushes it.
4. A worker runs `add(1, 0)` and gets `1`. That exit is wired to two
   places, so the value goes to both: back into `total.1`, and out to
   `seen`.
5. `seen` runs `keep(1)`. Its output port 0 is marked as the map's
   result 0, so if a caller has said where to put results, the value is
   written into the next free slot of that array. If nobody has said, the
   value is discarded exactly like any other output wired nowhere — a
   marked port is not a bucket, and a program nobody collects from grows
   nothing.

Now `total.1` holds `1` and `total.0` is empty, so nothing runs until the
next input arrives. Send `2` and the same walk produces `3`. Feed it 1,
2, 3, 4, 5 and `seen` collects **1, 3, 6, 10, 15** — every running total,
because every run produces one.

Two things that small example teaches:

**The loop needs one value to start with.** Deliver a zero to `total.1`
before the first input, or the station never has both ports full and
never runs at all. A back-edge with nothing on it is a program that sits
still.

**The order you take results out in is not promised.** Collecting the
five and keeping the last gives the largest most of the time and a
smaller one sometimes. A port has no head and no tail — which is the
rule from the section above, arriving somewhere you did not expect it.

**It is a bad fit when the steps are too small.** A box that adds two
integers costs more in task overhead than it saves in parallelism, and no
amount of wiring fixes that. Make the boxes bigger or do it by hand. It
is also a bad fit for work that does not want to be shaped as *run when
every input is present*; fighting that sentence goes badly.

## What is absent, and what is not

Deliberately absent:

- **No language bridge.** C functions only. A program that needs another
  language writes a box that calls into it.
- **No visualization or editor in the engine.** Those are built on top.
- **No box that can block.** A worker that cannot make progress is a
  worker not running the ten other things that are ready.
- **No fallbacks.** A missing file, a mistyped wire, a box that cannot be
  found — each stops the program and says why. A program that quietly
  does something else is worse than one that stops.

Present, and easy to miss:

- **A running program can be rewired** — wires cut and drawn, stations
  added and removed, while workers are in flight.
- **A box can be compiled and placed while the program runs.**
- **A running program can be written to disk and picked up again**,
  because with no hidden state anywhere, everything a program is lives in
  a graph that can be walked.

## Where to go next

| | answers |
|---|---|
| [002](002-stations-and-ports.md) | why a station, rather than just a function |
| [003](003-datapath-delivery.md) | what actually happens when a value moves — the core of the engine |
| [004](004-datapath-statics.md) | how a port holds a constant, and why writing one starts things |
| [005](005-routing.md) | how a station sends to one of several exits |
| [008](008-map-file-format.md) | how to write the file that says what feeds what |
| [058](058-guarantees.md) | every promise the runtime makes, and its price |

Read 002 and 003 and you have the engine. The rest is variation on that
path, or the machinery that gets a map into memory.

## A note on the name

This is a distillation of the larger **SoraMech** project, which lives on
this repository's `original` branch and bridges between languages. This
one deliberately cannot, and dropping the bridge is what buys the focus
on the runtime underneath.

The two names are one word: *soramech* and *ceramic* have the same
consonants in the same order. The engine's own files are `cera.c` and
`cera.h`.
