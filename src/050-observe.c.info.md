# 050-observe.c — the engine saying out loud what it already knew

The reporting half of phase 7. Nothing here decides anything: the
numbers have been kept all along — growth counts and high-water marks
since the station layer was built, run counts riding the delivery
path, timing from the shims when it is compiled in — and this file
reads and formats them.

| Function | Takes | Gives | Does |
|---|---|---|---|
| `map_report_buffers` | a program, a stream | nothing | Which ports have grown and how deep the backlog reached, plus the task queue's own growth. |
| `map_report_stations` | a program, a stream, an ordering | nothing | Per-station run counts, values produced, and time spent when timing is compiled in. |
| `map_report_shutdown` | a program | nothing | The parting word: anything that grew far enough to be worth complaining about. |
| `map_observe_start` | a program, a path, an interval | nothing | Emits a report on a timer, from a thread that is not a worker. |
| `map_observe_stop` | a program | nothing | Stops it. |
| `sora_stats_box_time` | a task, nanoseconds | nothing | How long one box took, charged onto the task by its shim. |

## Reading a machine that is still moving

**The walk takes no lock**, and the numbers are stale by a moment.
That is the nature of observing something live, and it is the right
trade: a report that stopped the engine to be exact would change the
thing it was measuring.

**The timer thread is not a worker and pushes nothing.** Termination
is decided by the last worker to fall asleep finding no work, so
anything that could push after that moment would break it. This
watches and never contributes.

## Three piles, three different diagnoses

This is the part worth knowing, because the numbers look alike and
mean opposite things.

**A port backing up** means one input side is outpacing its siblings —
values arriving faster on one arrow than the station can pair them
with on another. Memory is absorbing the difference.

**The task queue backing up** means the workers are behind the
program: more became runnable than there were threads to run it.

**Results backing up** means *nobody is collecting the program's
results at all*. This one is louder than the other two by design. The
first two are performance signals worth a line in the teardown report;
this is a program computing into somewhere nobody is looking, and
waiting until shutdown to mention it wastes the entire run — so it is
said from the first doubling, naming the station.

## The shout at the end

A port grown past a threshold gets a complaint at shutdown, per the
rule that a warning is an error nobody has decided about yet. **The
number lives here and nowhere else**, deliberately: a threshold quoted
in a document is one that goes stale silently.

It counts **pages**, not doublings, and it moved when growth stopped
doubling and started appending. Sixteen pages is the same backlog four
doublings used to mean — left where it was, the same complaint would
have fired whenever a consumer was briefly slow, and a warning that
cries wolf costs more than it is worth.

## Naming a station

Reports name a station the way a map file named it, or by its index
when nothing did. A program built by calling the construction surface
has no names at all, and a report that says `?` about it is one nobody
can act on.
