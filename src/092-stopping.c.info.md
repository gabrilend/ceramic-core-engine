# 092-stopping.c — the ways a program ends, from inside

Interface and the reasoning behind it in `091-stopping.h.info.md`.
This is what the file actually contains.

## The shape

**One loop around one `sigwait`.** Four numbers can come back and each
one is a branch:

- **the finished signal** — the pool ran out of work and said so.
  Collect the threads, return zero.
- **polite shutdown** — set the program's closing flag and go back to
  waiting. Nothing else. The entrance refuses from that moment, which
  is the whole of what "stop accepting new work" can mean, and the
  ordinary ending does the rest.
- **interrupt** — stop starting new things, unblock the escape,
  gather everything, return 130.
- **quit** — write what needs no lock, abort.

## The two reports

**`report_everything`** runs on the waiting thread and is free to take
every lock it likes, because the thread holding this number is running
ordinary code rather than sitting in a handler. It writes per-station
run and produced counts by name, per-port depth, high water and growth
count, which worker is inside which station, and then the program
itself rendered as a map file — because somebody diagnosing a program
that was edited while it ran needs the shape it had at the end, not
the shape the file on disk describes. Those stopped being the same
thing the moment a program could be built while running.

**`report_without_locks`** takes nothing. It reads atomic counters and
one integer per worker, names stations by index, and follows no
pointer another thread could be freeing. A shelf pointer is safe to
follow — the table grows by adding shelves and nothing already placed
moves — so reading a count that is one behind means missing the newest
station, which is a smaller wrong than not reporting at all.

**`say` formats into a stack buffer and writes once**, because `write`
is the only file operation available on every path in this file,
including the one forbidden to take a lock. A buffered stream takes
one.

## What is deliberately absent

**No watchdog.** Detecting a wedge from inside cannot be done. The
last-sleeper rule is structurally incapable of it: it fires when
workers go to *sleep*, and a worker in an infinite loop never sleeps.
A progress counter comes closer — every station carries an always-on
run count — but the threshold is unavoidably a guess, because sixteen
workers each legitimately running a very long box look identical to
sixteen wedged ones. That is the halting problem wearing work clothes.
So the signal comes from outside, where somebody who actually knows
how long is too long is the one deciding.

**No stopping a running box.** There is no safe way to interrupt
executing C. The cancellation facility only acts at cancellation
points, so a tight loop containing no system call never reaches one;
even when it fires it does not unwind C code, and any mutex the thread
held stays locked forever — which would freeze every thread delivering
into that station, a worse outcome than the wedge. So "halt
everything" honestly means **stop starting new things**.

**No clock.** See the polite path in the interface notes: the only
clock that knows how long is too long belongs to whoever sent the
signal.
