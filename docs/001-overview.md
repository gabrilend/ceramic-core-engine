# 001 — Overview

Minimal SoraMech is an engine for building programs out of small C
functions wired together, where the wiring decides what runs and when.
There is no main loop. There is no scheduler you write. You describe a
shape, and the shape executes itself across every core on the machine.

It is a distillation of the larger SoraMech project. That one could
bridge between languages; this one cannot, on purpose. Dropping the
bridge buys focus on the thing underneath it — a runtime that is
multi-threaded by default rather than by effort.

## The three nouns

**A box** is a plain C function you write. It takes its arguments by
value and returns one value. It is not permitted to remember anything
between calls.

**A station** is one placement of a box in a map. It owns the buffers
that hold values waiting to be fed to that box, the mutex that guards
them, and the list of places its output goes. The same box can appear
at several stations; each keeps its own buffers and its own wiring.

**A task** is one invocation. It holds a copy of each input value and
a pointer to the code that will run. It is created the moment a
station's inputs are all present, and destroyed by the worker that
runs it.

The three form a chain of decreasing lifetime: the box is compiled into
the binary, the station lives as long as the program, the task lives
for one call.

## The one rule

> A station runs when, and only when, every one of its input ports
> holds a value.

Everything else in the engine is a consequence of that sentence.

Nothing polls. Nothing scans for ready work. The check happens as the
tail end of a write: whoever just delivered a value into a port then
looks at that station's other ports, and if they are all occupied,
takes one value from each and builds a task. That worker then hands the
task to the pool and goes back to its own business.

So the graph propagates the way a fire propagates — each delivery is
what discovers the next thing to run. There is no central authority
looking for work to do, because the act of finishing is the act of
scheduling.

## What travels on a wire

**A value is atomic and independent.** It carries no relationship to any
other value, and none to whatever produced it. A station takes whatever
is at the head of each of its input buffers and runs; it has no notion
of a round, a batch, or a set of values that belong to each other.

This is easiest to see where a graph splits and rejoins. One station
feeds two paths, and those paths meet again at a third. Send two values
through and the two paths run on different threads at different speeds,
so the station where they meet may pair the first value's result from
one path with the second value's result from the other.

That is the design, not a gap in it. If two things must stay together,
they must **be** one thing — a struct, travelling one wire. Correlation
is something the map author builds out of the values, exactly as state
is something the map author builds out of the wiring.

## What that buys

Parallelism is not something the programmer arranges. Two stations
whose inputs are both satisfied are simply two tasks in the pool, and
whichever workers are free take them. A map with wide fan-out is
parallel because it is wide, not because anyone asked.

The cost is that a box cannot keep state. Two invocations of the same
station may be running at the same moment on different threads, so
anything a box stored would be shared between them. State lives on the
wires instead — to count, you route a box's output back into its own
input, and the running total travels around the loop.

## What is deliberately absent

- **No language bridge.** C functions only. If a program needs to call
  into another language, it writes a box that does so.
- **No visualization, no editor.** Those belong to whatever is built on
  top.
- **No box that can block.** Nothing waits. A worker that cannot make
  progress is a worker that is not running the ten other things that
  are ready.
- **No fallbacks.** A missing file, a mistyped wire, a box that cannot
  be found — all of these stop the program and say why. A program that
  quietly does something else is worse than one that stops.

## Reading order

The documents are numbered to be read straight through.
[002](002-stations-and-ports.md) describes what a station is made of.
[003](003-datapath-delivery.md) follows a value from one box to the
next, which is the core of the engine. Everything after that is either
a variation on that path or the machinery that gets the map into
memory in the first place.
