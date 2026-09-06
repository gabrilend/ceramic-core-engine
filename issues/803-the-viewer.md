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

### What stands between the ring and the page — decided

**A forwarding reader.** A separate program maps the ring and pushes
events down a socket the page opens. It does nothing else, which is what
keeps *cannot touch* true by construction rather than by discipline: a
program that can only forward cannot be talked into writing.

The cost is stated rather than hidden — **this project gains its first
long-running program**, and a port to pick. The two alternatives were a
file something has to keep handing the page, which needs a server
anyway and adds the poll interval as latency, and a regenerated static
page, which is a photograph rather than a view and makes lighting and
buffer depth close to meaningless.

### Where the picture comes from — decided

**The map file.** The viewer reads it, counts the stations, and knows
the whole graph before a single event arrives — including branches that
have not run and may never run. Learning the shape from the trail alone
was refused: a program with a rare branch would draw wrong for a long
time and the page would have no way to know which parts it had not seen.

Rewiring events from the trail keep it current afterwards, so a program
edited while it runs stays drawn correctly.

### The layout

**A grid, as close to square as the station count allows, preferring an
extra column to an extra row.** Boxes are placed in it with padding
between them and the wires drawn as lines. That is the whole of the
automatic layout: no force simulation, no ranking, nothing that moves
under you while you are looking at it.

**Dragging rearranges and changes nothing.** Where a box sits is a fact
about the picture, not about the program. Nothing a drag does can reach
the thing being watched.

**A layout can be saved beside the map**, as a small file of its own —
`<name>.lay` next to `<name>.map`. Saved only when asked for. The map
file is never touched, because the viewer does not write to the thing it
is describing any more than it writes to the program.

### A box that appears while you are watching

A station added to a running program has to go somewhere, and the
obvious answer — put it near the ones it is wired to — does not work at
the moment it is needed. **Creating a station and wiring it are separate
events**, so when the box first appears it has no wires at all.

So placement happens in two stages:

1. **On creation**, the box is appended to the grid, in the next free
   cell. This is always possible and never wrong, only uninformative.
2. **When its wires arrive**, if nobody has dragged it, it moves once to
   a spot roughly equidistant from the stations it connects to, nudged
   until it overlaps nothing.

**A box a person has dragged is pinned and never moves again.** The rule
is one sentence: the automatic layout may place a box the viewer has
never touched, and may not move one they have. A picture that rearranges
itself under a hand is worse than a picture in the wrong order.

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

- **How much history does it hold?** A view of *now* needs none. A view
  that answers "what happened just before it wedged" needs some, and the
  amount is the difference between a dashboard and a recorder.

## Related

- [802 — A program you can watch](802-a-program-you-can-watch.md), the
  trail this reads and the only thing it depends on
- [801 — The workbench in the browser](801-browser-workbench.md), which
  it shares nothing with, deliberately
- [703 — The map dump](completed/703-map-dump.md), where the graph
  comes from
- [701 — Buffer growth reporting](completed/701-buffer-growth-reporting.md),
  which is the thing most worth drawing
