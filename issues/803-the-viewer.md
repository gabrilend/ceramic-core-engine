# 803 — The viewer

A page that watches a running program and **cannot touch it**. Stations
light up as they run, values move along wires, buffers fill and drain,
and nothing on the page can reach back into what it is looking at.

It reads the trail [802](802-a-program-you-can-watch.md) leaves and
does nothing else.

## Current behavior

**Watching a running program means reading stderr.** The observer
prints a line when results pile up somewhere nobody is taking them
from; a dying program writes a diagnostic report
([106](completed/106-stopping-on-purpose.md)); a program put down
writes one beside its capture
([712](completed/712-capturing-a-running-program.md)). All three are
text, after the fact, and none of them is a picture.

**And the shape of a program is not visible anywhere while it runs.**
The dump writes the graph as a list of lines
([703](completed/703-map-dump.md)), which is the same problem the map
file has: the graph a file describes is not visible in the file.

## Intended behavior

### It only looks

**Nothing on this page changes anything.** There is no button that
starts a station, no field that writes a constant, no wire that can be
drawn. A viewer that could reach in would be a debugger, and a debugger
is a different thing with different rules — chiefly that its presence
is allowed to change what it is looking at, which is exactly what
[802](802-a-program-you-can-watch.md) refuses.

Being unable to touch is what makes it safe to point at a program that
matters. Somebody watching a live thing should not be one mis-click
from changing it.

### It shares nothing with the workbench

**Decided rather than drifted into**, and the same decision the phase
already made about the demo panels.

The workbench ([801](801-browser-workbench.md)) is a canvas for
composing a map that does not exist yet: every station is alike,
nothing is running, and the page's whole job is to let somebody make
changes. This is the opposite on all three counts. A shared drawing
layer would have to serve a picture where the interesting thing is
*which station is busy right now* and a picture where the interesting
thing is *what somebody is about to connect*, and a layer serving both
serves neither.

What may be shared is the **understanding of the map file format**,
because there is one format and two readers of it would be two things
that must agree.

### What it shows

- **The graph**, laid out, from a dump the program was asked for once.
- **Each station lighting as it runs**, and dimming as it goes quiet,
  so a bottleneck is the station that never dims and a dead branch is
  the one that never lights.
- **Each buffer's depth**, because a buffer filling is the single most
  useful thing to see: it says one input side is outrunning another,
  which is the fault this engine makes easiest to create and hardest to
  notice.
- **What it missed.** When the trail says events were lost, the page
  says so, in the picture rather than in a corner. A view quietly
  missing events is a view somebody will trust.

### How it gets the trail into a browser

**This is the one genuinely awkward part and it should be admitted
early.** A page cannot map shared memory. So something has to stand
between the ring and the page, and what that something is decides how
much of this issue is about plumbing.

The obvious answers, in the order they cost:

- **A tiny reader that serves the events over a socket the page opens.**
  A separate program, doing nothing but forwarding — which keeps the
  "cannot touch" property intact by construction, since forwarding is
  all it can do.
- **A file the page polls.** No server, but a page cannot read a local
  file it was not handed, so somebody has to hand it one repeatedly.
- **The reader writes a page.** No live view at all; a snapshot
  regenerated. Cheapest and weakest.

**Undecided, and it is the first thing to decide**, because the rest of
the page is the same either way and this determines whether the phase
gains a running program nobody asked for.

## Suggested implementation steps

1. Read the trail in a program that is not a browser, and print what it
   sees. This is [802](802-a-program-you-can-watch.md)'s test grown up,
   and it proves the format before anything is drawn.
2. Decide the bridge, in the open question below.
3. The graph, drawn from a dump, with no live data in it at all.
4. The lighting: stations reacting to events, and the page saying so
   when events were lost.
5. Buffer depths.
6. Pointed at a program doing something worth watching — a demo, an
   example, whatever phase 8's panel turns out to be.

## Open questions

- **What stands between the shared ring and the page?** The three
  answers above, and the choice decides whether phase 8 gains a
  long-running program. Nothing else in this project has one, and that
  is worth weighing rather than accepting.
- **Does the viewer need the program's dump, or can it learn the graph
  from the trail alone?** The trail says a station ran and a value
  moved from one to another, so the shape could be discovered by
  watching. Discovering it means the picture is incomplete until
  everything has run at least once, which for a program with a rare
  branch is a picture that is wrong for a long time. Asking for a dump
  once is exact and needs a second channel.
- **How much history does it hold?** A view of *now* needs none. A view
  that answers "what happened just before it wedged" needs some, and
  the amount is the difference between a dashboard and a recorder.
- **Does watching a program that has finished mean anything?** If the
  trail outlives the process — [802](802-a-program-you-can-watch.md)
  has not decided — then the same page could replay a dead program, and
  it is worth knowing whether that is one feature or two.

## Related

- [802 — A program you can watch](802-a-program-you-can-watch.md), the
  trail this reads and the only thing it depends on
- [801 — The workbench in the browser](801-browser-workbench.md), which
  it shares nothing with, deliberately
- [703 — The map dump](completed/703-map-dump.md), where the graph
  comes from
- [701 — Buffer growth reporting](completed/701-buffer-growth-reporting.md),
  which is the thing most worth drawing
