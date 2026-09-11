# 803 — The viewer

A page that watches a running program and **cannot touch it**. Stations
light up as they run, values move along wires, buffers fill and drain,
and nothing on the page can reach back into what it is looking at.

It reads the trail [802](802-a-program-you-can-watch.md) leaves and
does nothing else.

## Current behaviour

**Built, and running.** Three files and a program:

- **`viewer/119-viewer.c`** — the forwarding reader. It maps a watched
  program's ring with the engine's own reader, serves the page and the
  map over HTTP, and pushes events down a server-sent event stream. One
  thread, one poll loop, a reader per connected page — which is why two
  people watching do not interfere.
- **`viewer/120-viewer.html`, `121-viewer.css`, `122-viewer.js`** — the
  page. It reads the map file, lays the stations out on a near-square
  grid, draws the wires, lights each station as it runs, fills a pip per
  input port as its backlog deepens, and says so when the trail reports
  loss.
- **`viewer/123-a-program-to-watch.c`** — the engine's example graph fed
  at a human pace and left running until interrupted, because a program
  that finishes in four milliseconds is correct and useless to watch.

`make viewer` prints the two commands.

### It only forwards, and the test says so

There is no path the server answers by writing anything, anywhere.
Saving a layout is a **download the browser performs** — the file lands
in somebody's downloads and putting it beside the map is their own act,
after which the viewer will read it. That keeps the whole program a
window: it has no write in it at all.

`tests/124-test-the-viewer.sh` starts a watched program, starts the
viewer, and checks that the page and its parts come back, that anything
it does not know is refused, and that thousands of events arrive down a
socket **in sequence**. A stream out of order would be a picture that
cannot be trusted about what happened before what.

### The second pass

Six things, after looking at the first one:

**A blueprint.** Pale lines on blue, with the paper's own ruling under
everything. The issue files here are called blueprints and read like
them; this is the same drawing with the machine running inside it.

**Ports on the edges, and wires between ports rather than boxes.**
Which port a wire lands on is the whole of what a wire says, so the
picture now says it. Inputs down the left, exits down the right, each
one labelled and each one showing what it is: a ring buffer that can
back up, a constant that always holds a value, or no source at all.

**Values you can watch move.** A dot crosses the wire it moved along,
over half a second, its position worked out from the curve directly
rather than by asking the browser to measure the path — a measurement
per frame per value is the one thing that would make this page cost
something.

**Buffer depth, derived rather than reported.** The trail never carries
a value and never carries a depth. But a value arriving at a ring port
is one more waiting, and a station running takes one from each of its
ring ports — which is the readiness rule the engine itself follows,
applied to the same events the engine emitted. The port fills as its
backlog grows and turns when it gets deep.

**A box is a function, so it is drawn as one**, with parentheses.

**The program draws itself.** A trail carries station *indices*, and
turning an index into a box by counting down a map file is how a
picture ends up confidently wrong — point it at a map of the same shape
in a different order and every event lands on the wrong station. The
watched program now dumps its own live graph and the viewer reads that,
so what is drawn is what is running by construction. The dump also says
each station's index outright and each buffer's capacity, both of which
the page now reads instead of inferring.

### The third pass: a camera, and a speed that means something

**The paper has no edges.** The window used to be resized to fit its
contents on every frame, so dragging a box past the old boundary made
the whole graph jump. There is a camera now: drag the paper to pan,
scroll to zoom about the pointer, and the ruling coarsens as you pull
back so it never becomes a wash of lines. Nothing is ever resized to
fit anything.

**It opens looking at the entrance**, at a scale that shows the whole
graph if the whole graph will fit comfortably and comes in to a
readable size if it will not. A program is read from its way in
outward; a picture nobody can read is not an improvement on no picture.

**A value crosses at a fixed speed rather than in a fixed time.** It
used to take the same half second down every wire, so a long wire
looked faster than a short one. The trail says nothing about how long a
delivery took — it carries the moment, not a duration — so any speed
here is a picture rather than a measurement. Given that, the honest
choice is the one where distance on screen means something.

### A program built to show its own workings

`viewer/126-a-mechanism-to-watch.c`, and it is the answer to *what does
a ring buffer actually look like*:

```
feed ──> gate ─┬─ less    ──────────────> collect.0
        (comparator)
               ├─ equal   ──────────────> collect.1
               └─ greater ──> spread ─┬─> collect.2
                            (iterator) └─> drain
```

`feed` counts, so `gate` sees 0 to 29 over and over. A comparator routes
by its box's result against the threshold on its last port — ten here —
so of every thirty values **ten go left, exactly one goes down the
middle, and nineteen go right**, and the iterator halves the right-hand
stream again by dealing alternate values to a drain.

`collect` takes three inputs and cannot run until all three hold a
value, so it runs at the rate of its slowest feed while the other two
buffers grow. Measured over six seconds: 170, 17 and 161 values arrived
at its three ports, it ran 17 times, and the two fast ports were left
holding 153 and 144 while their buffers grew from ten slots to forty.

**That is the whole demonstration.** Memory quietly absorbing an
imbalance is what a ring buffer does, and this is what it looks like
while it happens.

All three station kinds appear, which was not the point but is worth
having: a plain station, a comparator with three exits, and an iterator
dealing round-robin.

### Routing by range, which needs a ladder

A comparator asks exactly one question — is my box's result less than,
equal to, or greater than the value on my last port — and exits 0, 1
and 2 **are** those three answers. Nothing in a map file chooses what an
exit means, and there is nowhere to put a second threshold: the only
number is the constant on the last port. So three ranges need two
comparators in succession, each taking what falls below its own
threshold and handing the rest down.

`maps/127-the-ladder.map` is that, and `viewer/128-watch-a-map.c` runs
any map the binary was built with, so a shape can be watched without a
program written for it.

**Every comparator in it has an unwired equal exit that can never
fire**, which is what the doubling at the front is for. A threshold of
nineteen against values that are always even puts the boundary at
nine-and-a-half: between the integers rather than on one. Compare
against ten with the values undoubled and the number ten itself lands on
`equal`, which then has to be wired somewhere or thrown away — an
unwired exit discards, and a silently discarded value is worse than an
extra wire.

Measured: 130, 130 and 122 values reached `collect`'s three ports, it
ran 122 times, and **exit 1 was never used by either comparator**.

### Chevrons rather than dots

There were dots crossing the wires, one per delivery, and they were a
lie in two directions. The trail carries no values, so a dot stood for
nothing in particular; it carries no durations either, so the speed was
invented. And it showed: a dot was still crawling along a wire after the
station at the far end had already run and drained the value it was
pretending to be.

A wire now shows that it is **carrying**, not what it carries. It
brightens when something crosses and fades over about a second, and
while it is lit, chevrons march along it toward the destination. The
marching says *this way*; the brightness says *just now*; neither claims
to be a particular value in a particular place. Their offset is a
function of the clock alone, so every live wire flows at the same rate
and none of them can be read as tracking one delivery.

Direction is said once at the end of each wire by an arrowhead, which
brightens with the wire.

### How long *just now* lasts

A wire lit for most of a second is lit for longer than the program's own
rhythm, and then the light outlives the truth. The ladder alternates:
ten values down one exit of a comparator, then ten down another, which
at a sixth of a second each is a **burst of six hundred milliseconds**.
A wire staying lit for nine hundred was therefore still lit through the
whole of the *other* exit's burst — so two exits of one comparator
looked simultaneously live, which is a picture of something that cannot
happen. One value goes to one exit.

A wire is lit for 160 milliseconds now and a station glows for 220. The
floor is not zero and no number removes it: the server drains the ring
on a forty millisecond poll and the page redraws every thirty-three, so
about eighty milliseconds of lag is structural. What the shorter window
buys is that nothing is shown as live once it has stopped.

**Checked rather than eyeballed.** `scripts/130-replay-the-page.lua`
applies the page's own rule to a captured event stream — reading the
window out of the page's source rather than restating it — and reports
how much of the time two exits of one comparator are shown live
together. A little is honest, at the moment a burst hands over; a lot
means the drawing is describing the past as the present.

At 160 milliseconds it is 5.7% of the time. At the 900 it replaced,
52.5%. `tests/129-test-what-the-page-shows.sh` fails above twenty, and
was proved by putting the old number back and watching it fail.

This is the only check in the project that tests a drawing, and it needs
no browser: what is being checked is the arithmetic a browser would do.

### Zoom, properly

The first attempt read the wheel's raw number, and a wheel reports its
movement in one of three units — pixels, lines or pages — whichever the
browser feels like. The same gesture arrives as 3 from a mouse and 300
from a trackpad, so one barely zoomed and the other lurched.

Now it converts first and moves in even steps of a fixed ratio, bounded
so one violent flick is one firm zoom. Zooming holds the point under the
pointer still, which is the whole of what makes it feel right. There are
buttons and a percentage, `+` `-` `0` from the keyboard, and `f` or a
double-click to frame the whole graph again — which is the way back from
having zoomed into a corner and lost the rest.

### Arriving late is not falling behind

A page said **11,794 events lost** on a program that had lost nothing.
Every reload starts a new reader, and a new reader began at the first
event the program ever wrote, so everything before the tab was opened
was reported as loss. That is arriving late described as a fault, and it
made every reload look like a broken view.

A reader now starts at the oldest event **still in the ring** and says
where it came in. Loss means what it should: events overwritten while
this reader was already attached. The page shows the two differently —
one quietly, one in red — because only one of them means the picture is
unreliable.

The ring also grew. It was sized for a few hundred events per station;
it is now a few thousand, with a floor of sixty-five thousand slots,
which is two megabytes of shared memory and nothing at all against being
told you missed something because the buffer was small rather than
because you were slow.

### Somebody else's screen

The viewer listens on the loopback address, so the machine running a
program is the only machine that can watch it. **Watching should not be
the act that puts a program on a network**, so opening that door is a
thing somebody types rather than a default.

`--listen=all` opens it, and the viewer then prints every address
another computer could use — because telling somebody "open the viewer"
is useless without the number, and looking it up is a detour nobody
should have to take. It also says plainly what has been opened: the port
is reachable by anything that can reach the machine, and everything it
serves is a read.

Nothing is needed on the other computer. The page asks for `/map`,
`/events` and its own two files by relative path and never names a host,
so it works from anywhere the socket does. There is nothing to install
and nothing to configure — it is a browser.

### One command, still two processes

`--view` has the watched program start the viewer as a child and stop it
on the way out. Watching is one command; the server is still not inside
the engine, because a program with a thread, a socket and clients is
doing exactly what *never waits for a reader, never learns one is there*
refuses.

### What is not verified here

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

The workbench ([801](../801-browser-workbench.md)) is a canvas for
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
- [801 — The workbench in the browser](../801-browser-workbench.md), which
  it shares nothing with, deliberately
- [703 — The map dump](703-map-dump.md), where the graph
  comes from
- [701 — Buffer growth reporting](701-buffer-growth-reporting.md),
  which is the thing most worth drawing
