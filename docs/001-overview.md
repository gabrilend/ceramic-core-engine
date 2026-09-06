# 001 — Overview

```
   in ─┬─ twice ──┐
       │          ├─ total
       └─ plus ───┘
```

`twice` and `plus` run at the same time. `total` waits until both have
arrived. **Nobody wrote either of those facts down** — there is no line
of code anywhere saying "run these two together" or "wait for both".
They are what the picture *is*.

That is the whole idea. You write small C functions, you say what feeds
what, and the shape executes itself across every core on the machine.
There is no main loop and no scheduler you write.

## The three nouns

**A box** is a plain C function. No registration, no macro, no header
to edit:

```c
int add(int a, int b) { return a + b; }
```

It takes its arguments by value, returns one value, and **may not
remember anything between calls** — no statics, no globals. The station
does the remembering instead.

**A station** is one placement of a box. It owns the buffers holding
values waiting for that box, the mutex guarding them, and the list of
places its output goes.

**Two stations placing the same box are two independent things.** Same
compiled code, different buffers, different wiring, different
neighbours. That is the whole reason the word exists: `add` is a
function, but *this* `add` — fed from here, sending there — is a
station.

**A task** is one invocation: a copy of each input value plus a pointer
to the code to run. It is created the moment a station's inputs are all
present and destroyed by the worker that runs it.

**A task is not free**, and that is the trade rather than a footnote.
Every invocation allocates, copies each input, goes on a queue, and is
freed at the end. For work measured in nanoseconds that loses to a
plain function call and always will.

| | lives | as long as |
|---|---|---|
| box | compiled into the binary | forever |
| station | one placement in a map | the program |
| task | one invocation | microseconds |

## The one rule

> A station runs when, and only when, every one of its input ports
> holds a value.

Everything else follows from that sentence, including things that look
unrelated to it. **A box may not remember anything** because two
invocations of one station can be in flight on two threads at once —
so anything a box stored would be shared between them, and the rule
that lets both run is the rule that forbids the storage.

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
        ┌─ slow ──┐
   in ──┤         ├─ meet
        └─ fast ──┘
```

The two paths run on different threads at different speeds. `meet` takes
whatever is waiting at each of its ports and runs. So it can pair the
**first** value's result from `slow` with the **second** value's result
from `fast`, and the answer belongs to neither.

That is not a defect. It is the same independence that let the two
paths run at once without anyone asking them to.

**Things that must stay together have to *be* one value** — a struct on
one wire, not two values that happen to arrive near each other.

## What it buys, and what it costs

Parallelism is not arranged. Two stations whose inputs are satisfied are
two tasks in the pool, and whichever workers are free take them. A map
with wide fan-out is parallel because it is wide.

State lives on wires. To count, wire a box's output back into its own
input — the running total travels round the loop. This is
[`maps/132-the-accumulator.map`](../maps/132-the-accumulator.map), and it
runs:

```
station feed keep p entry
  out 0 - total.0

station total add p
  out 0 - total.1     # back into itself: the running total
  out 0 - seen.0      # and out to be collected

station seen keep p result
```

Feed it 1, 2, 3, 4, 5 and `seen` collects **1, 3, 6, 10, 15** — every
running total, because every run produces one.

Two things this small example will teach you the hard way:

**The loop needs one value to start with.** Deliver a zero to `total.1`
before the first input, or the station never has both ports full and
never runs at all. A back-edge with nothing on it is a program that sits
still.

**The order you take results out in is not promised.** Collecting the
five and keeping the last one gave 15 four times and 6 once. A port has
no head and no tail — which is the same rule as the section above,
arriving somewhere you did not expect it.

**It is a bad fit when the steps are too small.** A box that adds two
integers costs more in task overhead than it saves in parallelism, and
no amount of wiring fixes that. Make the boxes bigger or do it by hand.
It is also a bad fit for work that does not want to be shaped as *run
when every input is present*; fighting that sentence goes badly.

## What is absent, and what is not

Deliberately absent:

- **No language bridge.** C functions only. A program that needs
  another language writes a box that calls into it.
- **No visualization or editor in the engine.** Those are built on top.
- **No box that can block.** A worker that cannot make progress is a
  worker not running the ten other things that are ready.
- **No fallbacks.** A missing file, a mistyped wire, a box that cannot
  be found — each stops the program and says why. A program that
  quietly does something else is worse than one that stops.

Present, and easy to miss:

- **A running program can be rewired** — wires cut and drawn, stations
  added and removed, while workers are in flight.
- **A box can be compiled and placed while the program runs.**
- **A running program can be written to disk and picked up again**,
  because with no hidden state anywhere, everything a program is lives
  in a graph that can be walked.

## Where to go next

| | answers |
|---|---|
| [002](002-stations-and-ports.md) | why a station, rather than just a function |
| [003](003-datapath-delivery.md) | what actually happens when a value moves — the core of the engine |
| [004](004-datapath-statics.md) | how a port holds a constant, and why writing one starts things |
| [005](005-routing.md) | how a station sends to one of several exits |
| [008](008-map-file-format.md) | how to write the file that says what feeds what |
| [058-guarantees.md](058-guarantees.md) | every promise the runtime makes, and its price |

Read 002 and 003 and you have the engine. The rest is variation on that
path, or the machinery that gets a map into memory.

## A note on the name

This is a distillation of the larger **SoraMech** project, which lives
on this repository's `original` branch and bridges between languages.
This one deliberately cannot, and dropping the bridge is what buys the
focus on the runtime underneath.

The two names are one word: *soramech* and *ceramic* have the same
consonants in the same order. The engine's own files are `cera.c` and
`cera.h`.
