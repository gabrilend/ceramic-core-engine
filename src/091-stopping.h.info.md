# 091-stopping.h — the ways a program ends, from outside

Running out of work is the happy ending and belongs elsewhere: the
last worker to fall asleep looks once more, finds nothing, and stops
everyone. This is every other way — a program **told** to stop, and a
program that found an instruction it refuses to continue past.

## The one idea

**No handler is installed anywhere.** The three signals are blocked in
every thread, and the thread that started the program waits for one to
arrive as an ordinary value.

That removes the hardest constraint in the whole design. A signal
handler may call almost nothing; a thread that woke up holding a
number may call anything at all — take locks, format text, walk the
station table. The restriction was the difficult part and it turned
out to be avoidable rather than manageable.

The pool's own ending arrives at that same waiting point as one more
signal. **One place to wait, woken for two reasons, told apart by
which number came back.**

There is exactly one exception, and it is described under the second
interrupt below.

## Functions

| Function | Takes | Gives | Does |
|---|---|---|---|
| `sora_prepare` | where the report should go, or NULL for the RAM tier | — | Blocks the three signals and opens the report's destination. **Before any thread exists**, because a thread inherits the mask of whoever made it — which is what makes "every thread" true without visiting any of them. |
| `sora_report_path` | — | the path | Where a report will be written. |
| `sora_wait` | a program | the exit code | Waits for the program to end, however it ends. The pool must already be released. |
| `sora_stop_now` | a program (or NULL), an exit code, a reason | never returns | An invalid operation ending the program, having first said everything it can. |

**The report's destination is opened during preparation, not while
dying.** It lives on a filesystem a reboot empties, so it may be
absent, and creating it on a failure path means a system call that can
fail for reasons a dying program can do nothing about. A descriptor is
an integer, and writing to an integer is the one file operation
available on every path here — including the one forbidden to take a
lock.

## The three signals

They form a progression, and the ordering is the design: each needs
less cooperation from the program than the one before it, and each is
the right answer for a program in worse condition than the last.

| | polite shutdown | interrupt | quit |
|---|---|---|---|
| who sends it | a service manager | a person at a terminal | a person who wants evidence |
| what it means | wind down, there is time | stop, and tell me why | stop now, leave the body |
| diagnostics | none | everything | only what needs no lock |
| waits for running boxes | yes | no | no |
| how it ends | zero, by the existing rule | 130, explicitly | aborts, leaving a core |

**The polite path adds no mechanism at all**, which is the argument
for it. It shuts the one door the outside can push work through and
goes back to waiting, so the program ends exactly the way it would
have ended on its own. It writes nothing, because a supervisor
stopping a healthy program does not want a report it did not request.

**It borrows a clock rather than inventing one.** On a wedged program
this path never completes, which is correct: whatever sent the signal
already has a timer and will escalate to the uncatchable kill. That
supervisor's clock is the only one in the system that knows how long
is too long for this deployment, and a guess made here is wrong on a
slow machine and wrong differently on a fast one.

**The interrupt gathers on the waiting thread itself**, not as a task
somebody hopes gets scheduled. That is what lets it work on a program
whose every worker is wedged — the queue guarantees nothing to
anybody, and the one piece of work that must happen cannot be the one
piece of work standing in line.

**A second interrupt is the one place a handler exists.** A thread
that is gathering is not asking for signals, so a second interrupt
would sit pending until the gather finished — and the gather is
exactly the thing that may never finish, because it takes a station's
mutex and the reason somebody is pressing the key twice may be that a
mutex is held by something that will never release it. So for the
length of the gather, and only then, that one signal is unblocked with
a handler that calls `_exit` and nothing else.

**The quit takes no locks at all.** It writes what is readable without
cooperation — atomic counters, and one integer per worker saying which
station it is inside — and then aborts, leaving a core so a debugger
sees every thread's stack including the box that is not returning.
Stations are named by index rather than by name, because the names are
an array somebody may be growing right now and reading it would be the
crash this report exists to explain.

## Exit codes

Everything below the signal offset belongs to the program; the offset
and above belongs to signals by convention, so no meaning here ever
reaches up there.

| code | meaning |
|---|---|
| 0 | ran out of work, or was politely asked to stop |
| 65 | a map file the engine refused — the input was malformed |
| 70 | an invalid operation from a program's own construction calls — the calling code was wrong |
| 71 | out of memory, or another resource no edit can fix |
| 130 | interrupted by a person |

The three middle codes make the distinction this project already
draws — a fault the caller can correct and retry, against one it
cannot — visible to a shell script rather than only to somebody
reading the message. The interrupt code is the signal number added to
the conventional offset, so every shell that already understands "this
was interrupted" keeps understanding it.

**The quit path is not in the list.** It aborts, and the shell
computes its own number from the signal. Naming one here would be
inventing an agreement that already exists.
