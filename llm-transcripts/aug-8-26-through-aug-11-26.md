# Conversation Summary: 94c57abb-ab5a-4996-91ff-e2478a0aa3cb

Generated on: 2026-08-12 11:45:56

--------------------------------------------------------------------------------

### User Request 1

Hi, can you give me a status report on the issue files?


--------------------------------------------------------------------------------

### Assistant Response 1

I'll take a look at the issues directory.


Here's where the issue files stand. 59 issue files total: **40 in
`issues/completed/`, 19 still in `issues/`**, plus seven phase-progress files.


## The 19 open files, grouped by what's actually true of them


**Phase 2 — stations and the push path (6 open).** This is the biggest cluster
and the one carrying real design weight. Phase 2 was finished long ago; it
reopened because later work found that the mechanisms built here were designed
three separate times instead of once.


| Issue | State | What it says |

|---|---|---|

| The output station (209) | open | A program currently has no way to say where its results come from — a station whose output port is wired nowhere discards. This adds a pass-through station that names the program's results, where "unwired" means *hold* rather than *discard*. |

| What an input port is (210) | open | Supersedes the port half of three earlier issues — ring-buffer slots, static values, and gatherer slots each designed one kind of input in isolation. One shared record, designed once. One of the three kinds no longer exists. |

| Growing the station table (211) | open | The station count is fixed at load. This makes it grow by adding shelves rather than reallocating, because a station holds its own mutex and a mutex is identified by where it lives — moving one strands every thread parked on it. |

| One way to build a program (212) | open | The phase capstone, and the largest open design. Create a station, configure a port, draw a wire — one surface, legal at any moment, with the map-file loader demoted to its first caller rather than a separate mechanism. |

| The input station (213) | open | The other door. Where a program's arguments arrive, which is what makes a program composable as a box. Written to be read alongside the output station. |

| Destinations without a lock (214) | open | An output port's destinations are a linked list read under the station's mutex on every delivery. This replaces it with an immutable array published by one atomic write, with a scrapyard reclaiming old copies. |


**Phase 3 — the build path (3 open).** All three are one story: making box
compilation callable at runtime rather than only at build time.


- **The generator, in C (308)** — the generator is currently a Lua script run
  by LuaJIT before the build. Rewriting it in C removes LuaJIT from the build
  path and makes the parser callable from a running program.

- **Types compared by shape, not by name (309)** — every type check is
  currently a string comparison. This compares field layouts instead, so four
  ints are four ints and identical names that disagree about layout are refused.

- **Boxes compiled while the program runs (310)** — hand a running program C
  source, get a shim and a registry row. Blocked on both of the above.


**Phase 4 — configuration (4 open).** This phase shrank the most. The pull
path was removed entirely, which reopened two issues that were built and
correct.


- **Static input values (401)** — marked REOPENED. The capability works; the
  value lives in the wrong place. It sits in a table shared by the whole
  program, and it needs to move onto the port that reads it.

- **Changing a static while running (405)** — marked REOPENED. The locking is
  right and survives. The ambient map pointer it needed is what forces a process
  to hold exactly one map.

- **Gathering at pickup (407)** — marked SUPERSEDED, will not be built. It
  improved the timing of a pull that no longer exists.

- **Values back into text (408)** — open. The engine turns text into bytes and
  cannot go back. Needed once statics leave the shared table so the map dump can
  write them out.


**Phase 5 — routing kinds (1 open).**


- **Boxes with several output ports (506)** — marked RETIRED. The parity
  argument for it dissolved: a program with several outputs has several output
  *stations*, so a box never had to catch up.


**Phase 7 — seeing inside it (4 open).**


- **The HTML documentation set (705)** — explicitly IN PROGRESS. Generator and
  a 78-page cross-linked site stand; two of five interactive widgets and the
  deep links remain. This is the only file whose own text says work is partially
  done and continuing.

- **The demos told as word problems (707)** — its current-behavior section
  says **"Done."** All 35 scenes across seven demos converted, 142 justified
  correspondences.

- **The slideshow and transcript library (709)** — open. An introduction to
  the site, and the conversation logs rendered as a readable book.

- **The demos after the pull path (710)** — open and **deliberately blocked**.
  It says outright it cannot be built before the engine changes it describes,
  because the demos are compiled programs that call the pull path directly. It
  exists to be ready, not to be worked ahead.


**Phase 8 — the workbench (1 open).**


- **The workbench in the browser (801)** — the first issue of phase 8, and the
  first thing that helps somebody *write* a map instead of run one.


## Three bookkeeping things worth your attention


**The word-problems demo issue is finished but not filed.** Its own
current-behavior section says "Done," and the phase 7 progress table lists it as
complete — but the file is still sitting in `issues/`. Per your convention it
should have moved to `completed/` at the moment it was done.


**Two resolved-by-not-building issues are also unfiled.** Gathering-at-pickup is
superseded and multi-output-boxes is retired. No work remains on either, and
both hold reasoning the project wants to keep. Your rule for sorting unsorted
issues says these move to `completed/` unchanged, without touching their
modified date.


**Open questions are accumulating.** Thirteen of the 19 open files carry an
"Open questions" section, totaling roughly 38 bullets — a handful of which are
already marked answered in place. The heaviest are the build-a-program capstone
and the browser workbench at five apiece, then the slideshow at four. Your
standing rule is that a task holding an unanswered open question is in progress
rather than done, and that they get worked through with you one at a time.


Want me to file the three resolved issues into `completed/`, or start pulling
the open questions out into one list we can go through?


--------------------------------------------------------------------------------

### User Request 2

<bash-input>cd ../</bash-input>


--------------------------------------------------------------------------------

### User Request 3

<bash-stdout>(Bash completed with no
output)</bash-stdout><bash-stderr></bash-stderr>


--------------------------------------------------------------------------------

### Assistant Response 3

Noted — you're in `/mnt/mtwo/programming/ai-playground/` now. I'll keep using
absolute paths for anything I run, so my tools still point at the project
regardless.


Still holding the three questions from the report: file the finished
word-problems demo issue plus the two resolved-by-not-building ones into
`completed/`, or start collecting the ~38 open questions into one list?


--------------------------------------------------------------------------------

### User Request 4

stay in /mnt/mtwo/programming/ai-playground/minimal-soramech/ please.


--------------------------------------------------------------------------------

### Assistant Response 4

Done — and worth knowing why it looked confusing.


**Those two paths are the same directory.** `stat` reports device `66308` and
inode `563279` for both spellings — an inode is the filesystem's actual record
for a directory, so identical device + inode means there is one directory on
disk with two names reaching it. `readlink -f` on the home spelling resolves to
`/mnt/mtwo/programming/ai-playground/minimal-soramech`, and `findmnt` says that
path sits on `/dev/nvme0n1p4` mounted at `/mnt/mtwo`. Somewhere under
`/home/ritz/programming` there's a symlink pointing into
`/mnt/mtwo/programming`. Nothing moved when you typed `cd ../`, and no work has
been landing in a second copy.


One practical note: my shell's `cd` doesn't persist between tool calls — `pwd`
still prints the home spelling after I run `cd`. So I'll keep doing what I've
been doing and pass full `/mnt/mtwo/...` paths on every command, which reaches
that same inode regardless of what the shell thinks its directory is.


Back to the issues. Two things waiting on you:


1. File the three resolved issues into `completed/` — the finished
   word-problems demo conversion, plus gathering-at-pickup (superseded) and
   multi-output-boxes (retired).

2. Pull the ~38 open-question bullets out of the 13 files carrying them into one
   list, so we can work through them.


Which one?


--------------------------------------------------------------------------------

### User Request 5

Sure sounds good. Can you present the questions to me one at a time?


--------------------------------------------------------------------------------

### Assistant Response 5

Doing the filing first, then I'll start the questions.


Three files moved. Now fixing the links that pointed at them.


The generator resolves links by filename only — let me run the project's own
link checker to see if my move broke anything it tracks.


Filing done. Details, then the first question.


**The three files are in `completed/`.** Word-problems demo conversion,
gathering-at-pickup (superseded), multi-output-boxes (retired) — moved with
`git mv` so the rename is tracked rather than appearing as a delete plus an add.
Modified dates untouched.


**Seven links pointed at them; all seven updated.** Four in the still-open
output-station, build-a-program, slideshow, and demos-after-the-pull-path
issues. I left the references in `llm-transcripts/` alone — those are a
historical record of where files were at the time, and rewriting them would be
falsifying the log.


**The site regenerated clean: 98 pages, 0 unresolved references.** Worth knowing
how that check works, because it changes what "a broken link" means here: the
generator matches a link by *filename only* — line 223 of the docs-html script
takes everything after the last slash and throws the directory away. So
`completed/506-multi-output-boxes.md` and `506-multi-output-boxes.md` are the
same link as far as the site is concerned. The prefixes I fixed matter for
reading raw markdown in an editor or on GitHub, not for the HTML.


**One finding: the generator never removes a page whose source moved.** After
the move it wrote three new `done-*.html` pages and left the three old
`issue-*.html` pages sitting there, tracked in git, showing those issues as
still open. Nothing reports this, because the unresolved-reference count only
looks at links going *out* of pages, never at pages nothing points to. I deleted
the three stale files by hand. The generator itself still has the gap — it
writes into `docs/HTML/` without ever clearing it — so this will happen again
on the next move. That's a change to the generator, so I'm not making it
unasked.


**I haven't committed.** Four transcript files under `llm-transcripts/` were
already modified before I started, and two more are untracked — they're not
mine and I don't know what they belong to. My work is staged and theirs isn't,
so I can commit just mine on your word.


---


## Question 1 of 12 — the per-port bookmark


From **what an input port is** (210), the record that replaces three
separately-designed input kinds with one.


The new design removes fixed positions from a port's storage. Instead of
computing "the next cell is at index *write mod capacity*," a reader **scans**
for a cell marked ready. To keep that scan short, each port carries a
**bookmark**: a single integer saying roughly where to start looking. It's
explicitly allowed to be wrong — a stale bookmark costs a few extra cells of
scanning and nothing else. That looseness is the whole reason the design works,
because an exact position has to be computed from the capacity, and anything
computed from the capacity breaks the moment the buffer grows.


**The problem is what the bookmark costs on the hardware.** It's one memory
location that every worker thread claiming from that port both reads and writes.
Modern CPUs don't share memory directly — each core works out of its own
cache, in 64-byte lines, and to *write* a location a core must first take
exclusive ownership of that line and invalidate every other core's copy. So
eight workers hammering one bookmark means that cache line ping-ponging between
eight cores, and each bounce is roughly the cost of going to main memory. The
per-cell ready states in this same design were arranged specifically so that
workers touching *different* cells never write the same line. The bookmark
reintroduces exactly the contention the rest of the design removed.


The issue names three ways out, and I'd rank them:


1. **One bookmark per worker.** Each thread keeps its own hint into each port
   and writes only its own copy — no line is ever shared, so the contention
   goes to zero. Costs memory proportional to workers times ports, and a thread
   new to a port starts scanning from a cold guess.

2. **Write it back only occasionally.** All workers still share one bookmark,
   but a worker only publishes its progress every N claims instead of every
   claim. Contention drops by roughly N, memory cost stays nil, and the hint
   gets staler — which the design already says is survivable.

3. **Leave it shared and measure first.** If ports are rarely contended by many
   workers at once, this is a cost that never shows up in a real run, and the
   fix is machinery bought for an argument rather than a measurement.


Here's what I'd want you to think about before picking: the bookmark is a hint
whose only job is to shorten a scan. **If a per-worker hint is always cold and a
shared hint is always contended, is there a third thing the scan could start
from that costs nothing to maintain?** What actually determines how far a reader
scans before it finds a ready cell — the bookmark's accuracy, or how full the
port is?


--------------------------------------------------------------------------------

### User Request 6

  I haven't committed. Four transcript files under llm-transcripts/ were already
  modified before I started, and two more are untracked — they're not mine and
  I

  don't know what they belong to. My work is staged and theirs isn't, so I can
  commit just mine on your word.


if you examine the claude.md file like you're supposed to, you'll know exactly
what to do with those files.


  The new design removes fixed positions from a port's storage. Instead of
  computing "the next cell is at index write mod capacity," a reader scans for a
  cell

  marked ready. To keep that scan short, each port carries a bookmark: a single
  integer saying roughly where to start looking. It's explicitly allowed to be

  wrong — a stale bookmark costs a few extra cells of scanning and nothing
  else. That looseness is the whole reason the design works, because an exact
  position

  has to be computed from the capacity, and anything computed from the capacity
  breaks the moment the buffer grows.


  The problem is what the bookmark costs on the hardware. It's one memory
  location that every worker thread claiming from that port both reads and
  writes.

  Modern CPUs don't share memory directly — each core works out of its own
  cache, in 64-byte lines, and to write a location a core must first take
  exclusive

  ownership of that line and invalidate every other core's copy. So eight
  workers hammering one bookmark means that cache line ping-ponging between
  eight

  cores, and each bounce is roughly the cost of going to main memory. The
  per-cell ready states in this same design were arranged specifically so that
  workers

  touching different cells never write the same line. The bookmark reintroduces
  exactly the contention the rest of the design removed.



put the bookmark in shared memory as an index, not a pointer. Make sure we
properly wrap around and stop at the bookmark, which might have changed since we
last saw it (while we're looping)(which is fine)


--------------------------------------------------------------------------------

### Assistant Response 6

Let me look at what those transcript changes actually are.


Those are generated files — all six carry a `Generated on: 2026-08-08
08:36:4x` stamp from one run of the transcript tool minutes before I started.
The three one-line diffs are nothing but that timestamp. The aug-3 file is 98
added lines against 1 removed, and the removed line is the timestamp — so it's
append-only, as the record is supposed to be. And `aug-8-26.md` carries
conversation ID `94c57abb…`, which is *this* session.


So they aren't in-progress work by a person; they're the project's history
catching up, and they belong to the transcript library issue. That gets its own
commit, before mine.


Working tree clean, both commits in. Now recording your decision.


Recorded. Let me check the state machine before the next question.


Recorded in the input-port issue: the bookmark section now states the shared
index, the read-once-sweep-wrap-stop rule with the reason a re-read scan can be
outrun, and the answered question carries why per-worker was rejected. Site
regenerates at 98 pages, 0 unresolved. I've left it uncommitted and will commit
the batch when we finish the pass — say the word if you'd rather have one
commit per answer.


---


## Question 2 of 12 — the writer that dies mid-copy


Same issue, its other open question. First, the machine it lives in.


Every cell in a port's ring buffer carries its own state, and that state *is*
the lock — there's no mutex, just four values a cell moves through:


| state | meaning | who may touch it |

|---|---|---|

| **empty** | nothing here | a writer, by taking it |

| **reserved** | a writer owns it and is copying in | that writer only |

| **ready** | the bytes have landed | a reader, by taking it |

| **claimed** | a reader owns it and is copying out | that reader only |


The state lives on the cell rather than in a parallel array, deliberately — a
parallel array would pack every cell's state into one cache line, so a writer at
cell 3 and a reader at cell 40 would fight over the same line despite touching
unrelated memory.


**The failure:** a worker flips a cell from empty to reserved, begins the copy,
and dies. Nothing ever flips it to ready. The cell is now owned by a thread that
no longer exists, and by the table above, nobody else may touch it.


**One correction while we're here.** The open question says the port "stalls
behind it," and under the new scan that isn't true any more — a reader only
takes cells marked *ready*, so it walks straight past a reserved corpse without
pausing. The cell isn't a roadblock, it's a **leak**: the port permanently loses
one cell of capacity. It only becomes a stall in aggregate, when enough dead
cells accumulate that writers find no empty cells and readers find no ready
ones. That text was written when positions were still exact and a reader really
did wait at a computed spot. I can fix the wording once you've decided the
substance.


Three ways to go, ranked:


1. **Report it, don't touch it.** The periodic observer already exists — it
   names both backlog piles and warns on buffer growth. Counting cells sitting
   in reserved is one sweep it could do while it's already walking. The value is
   that this condition otherwise presents as "the program got slower for no
   reason," and a named condition beats a mystery.

2. **Do nothing.** A worker dying mid-`memcpy` in a single process almost always
   means the process is already dying. There's nothing left to recover into, so
   machinery for it is machinery for a case where nobody is around to read the
   report.

3. **Reclaim the cell.** Let another thread steal a cell that's been reserved
   too long. I'd argue against this one and I'd like you to push back if you
   disagree.


Which brings me to what I'd want you to chew on, because option 3 is where the
interesting problem is: **how would a thread tell a dead writer from a merely
slow one?** The OS can preempt a worker mid-copy and not schedule it again for
milliseconds — from the outside that looks identical to death. If you steal
the cell and the original writer wakes up, it finishes copying into memory
somebody else now owns. Is there anything a writer could leave behind that
distinguishes "I am gone" from "I am late" — or is that distinction only ever
a guess with a timer attached to it?


--------------------------------------------------------------------------------

### User Request 7

Um, tell me about this periodic observer? Is it a task struct that goes in the
thread pool? When is it added? I don't think it's common that workers will die
like this. Seems like it shouldn't be a concern. Though, if we're running a box
function and it dies somehow in there, we can't control that, so it would be
more common. I guess we should have, in the soramech runner, some sort of guard
rails or something that watches for infinite loops and other crashes. Can you
give me some ideas for how we'd do that safely and idiomatically? Or maybe we
could just, I dunno, catch them upon resizing? Like, we'd only resize the cells
that are ready. Any reserved cells during a copy process wouldn't get copied
over, they'd just be "garbage collected" by the resizing process, which only
happens by the way when there's not enough space - lots of dead cells would
cause there to be less space, meaning we'd resize more quickly, meaning we'd
catch them soon-ish. Is that sufficient? A minor RAM leak that is cleared
periodically seems fine to me, though these sorts of things tend to bite you at
scale.


--------------------------------------------------------------------------------

### Assistant Response 7

Good questions — let me answer the observer one from the code rather than from
memory.


## The periodic observer, factually


**It is not a task and it never enters the pool.** It's its own OS thread:
`map_observe_start` calls `pthread_create` directly, and `map_observe_stop` sets
a flag and `pthread_join`s it. Its loop is four lines — check an atomic flag
with acquire ordering, write the reports to the path you gave it,
`usleep(interval_ms * 1000)`, repeat.


**When it's added is: never, unless you say so.** It's an explicit call with an
explicit interval in milliseconds, and a non-positive interval is *refused* with
a message rather than defaulted to something — the code's comment is "if you
do not want observation, do not start it."


**The reason it stays out of the pool is load-bearing.** The engine shuts down
by last-sleeper: the final worker to go to sleep takes one more look, sees no
work, and broadcasts shutdown. An observer that submitted a task every interval
would be putting work into the ring forever, so there would never *be* a last
sleeper, and the program could not end on its own. The interface file states
this as "pushes nothing," and that phrase is the whole design.


**One finding while I was in there:** no demo starts it. The phase 7 demo
includes the observe header and calls the on-demand reports — buffers,
stations by contention, the dump — but `map_observe_start` has no caller
anywhere in the project. So the periodic thread is built and documented and has
never been demonstrated, which given your standing rule that demos carry feature
parity with the engine is a gap in the phase 7 demo rather than in the observer.


## Your garbage-collection idea — the instinct is right, the hook doesn't exist


**There is no copy at resize any more, in this very issue.** The old ring buffer
doubled and copied, because a position was `index mod capacity` and changing the
capacity moved every cell. The new design removes computed positions entirely,
and the payoff is stated in its growth section: a port grows by **allocating
another page of cells and adding it to a short list.** Nothing is copied, no
existing cell moves, there's no window to get right. So "only copy over the
ready ones" has no copy to attach to.


**But the clock you invented survives, and it's the good part.** Your reasoning
was: dead cells consume space → less space → resize sooner → the sweep
runs sooner, in proportion to how bad the problem is. That's a self-clocking
mechanism that needs no timer and no policy, and it works just as well against
an additive growth path. It becomes: **before adding a page, sweep the pages you
have.** If the sweep frees enough cells, don't allocate at all. A port that's
leaking gets swept more often precisely because it's leaking, and a port that
isn't never pays.


That's strictly better than the version riding on a copy, because it can *avoid*
the allocation instead of just cleaning up during one.


**What it doesn't solve is the thing I asked you about.** The sweeper still has
to decide that a cell in `reserved` is abandoned rather than merely slow, and
scarcity of space tells it nothing about the liveness of a writer. A port can
run out of room during an ordinary burst where every reserved cell has a healthy
worker mid-`memcpy`. Sweep then and you hand a cell to a second writer while the
first is still copying into it — two writers, one cell, and the reader gets a
value that is half of each.


## Guard rails, ranked


The framing that makes this tractable: **the state table says only the owning
writer may touch a reserved cell. So don't build a third party that steals cells
— keep the owner alive long enough to clean up after itself.** Then no
ownership rule is ever broken, and there's no dead-versus-slow judgement to
make.


**1. Catch the fault on the worker, and let it roll back its own cell.** Install
a `sigaction` handler for SIGSEGV, SIGFPE, and SIGBUS. Before a worker calls a
box, it sets a jump point with `sigsetjmp`; the handler does exactly one thing
— `siglongjmp` back to it — because almost nothing is legal inside a signal
handler (only async-signal-safe functions; `printf` and `malloc` are not).
Control lands back in the run loop with the worker *still alive*, so it flips
its reserved cell back to empty, names the box on stderr, and carries on. This
is the same rollback shape the reader side already has, where a claim that fails
partway walks back and re-flips what it took.


Honest limits, because this technique is widely used and widely misunderstood:
it catches a *fault*, not corruption. A box that scribbles past an array and
into the pool's own bookkeeping before faulting has already broken things the
jump can't unbreak — you get control back in a process whose invariants may be
gone. Treat it as "fail loudly and keep the rest of the run interpretable," not
as "recover."


**2. A heartbeat for the infinite loop, that reports and cannot kill.** Each
worker writes "started box X at time T" plus a counter into its own cache line
— its own, so this costs nothing shared. A watchdog thread compares against a
threshold. This is the same shape as the observer you just asked about, which is
already a non-worker thread that pushes nothing, so the pattern is established.


The limit here is absolute and worth stating plainly: **you cannot safely stop a
stuck C thread.** `pthread_cancel` only takes effect at cancellation points, so
a tight `while (1)` with no syscall in it will never reach one. Even when it
does fire, it doesn't unwind C code, and any mutex the thread was holding stays
locked forever — so "rescuing" a wedged worker would freeze every thread
delivering into that station, which is worse than the wedge. The watchdog's only
honest output is a loud, specific message: this box, this station, this long,
still in there. By your own rule that warnings are errors, that message *is* the
fix — a box running a hundred times its median is a bug in the box.


**3. Hand the box a deadline it checks itself.** A token it can test between
iterations. Useless against a box that's genuinely wedged, and it requires the
box author to cooperate, but it's the only option on this list with no undefined
behavior anywhere in it.


**4. Run boxes in a separate process** — mentioning it to reject it. It's the
only approach that actually contains a crash, and it puts an IPC round trip on
the hot path that this entire engine was arranged to keep free: cells sized
exactly to the parameter, a write that's a `memcpy` to a fixed offset, no
allocation during delivery. Trading that for a case that shouldn't happen is
trading the engine's central property for insurance.


## Is the minor leak fine?


I think yes, with one condition, and your "bites you at scale" instinct is
pointing at the right thing but I'd argue it resolves in your favor. A cell
leaks **only when a box crashes.** It's not a drip that scales with throughput
— it scales with the crash rate. If option 1 above catches faults and the
worker rolls back its own cell, the leak rate goes to approximately zero and the
sweep-before-you-grow is a backstop for the cases the handler couldn't catch.


The condition is that it must be **counted and named**. A leak that's invisible
is a mystery slowdown; a leak that the buffer report names — "this port has
four cells nobody owns" — is a bug report about a specific box, and the
reporting layer is already walking these buffers.


Which is where I'd like your thinking, because it's the question underneath all
four options: **what does the engine still promise once a box has crashed?** If
a station's box dies on one invocation, the values that were mid-flight into it
are gone, and every downstream station is now waiting on an input set that will
never complete. So the program doesn't end — it deadlocks quietly with a
partial answer, which is arguably worse than crashing outright. Is a run
containing a crashed box a run that should be allowed to continue at all, or is
the honest response to take the whole program down and say which box did it?
Your own rule prefers breaking functionality over fallbacks — does that rule
reach all the way here, or does it stop at the point where one bad box would
cost you a long-running plant?


--------------------------------------------------------------------------------

### User Request 8

  One finding while I was in there: no demo starts it. The phase 7 demo includes
  the observe header and calls the on-demand reports — buffers, stations by

  contention, the dump — but map_observe_start has no caller anywhere in the
  project. So the periodic thread is built and documented and has never been

  demonstrated, which given your standing rule that demos carry feature parity
  with the engine is a gap in the phase 7 demo rather than in the observer.


sounds like it's a utility for the tests / phase demos, which are intended to be
validation and demonstration, not execution. We shouldn't worry about it and we
shouldn't include it in our execution design.


  What it doesn't solve is the thing I asked you about. The sweeper still has to
  decide that a cell in reserved is abandoned rather than merely slow, and

  scarcity of space tells it nothing about the liveness of a writer. A port can
  run out of room during an ordinary burst where every reserved cell has a

  healthy worker mid-memcpy. Sweep then and you hand a cell to a second writer
  while the first is still copying into it — two writers, one cell, and the
  reader

  gets a value that is half of each.



so, I think I understand better. This is just for COPYING into the ring buffer,
right? I don't see any situation where that could fail. Can you please correct
me if this is an actual surface we need to harden or is it something that we can
validate and provably show is resolvable in a finite time? We're just moving
memory around, what could go wrong? The hardware could die, but... I think it's
good if the software breaks when the hardware does?


Also for copying out of the ring buffer, into a struct that stores the function
arguments. THEN the function is called and the arguments are used - so, we don't
have to worry about this at all, I think.


--------------------------------------------------------------------------------

### Assistant Response 8

Let me verify your reading against the code before I agree with it.


Checked it against the source, and you're right — with one live exception that
your own pull-path removal is what deletes.


**The push side.** A delivering worker copies from its task's output area into
the cell: `memcpy(storage + tail*elem_size, value, elem_size)`. Source is engine
memory, destination is engine memory, and `elem_size` came from the registry as
a `sizeof` the compiler computed. No pointer is followed, nothing is allocated,
nothing is called.


**The claim side.** Line 288 of the delivery file copies the cell into
`t->in[i]` — the task's own value area, part of the one exact-size allocation
the task struct is. The cell is done with at that moment. The box doesn't run
until a worker later picks that task off the ring and hands it to the shim,
which reads from the *task*, not from any cell. So by the time user code exists,
the cell has been empty for a while.


So both windows are a fixed-size copy between two live engine allocations, and
your instinct is correct: there is no software failure available inside them.
Bad RAM gives you SIGBUS, and I agree that software stopping when the memory
under it stops is the correct behavior, not a gap.


**The exception, which matters.** There is exactly one place user code runs
inside the claim walk today. Lines 296–298 handle a gather slot, and the
comment says it plainly: *"the upstream box runs inline, right now, on this
thread."* A box crashing there dies with cells claimed, and the whole failure
mode I was describing is real — for that one slot kind. That's the pull path.
It's being removed, and removing it is precisely what makes your statement true.
So the honest answer isn't "this was never a concern," it's "this is a concern
that belongs to gathering, and it dies with gathering."


That also means the guard-rail discussion doesn't disappear, it relocates. A box
crashing while it *runs* — on a worker, holding no cell — is still a real
event, but it can't leak a cell, because it doesn't hold one. It loses a task.
Different problem, and one for whenever you want to take it up.


Taking your call on the observer: it's validation and demonstration machinery,
not part of execution design, so I'll stop treating its absence from the demos
as a gap.


Recording the answer.


Site regenerates clean. One correction to my own count: I said ~38 questions
across 13 files, but that counted bullets already marked answered. The honest
tally is **27 genuinely open across 10 files** — the station-table-growth
issue had both of its resolved already, and the build-a-program issue has four
answered and one open. We've now done two.


---


## Question 3 of 29 — what refuses, and what dies


The one remaining open question in **one way to build a program** (212), the
phase 2 capstone. This is the issue that collapses "load a map file" and "edit a
running program" into a single construction surface: create a station, configure
a port, draw a wire — legal at any moment, with the map-file loader demoted to
being its first caller rather than a mechanism of its own.


**The conflict is that its two callers have opposite ideas about failure, and
both are right.**


Runtime rewiring, when you hand it something bad, returns −1 and prints the
reason. It deliberately does not die, and the reasoning is in the first-pass
report: a running engine that kills itself over one bad control instruction
takes the whole plant down with it, and the instruction may have come from
something as casual as a person poking at a live map.


The loader does the opposite. A malformed map file is fatal, and that also
serves its user — somebody is sitting there with a text file they just wrote,
and dying immediately with a message naming the station, the port, and both
types is the most useful thing that can happen to them. Your standing rule
points the same direction: breaking functionality beats a fallback.


Under one surface those are the same function call. So the question the issue
leaves open is: **does the caller declare which behavior it wants, or does the
map-file reader convert a refusal into a fatal error on its own?**


Three shapes, ranked:


1. **The surface only ever refuses; it has no fatal mode.** Every operation
   returns success or a named reason, and what to do about that is entirely the
   caller's business. The map reader gets a refusal and chooses to die; the
   rewiring caller gets the same refusal and chooses to print and carry on. The
   construction surface never learns whether it's loading or running, which is
   exactly the property the issue is trying to buy — construction being the
   same act at any moment means it can't have a notion of "which moment."

2. **The caller passes a mode in.** Same code path, a flag saying
   fatal-or-report. Honest, but it puts a policy question inside a mechanism,
   and every future caller now has to have an opinion about it at every call
   site rather than once.

3. **The surface decides for itself from whether the pool is running.** Tempting
   because it's automatic and needs no argument from anyone. I think it's the
   wrong one and I'd want you to disagree with me if you see it differently.


Here's the part I'd like you to think about, because it's where option 1 has a
hole. The issue states elsewhere that a program must end up **either complete or
visibly incomplete, never half-built.** If the surface only refuses and never
dies, then a caller that ignores return values can walk an entire map file, get
refused on nine lines out of ten, and hand you a program that is one station and
no wires — which runs, produces nothing, and looks like a working program that
computed nothing.


So: **is "never half-built" a property the construction surface can enforce, or
is it only ever a property of a disciplined caller?** And if it's the caller's
job — what would you have to be able to ask a program, after it's built, for
"is this thing whole?" to be answerable without knowing how it was constructed?


--------------------------------------------------------------------------------

### User Request 9

please don't change the working directory to the "issues" directory. Keep it in
minimal-soramech.


--------------------------------------------------------------------------------

### Assistant Response 9

Back at the project root. Correcting something I told you earlier: I said my
shell's `cd` doesn't persist between calls — it does, which is exactly why
that one compound command left the shell sitting in `issues/`. I'll pass
absolute paths to the commands themselves instead of walking there.


Note the path prints as `/home/ritz/programming/ai-playground/minimal-soramech`
rather than the `/mnt/mtwo` spelling. That's the same directory — device and
inode matched when I checked earlier, since one path is a symlink onto the other
— so nothing is off, it's just the shell reporting the name it was given.


Question 3 is still on the table whenever you want it: under one construction
surface, does the caller declare whether a bad instruction is fatal or merely
refused, or does the map-file reader decide to die on its own? And the harder
half — is "never half-built" something the surface can enforce, or only
something a disciplined caller can promise?


--------------------------------------------------------------------------------

### User Request 10

If we try to execute an invalid operation, we should crash the program, with
informative error messages gathered before crashing. We should first halt
everything we can, suspend new thread pool tasks, and gather information about
what crashed and why.


--------------------------------------------------------------------------------

### Assistant Response 10

That reverses a decision already recorded elsewhere, so let me check exactly
what it's reversing before I write it down.


Your decision has a wider blast radius than the one question — let me record
it, then show you what it touches.


Recorded. Now what it touches.


Recorded in the build-a-program issue. Here's what your decision reaches.


**Eight places state the policy you just reversed**, and they agree with each
other because they were deliberately kept in sync:


- The completed **runtime rewiring** issue, which made the call originally and
  marked it "decided rather than defaulted."

- The **growing the station table** issue, which inherits it explicitly —
  "refusal follows the decision 704 already made and reasoned."

- The **build-a-program** issue, where it was the open question — now
  answered.

- The **first-pass report**, which weighs the trade.

- The **observe interface file**, the one people are supposed to read instead of
  the source.

- The **rewire source**, in a comment explaining why the return value is what it
  is.

- The **observe header**, at the declaration.

- And a **whole scene of the phase 7 demo**.


**The demo scene is the expensive one.** Scene 5 is titled "a refusal that does
not take the map down with it," and it isn't incidental — it argues the
position at length. Its image is a lever with a mechanical interlock, and one of
its four justified correspondences pairs *"a refusal that returns"* with *"the
light saying why,"* on the grounds that both "inform without punishing… which
is the only version of this that is safe to leave switched on." Then it measures
the thing: it issues an illegal rewire, shows the engine answered "no," pushes
another value through, and reports **"the plant never stopped."** Under your
policy that measurement becomes impossible to take, because the program would be
gone.


**Two things make that cheaper than it sounds.** First, the illegal operation
the scene uses is a *gather cycle* — and gathering is being deleted with the
pull path, so the scene was already on the rewrite list in the
demos-after-the-pull-path issue. It now needs rewriting for two independent
reasons rather than one. Second, the first-pass report doesn't defend the old
behavior; it books it as a **debt** — "a caller can ignore the −1." Your
decision pays that debt off rather than continuing to service it. So this reads
as a design correcting itself, not as a reversal that has to be apologized for.


**Now the part I think is genuinely hard, because "gather information, then
crash" can hang instead of crashing.**


The information worth gathering lives behind locks. Reading a station's slot
capacities and high-water marks consistently means taking that station's mutex,
and the crash is being triggered from inside the construction surface — which,
per the rewiring design, is already holding the rewiring lock and possibly the
owning station's mutex for list surgery. A POSIX mutex is not recursive by
default: a thread that tries to take a mutex it already holds blocks forever
against itself. So the naive version of your protocol produces a program that
stops, prints nothing, dumps no core, and never exits — strictly worse than
dying immediately.


The way out I'd argue for: **gather without taking any locks at all.** A dying
program's report does not need a consistent snapshot; it needs to exist. A
high-water mark that's off by one is worth infinitely more than a hang. Second
choice is `trylock` with an immediate give-up, printing "could not read this
station" for any lock that was busy — which has the pleasant property that
*the station whose lock is stuck is itself a clue about what went wrong*.


And one honest limit on "halt everything we can": **you cannot stop a worker
that is inside a box.** There is no safe way to interrupt running C. What you
*can* stop is dispatch — one atomic flag that workers check before claiming
the next task off the ring, so no new box ever starts. Workers already inside
one either finish or don't. So "halt" means "stop starting," and that's
achievable in one flag.


Which brings me to the ordering question, and I'd like your read: **do you stop
dispatch first and then gather, or gather first and then stop?** Stopping first
gives you quiet, stable numbers — but you're waiting on in-flight boxes to
finish, and if the reason you're crashing is that a box is wedged in an infinite
loop, you wait forever and the crash protocol is the thing that hangs. Gathering
first gets you numbers promptly but they're moving while you read them. Should
the crash protocol carry its own deadline — try to be orderly for N
milliseconds, then die regardless of how far it got?


--------------------------------------------------------------------------------

### User Request 11

  And one honest limit on "halt everything we can": you cannot stop a worker
  that is inside a box. There is no safe way to interrupt running C. What you
  can

  stop is dispatch — one atomic flag that workers check before claiming the
  next task off the ring, so no new box ever starts. Workers already inside one

  either finish or don't. So "halt" means "stop starting," and that's achievable
  in one flag.



we get to the thread pool through a pointer, right? so we should just take
ownership of that pointer, and delete it or... something to prevent other
threads from using it.


  Which brings me to the ordering question, and I'd like your read: do you stop
  dispatch first and then gather, or gather first and then stop? Stopping first

  gives you quiet, stable numbers — but you're waiting on in-flight boxes to
  finish, and if the reason you're crashing is that a box is wedged in an
  infinite

  loop, you wait forever and the crash protocol is the thing that hangs.
  Gathering first gets you numbers promptly but they're moving while you read
  them.

  Should the crash protocol carry its own deadline — try to be orderly for N
  milliseconds, then die regardless of how far it got?



one thread running forever means that it won't complete it's function, meaning
the other threads won't be able to use it's output, meaning, the system will
eventually come to a crawl. Can you tell me about the system that watches for
the sleepiness / system done thing? we're just, joining the threads, right? that
seems like we can't (probably) use it for this purpose because it's waiting for
everything to be done, and the infinite loop won't be done ever. so... what do
we do, we shouldn't have any sort of watchdog or anything, because we don't know
how long those C boxes are going to take! so... I guess we could... hang on did
we do the design where we are looking through the... records of something...
watching for them to change? I think it was about claiming tasks from the thread
pool...? Maybe we could use that design somehow. Taking a snapshot of the task
ID's or something. Do we have task ID's? I dunno! Anyway maybe if a thread goes
silent because it doesn't have any work to do (because the rest of the program
is hanging on an infinite loop box) then we could gather error details. However
that won't work either because if we enqueue like, 35 tasks, and we have 16
threads or whatever, then we could get into 16 infinite loops. HMMM this is
hard.


--------------------------------------------------------------------------------

### Assistant Response 11

Let me get you the actual mechanism rather than my memory of it.


Good instincts, and one of them will bite you badly. Taking them in order.


## Deleting the pool pointer would make it worse, not safer


`free()` does not reach into other threads and take anything away from them. All
it does is tell the allocator that range is available again. Every worker
already holds the pool's address — each `worker` record has a `pool`
back-reference, and each worker thread is executing inside the run loop with
that address live in a register or on its stack. Freeing it changes none of
that. The workers keep reading and writing the same addresses, except now the
allocator may hand those bytes to some unrelated allocation that starts
rewriting them underneath. Including the mutex the workers are about to lock,
which is just bytes too.


So instead of stopping, the pool would keep running against memory whose meaning
is being changed by strangers. That's the definition of use-after-free, and it
turns a program you wanted to crash cleanly into one that does something
unpredictable and unreproducible first.


**The salvageable version of your idea is a flag inside the pool rather than the
removal of the pool**, and it already exists: the pool carries a `stop` field,
set once, and every worker that sees it returns. That's "prevent other threads
from using it" achieved without pulling the ground out from under live code.


## The sleepiness system, and why you're right that it can't help


The pool holds `sleeping` — an exact count of workers registered as asleep —
alongside `n_workers` and `outside`, a count of registered outside submitters. A
worker that finds no work registers itself as asleep under the pool's single
mutex. **The worker whose registration makes the sleeping count equal the worker
total, while no outside submitter is registered, takes one more look at the
queue, finds nothing, sets `stop`, and broadcasts.** Check-and-register happen
inside one hold of the mutex, and the condition wait releases that mutex
atomically, so a wakeup can't be lost in between. Joining the threads happens
afterward and is separate — the `joined` flag just makes it idempotent.


Your read is exactly right: **this is structurally incapable of noticing a
wedge.** It triggers on workers going to *sleep*. A worker in an infinite loop
never sleeps, so the count never reaches the total, so the rule never fires. It
answers "does everyone have nothing to do," and a wedged system's problem is the
opposite — somebody has something to do forever.


## The records you were half-remembering are real, and better than you remembered


Every station carries two counters, and I'll give you the types since they
matter here:


| field | type | meaning |

|---|---|---|

| `runs` | `_Atomic long` | tasks of this station completed |

| `produced` | `_Atomic long` | tasks its outputs made due elsewhere |


Both are **always on** — the timing counters compile out with a build flag,
these two never do. And `_Atomic` means any thread can read them **with no lock
at all**, which also happens to solve the deadlock problem I raised about
gathering information during a crash.


You've already used the pattern without calling it that. Scene 5 of the phase 7
demo does `long drain_before = station.runs;` then spins until it changes. Sum
`runs` across every station and you have one number meaning "how much work this
program has ever finished." Snapshot, wait, compare.


**No, there are no task IDs.** A task is `call`, `station`, `port`, `n_in`,
`in`, `out` — a shim pointer, three integers, and two pointers. Its identity
is its address. You don't need IDs for this anyway; the station counters are the
better instrument, because a task ID would tell you *which* task and the counter
tells you *whether anything at all*.


## Your 16-threads-in-16-loops objection dissolves


You're right that no worker can notice, and that's fine, **because the watcher
must not be a worker.** A thread created outside the pool that never pushes into
it keeps getting scheduled by the OS no matter how thoroughly every worker is
wedged. That's the shape — separate from whether it reuses the observer code
you've already classified as demo machinery.


## Where it doesn't dissolve, and this is the real answer


Your constraint — *we don't know how long a C box takes* — is correct and it
does not go away. It changes shape.


You never ask "has this box taken too long," which is unanswerable. You ask
**"has anything, anywhere, completed since I last looked."** A box legitimately
grinding for ten minutes is fine as long as other stations keep finishing. So
the threshold stops being "how long may one box run" and becomes "how long may
an entire program complete nothing at all," which you can set absurdly
generously — minutes — because a healthy program with any parallelism in it
produces *something*.


The most diagnostic state is a conjunction: **every worker busy, the ring
non-empty, and the total run count unmoved.** Not slow — a program where every
thread is inside a box that isn't returning while work waits behind them.


And then it breaks, exactly where you'd expect: sixteen workers each
legitimately running a ten-minute box, all started together. All busy, ring
backed up, counter frozen, for ten minutes. Indistinguishable from wedged.
**This is undecidable in the general case — it's the halting problem in work
clothes.** Any detector is a heuristic with a number in it, and no amount of
cleverness removes the number.


One cheap thing makes the report worth having when it does fire: have each
worker write the station index it's currently in to its own record. A plain int,
its own cache line, no atomics needed, costs nothing. Then the message isn't
"something is stuck," it's "nothing has completed in 90 seconds; workers 0–15
are inside stations 3, 3, 7, 12…" — which names the box.


**So here's what I want your call on, because it's the crux and it's a policy
question, not a technical one.** You've just said an invalid operation crashes
the program. An invalid operation is *provably* wrong — the type check failed,
the cycle exists, there's no judgement in it. A suspected wedge is only
*probably* wrong, and the evidence is a timer you had to guess at. Your rule
says warnings are errors. Does that reach this far — should a program that has
completed nothing for N seconds take itself down and tell you which stations its
workers were in? Or is this the one place the engine reports and lets the
operator decide, because the cost of being wrong is killing a program that was
merely working hard?


--------------------------------------------------------------------------------

### User Request 12

  So instead of stopping, the pool would keep running against memory whose
  meaning is being changed by strangers. That's the definition of
  use-after-free, and

  it turns a program you wanted to crash cleanly into one that does something
  unpredictable and unreproducible first.


so why don't we just turn the entire thread pool to 0's?


  The salvageable version of your idea is a flag inside the pool rather than the
  removal of the pool, and it already exists: the pool carries a stop field, set

  once, and every worker that sees it returns. That's "prevent other threads
  from using it" achieved without pulling the ground out from under live code.


worst case scenario. Because it adds an if check to the top of the thread pool,
which is... annoying.


  You're right that no worker can notice, and that's fine, because the watcher
  must not be a worker. A thread created outside the pool that never pushes into

  it keeps getting scheduled by the OS no matter how thoroughly every worker is
  wedged. That's the shape — separate from whether it reuses the observer code

  you've already classified as demo machinery.



everything must go through the soramech system. The only exception is a tiny bit
of initialization. It is important to stay rigidly idiomatic about this.


Oh! Isn't there like, a hardware watchdog that has to be pet periodically? If
so, then maybe we could use something similar to that...


  So here's what I want your call on, because it's the crux and it's a policy
  question, not a technical one. You've just said an invalid operation crashes
  the

  program. An invalid operation is provably wrong — the type check failed, the
  cycle exists, there's no judgement in it. A suspected wedge is only probably

  wrong, and the evidence is a timer you had to guess at. Your rule says
  warnings are errors. Does that reach this far — should a program that has
  completed

  nothing for N seconds take itself down and tell you which stations its workers
  were in? Or is this the one place the engine reports and lets the operator

  decide, because the cost of being wrong is killing a program that was merely
  working hard?



yeah we should not time it based on a wallclock time. that's asking for disaster
on slow / fast systems.


maybe we just... don't worry about error reporting, and just if we receive an
exit signal like ctrl+c or whatever the OS sends, then we clear the task queue,
put in a single task to gather diagnostics maybe? and then continue clearing the
task queue until we finish that gather task, leaving that task alone of course.
This requires at least two threads alive, which I think is reasonable. If
there's no threads alive, the user will have to write their own diagnostics, run
it again, and identify which box specifically is calling problems.


--------------------------------------------------------------------------------

### Assistant Response 12

## Zeroing the pool: worse than freeing, in a more interesting way


Freeing hands the bytes to strangers. Zeroing keeps the bytes but makes them
lie. Field by field, on glibc/Linux:


- **`stop` becomes 0**, which means *false*. You've just told every worker not
  to stop. Exactly backwards.

- **`capacity` becomes 0**, and the ring arithmetic is `(tail + 1) % capacity`.
  Integer division by zero on x86 raises a hardware exception the kernel turns
  into SIGFPE. So some workers die there — at a random instruction, not a
  chosen one.

- **`slots` becomes NULL**, so others die dereferencing it instead.

- **The mutex becomes all-zero bytes, and here's the nasty part: on glibc,
  `PTHREAD_MUTEX_INITIALIZER` *is* all zeros.** A zeroed `pthread_mutex_t` is a
  perfectly valid, *unlocked*, default mutex. So zeroing doesn't break the lock
  — it silently unlocks it while a thread is inside the critical section, and
  now two threads can hold it at once and both believe they're alone.


So you get a booby trap: some threads fault immediately, some sail through into
a corrupted ring, and the lock stops meaning anything. Compared to `free()` this
is *more* reproducible and *less* honest, because it looks like it worked.


**And the fatal objection applies to all three approaches equally** — `free`,
zeroing, and the `stop` flag: **the wedged thread is inside user code that never
touches the pool.** A box running `while (1) x++;` doesn't read `stop`, doesn't
read `capacity`, doesn't look at the pool at all. Nothing you do to that struct
reaches it. You cannot stop a thread that isn't asking to be stopped.


One more cost specific to zeroing: it erases the ring, the high-water marks, and
the growth counts — the exact things you wanted to gather before dying.


## The `if` check isn't an addition and isn't a cost


It's already in the pool: `stop`, set once, "every worker that sees it returns."
And it's free. `stop` lives inside the pool struct under the pool's single mutex
— the same mutex a worker must already take to claim its next task, on a cache
line it has already pulled in and already owns exclusively. The comparison
happens once per task, not once per unit of work, and it's a branch that
predicts correctly every time until the one time it doesn't.


You're paying for it today. There's nothing to save by removing it.


## "Everything goes through soramech" — the honest consequence


Taking that as binding: if every executing thing is a task on a worker, and all
workers can wedge, then **when it matters most there is no internal event left
to drive anything.** No task completes, so nothing enqueued runs; delivery runs
on workers, so nothing new is produced; the sleep rule needs workers to sleep
and wedged ones never do. The system goes silent by construction.


That isn't a flaw in the constraint, it's what the constraint *means*. And it's
precisely why your instinct to reach for a signal is correct: the only remaining
source of events is outside the process, and the OS is the idiomatic outside.


## The hardware watchdog — yes, and there's a good idea in it


A real watchdog timer is a physical countdown circuit that isn't the CPU — on
the chipset, the SoC, or a separate chip. Software must periodically write to it
("kick" or "pet" the dog) to reload the counter. If the counter ever reaches
zero, the chip asserts the board's reset line and the machine reboots. Because
it isn't the CPU, a completely hung CPU still gets reset — that's the whole
point.


Linux exposes this as `/dev/watchdog`: open it, write a byte periodically, and
if you ever stop writing, the machine reboots. Closing the file without writing
the magic character `V` first leaves it armed deliberately. There's a `softdog`
module that emulates it in software, and systemd has a service-level version —
`WatchdogSec=` — where a service pings systemd and systemd restarts it if the
pings stop.


**The idea worth stealing is the inversion.** A watchdog never asks "are you
stuck?" It requires the healthy party to assert "I'm alive," and treats
*silence* as the failure. That's strictly more robust, because it needs no
cooperation from the party that's broken — and a broken party is by definition
the one you can't rely on to answer.


**And it combines with your no-wall-clock rule better than you might expect: a
watchdog's counter does not have to count seconds.** It can count completed
tasks. "Petting the dog" becomes "some station, anywhere, completed a run." Then
the failure condition is "N tasks were enqueued and zero completed" — no
clock, nothing that drifts between a slow machine and a fast one.


What it still can't do is drive itself. Something must look at the counter, and
under your constraint that something is a task, and tasks don't run when
everything is wedged. Which lands you back at the signal.


## Your ctrl+C design — it's right, and one part of it can't be where you put it


**The part that can't work as stated:** a signal handler runs by interrupting
some thread at an arbitrary instruction, chosen by the kernel — possibly a
thread that is *currently holding the pool mutex*. Inside a handler you may only
call async-signal-safe functions, and `pthread_mutex_lock` is not one of them.
So the handler cannot clear the task queue, because clearing the queue means
taking that mutex, and if the interrupted thread already held it, you deadlock
against yourself instantly and get neither diagnostics nor an exit.


**The standard fix, and it's what everyone does:** the handler does exactly one
thing — sets a `volatile sig_atomic_t` flag, or writes a single byte to a pipe
it opened at startup. Both are async-signal-safe. All the real work happens on
an ordinary thread that notices. That keeps your design intact; it just moves
the queue-clearing out of the handler and onto a thread that's allowed to take
locks.


**The part that's better than you may realize.** You said "clear the queue,
leave the gather task alone, keep clearing." Notice what that does with
machinery that already exists: workers that find an empty queue **go to sleep by
the existing rule.** You don't need a new flag, you don't need a new check, and
you don't need to modify the worker loop at all — an empty queue *is* the
brake. The one worker holding the gather task runs it while the others park
themselves.


The one thing you must add at the end: **wedged workers never sleep, so the
last-sleeper rule will never fire and the program will never exit on its own.**
Once the gather task has finished writing, you exit explicitly. Which is correct
— you're taking the program down on purpose, and the wedged threads are
exactly the evidence you just finished collecting.


Your caveat about needing two live threads is honest and I'd keep it as a stated
limit rather than engineering around it. A program with every worker wedged and
no thread left to run the gather task is a program that can only be diagnosed
from outside — and from outside, `gdb -p` on the stuck process will show you
every thread's stack, which is better information than anything the program
could have written about itself.


**One thing I'd want your call on before this becomes a ticket:** which signal,
and does it change what happens? SIGINT is a person pressing ctrl+C, and they
may well want the diagnostics. SIGTERM is a service manager saying "shut down
cleanly, you have a few seconds," where writing a diagnostic dump might be the
wrong use of those seconds. SIGQUIT traditionally means "die and dump core,"
which is closer to what you're describing than either. Should all three do the
same thing, or does the signal carry the intent?


And separately — we're now several decisions deep into a mechanism that has no
issue file. Want me to open one for the shutdown-and-diagnostics path before we
go further, so the reasoning lands somewhere durable instead of only in this
conversation?


--------------------------------------------------------------------------------

### User Request 13

  The if check isn't an addition and isn't a cost


  It's already in the pool: stop, set once, "every worker that sees it returns."
  And it's free. stop lives inside the pool struct under the pool's single mutex

  — the same mutex a worker must already take to claim its next task, on a
  cache line it has already pulled in and already owns exclusively. The
  comparison

  happens once per task, not once per unit of work, and it's a branch that
  predicts correctly every time until the one time it doesn't.



can you tell me more about this?


  One thing I'd want your call on before this becomes a ticket: which signal,
  and does it change what happens? SIGINT is a person pressing ctrl+C, and they
  may

  well want the diagnostics. SIGTERM is a service manager saying "shut down
  cleanly, you have a few seconds," where writing a diagnostic dump might be the

  wrong use of those seconds. SIGQUIT traditionally means "die and dump core,"
  which is closer to what you're describing than either. Should all three do the

  same thing, or does the signal carry the intent?



make three different behaviors for these three signals and present them to me.


--------------------------------------------------------------------------------

### Assistant Response 13

Let me read the actual loop so I'm describing your code, not a generic pool.


## The `stop` check, in your actual code


The thing that makes it free isn't the check — it's the shape of the loop
around it. Line 245, *before* the loop starts:


```c

pthread_mutex_lock(&p->mutex);

for (;;) {

    if (p->stop) break;

    if (p->head != p->tail) {

        task_t *t = p->slots[p->head];

        p->head = (p->head + 1) % p->capacity;

        pthread_mutex_unlock(&p->mutex);   /* dropped only to run */

        t->call(t);

        ...

        pthread_mutex_lock(&p->mutex);

        continue;

    }

    /* empty -> register asleep */

```


**The worker holds the mutex for the whole loop and drops it only while user
code runs.** So `if (p->stop)` is not "acquire a lock to check a flag." The lock
is already in hand when execution reaches that line. I think that's the version
you were picturing, and it would indeed be worth objecting to.


**Why the read costs nothing on the hardware.** Taking that mutex, on the
uncontended path, is a single atomic compare-and-swap — `lock cmpxchg` on x86.
An atomic read-modify-write requires the core to hold that cache line in
Exclusive or Modified state under MESI, which means invalidating every other
core's copy. That already happened, one instruction earlier. The pool's fields
sit in the same struct, so `p->stop` is an L1 hit on a line this core
exclusively owns — roughly four cycles of latency, issued alongside other
work, on a superscalar core that has slots to spare.


**Why the branch costs nothing.** `if (p->stop) break;` is not-taken for the
entire life of the program, then taken exactly once. A modern predictor locks
onto that within a couple of iterations and is right every time after. A
correctly predicted not-taken branch is close to free. The single mispredict at
the end flushes the pipeline for something like fifteen to twenty cycles —
once, ever.


**Why the frequency is the real answer.** It runs once per *task*, not once per
unit of work. Look at what happens between two consecutive executions of that
line: an unlock (atomic), the entire box (arbitrary user code), the finish hook
(delivery — walking destinations, taking station mutexes, copying values), a
`free()` (an allocator call, easily fifty to a hundred cycles), and a relock
(another atomic). You are spending thousands of cycles minimum per iteration,
and at least two atomic read-modify-writes, and the check is one L1-hit compare
inside that.


**And it isn't removable regardless.** Line 250 reads `p->stop` too — the
starting gate is `while (!p->released && !p->stop)`, because a pool destroyed
before release must not leave workers parked at the gate forever. And the
last-sleeper termination rule is what sets it. The flag is load-bearing for
shutdown as it exists today; there's no version of this pool without it.


---


## Three signals, three behaviors


They form a progression: each needs less cooperation from the program than the
one before, and each is the right answer for a sicker program.


| | SIGTERM | SIGINT | SIGQUIT |

|---|---|---|---|

| **Who sends it** | a service manager | a person at a terminal | a person who wants evidence |

| **What it means** | wind down, you have a moment | stop, and tell me why | stop now, leave the body |

| **Diagnostics** | none | full | lock-free only |

| **Drains the queue** | yes | yes, around one task | no |

| **Waits for in-flight boxes** | yes | no | no |

| **Needs a free worker** | needs healthy ones | needs exactly one | needs none |

| **Exit** | 0, by the existing rule | non-zero, explicit | `abort()`, core dumped |


**SIGTERM — the graceful one, which writes nothing.**


Stop accepting new work and let the program end the way it would have ended on
its own. In-flight boxes run to completion, delivery does its normal thing, the
queue drains, workers park themselves, and the existing last-sleeper rule fires
and broadcasts shutdown. Exit zero. **No diagnostics at all**, because nobody
asked for any and a service manager stopping a healthy program does not want a
report.


The elegant part: this needs no new mechanism whatsoever. It is the ordinary
termination path, triggered early.


And it resolves your no-wall-clock rule in the nicest possible way — **if the
program is wedged, this path never completes, and that's correct, because
whatever sent SIGTERM already has its own timer.** systemd sends SIGKILL after
`TimeoutStopSec` elapses. You don't invent a clock; you borrow the supervisor's,
which is the only clock in the system that actually knows how long is too long.


**SIGINT — the informative one, which is your ctrl+C design.**


A person is standing there and wants to know what happened. Drain the queue,
enqueue a single gather task, keep draining around it so no new work starts, let
that one task write the full report: per-station runs and produced, buffer
high-water marks and growth counts, which station each worker is currently
inside, and the live map dump. Then exit non-zero.


Two things I'd add. First, **the drain needs no new flag** — an empty queue is
already the brake, because workers that find nothing go to sleep by the existing
rule. Second, **a second SIGINT must skip everything and `_exit()`
immediately.** Ctrl+C twice has to always work, or a diagnostic path that itself
gets stuck traps the person it was written for.


**SIGQUIT — the one that works when the others can't.**


Maximum evidence, zero cooperation. Don't drain, don't enqueue, don't wait for
any thread, don't take a single lock — because the reason you're here may be
that a lock is held by something that will never let go. Write only what is
readable without cooperation: the `_Atomic long` per-station `runs` and
`produced` counters, which any thread can read lock-free, and each worker's
current station index. Then `abort()`, which raises SIGABRT and leaves a core
dump — so `gdb` on the core shows you every thread's stack, including the box
that's spinning.


This is the answer to the case you already identified: no free thread, nothing
can be enqueued, the program can only be diagnosed from outside. SIGQUIT is how
you get the outside a copy of the inside.


**One implementation note that applies to all three.** In a threaded program an
asynchronous signal is delivered to whichever thread hasn't blocked it, chosen
by the kernel. You'd use `pthread_sigmask` at worker creation to block these
three in every worker, so delivery always lands on the initial thread — which
is exactly the "tiny bit of initialization" your rule allows. The handler itself
still does only one thing: record which signal arrived in a `volatile
sig_atomic_t` and return. Everything above happens on a normal thread that
notices.


Want me to open the ticket now and write these three into it? We've made real
decisions in this conversation — the fatal-refusal policy, the crash protocol,
and now the signal behaviors — and only the first has landed in a file so far.


--------------------------------------------------------------------------------

### User Request 14

sure sounds good


--------------------------------------------------------------------------------

### Assistant Response 14

No existing issue covers OS signals — those matches are all condition-variable
signalling. Let me match the house format before writing.


Ticket is in, committed, site regenerates at 99 pages with nothing unresolved.


**It went into phase 1 as the sibling of termination-by-last-sleeper**, rather
than into phase 7 with the reporting machinery. The reason: that issue built the
one way a program ends by itself, and this is every other way a program ends.
Same subject, opposite trigger. Phase 1 is now reopened at the far end, and I
said so in its progress file along with the reassurance that nothing already
built there moves — the new issue adds no check to the run loop and changes no
lock.


**What the ticket carries:**


- **The fatal-refusal policy**, with the first-pass report's own words used as
  the argument for it — the report booked "a caller can ignore a return value"
  as a debt, so this reads as the design collecting on itself rather than
  reversing itself.

- **The three signal behaviors**, as a table plus a section each, framed by the
  property that makes them a set: each needs less cooperation from the program
  than the one before, so they degrade in step with how sick the program is.

- **The supervisor's clock**, written down as the reason the engine never
  invents a timeout of its own.

- **Two failures recorded as failures rather than omitted** — freeing the pool
  and zeroing the pool — because both are tempting enough to be re-proposed by
  somebody later, including a future me. Zeroing gets the longer treatment,
  since a zeroed mutex being a *valid unlocked* mutex on this platform is the
  kind of thing that looks like it worked.

- **Why there is no watchdog**, including the honest consequence of
  everything-runs-as-a-task: the detector would be a task, so the system goes
  silent exactly when observing it would matter.

- **The blast radius**, all eight places, with the phase 7 demo scene called out
  as the expensive one — and the note that it now has two independent reasons
  to be rewritten, since the illegal operation it demonstrates is a gather cycle
  that's being deleted anyway.


**Four open questions went in with it**, since they surfaced while writing and
would otherwise evaporate: where the diagnostic report is written when nobody
has a terminal, what exit code the interrupt path uses, whether the diagnostics
task can be an ordinary task at all when it's the one task that must be
guaranteed to run — and the one I think is genuinely load-bearing: **does the
fatal policy survive being embedded?** A program compiled into the browser
workbench that takes its whole host page down on a bad edit is a worse
experience than one that reports, and the workbench exists specifically for
people to make bad edits and watch what happens.


That last one may want your answer before this gets built, because it's the one
case where the policy you chose might need an exception, and an exception
decided during implementation tends to become an accident.


Running tally on the question pass: three answered, **thirty still open** —
twenty-six from the original sweep, plus the four this ticket just added. Want
to keep going down the list, or take the embedded-policy question now while it's
in view?


--------------------------------------------------------------------------------

### User Request 15

  Four open questions went in with it, since they surfaced while writing and
  would otherwise evaporate: where the diagnostic report is written when nobody
  has

  a terminal


to a tmpfs location, same as any of our logs


what exit code the interrupt path uses,


you tell me


whether the diagnostics task can be an ordinary task at all when it's the one
task that must be

  guaranteed to run


it's an ordinary task that's created when the other tasks are removed from the
task pool. It doesn't have to be a task I guess? We could just make it part of
the system that's handling the interrupts / shutdown process.


does the fatal policy survive being embedded?


it must


--------------------------------------------------------------------------------

### Assistant Response 15

All four answered. The third one is a real simplification — let me record them
and say why.


All four recorded and committed. Three were straightforward; the third one
changed the design, so here's what happened.


**The report goes to RAM, with one requirement attached.** Same shared-memory
directory as every other log — the build already creates the link and the demo
runner already ensures it exists. Two things fall out that I wrote down rather
than left implicit. It won't survive a reboot, which is correct for something
you read while debugging and useless as a post-mortem, and that's what the core
dump is for. And **the destination has to be opened during startup, not while
dying** — that directory lives on a filesystem a reboot empties, so it may be
absent, and creating it in a failure path means a system call that can fail for
reasons a dying program has no way to handle. Open it at initialization and keep
the integer.


**Exit codes — my call, and the reasoning is that the space is already
divided.** Everything below 128 is yours; 128 and up belongs to signals by
convention, so engine meanings never reach up there.


| code | meaning |

|---|---|

| 0 | ran out of work, or was politely asked to stop |

| 65 | a map file the engine refused — the input was malformed |

| 70 | an invalid operation from a program's own construction calls — the calling code was wrong |

| 71 | out of memory, or another resource no edit can fix |

| 130 | interrupted by a person |

| 134 | not chosen — quit aborts, the shell computes this |


The three middle ones are the ones that earn their place. This project already
draws a line between a fault a caller can correct and retry and one it can't —
it shows up in two separate issues as "a misspelled box name versus out of
memory." Those codes make that line visible to a shell script instead of only to
somebody reading the message.


**The third answer collapsed the design, and I want to be clear how much.** You
said the diagnostics don't have to be a task. Following that: if the thread
receiving the signal writes the report itself, then nothing is enqueued, no
worker is needed, and the "requires at least two live threads" caveat vanishes.
Which means **the informative path now works on a program whose every worker is
wedged** — the exact case it was written for, and the exact case the
task-based version couldn't survive. The queue guarantees nothing to anybody,
and the one piece of work that must happen can't be the piece waiting in line.


That dragged a second change with it. If the gathering happens on a normal
thread, there's no reason to install a handler at all — **block the three
signals everywhere and have the initial thread wait for one.** Waiting returns
the signal number to a thread running ordinary code, so the entire catalogue of
things a handler may not call stops applying. It can take locks. It can format
text. That restriction was the hardest thing I was designing around for the last
several exchanges, and it turned out to be avoidable rather than manageable.


One wrinkle with a nice fix: that thread would otherwise be blocked collecting
workers, and waiting for a signal and waiting for the pool are two different
waits. So the last sleeper sends the initial thread a signal after broadcasting
shutdown. One place to wait, two reasons to wake, told apart by which number
arrives.


I updated the implementation steps, the comparison table, and the interrupt
section to match — the ticket no longer describes a mechanism we've abandoned.


**And the policy holds when embedded, at no cost.** A program compiled into the
workbench that aborts doesn't take the page down: the instance traps, the
runtime unwinds it, the host gets an exception, the page is fine. So the
workbench's answer to a refused edit is "that program died, here's what it
wrote, here's a fresh one" — which is better than a half-broken program that
keeps running, given the workbench exists for people to make bad edits and
watch.


Tally: **seven answered, twenty-six still open.** Want to keep going down the
list?


--------------------------------------------------------------------------------

### User Request 16

sure.


--------------------------------------------------------------------------------

### Assistant Response 16

Next: **the output station** (209), first of its two.


The output station is a pass-through — it runs no box, and its input ports
*are* the program's output ports. Values arriving there have reached the
boundary; a parent wiring into a sub-program reads whatever landed. That's what
makes composition need no mechanism.


**The question as written:** does a program with no output station get refused?
The issue says yes, per an earlier decision — it hasn't said what it's for.
But that reason was entangled with the seed sweep, which has since been deleted
outright, so the conclusion is sitting on a foundation that isn't there.


**Two things have happened since that I think decide it.**


First, there's no longer a moment to ask the question. The build-a-program issue
already answered that reading a map file "stays only a sequence of operations,
so several files can be read into one program" — there is no end-of-file event
any more. A program with no output station right now might get one from the next
line, the next file, or a rewire an hour from now. Every whole-program property
in this design has already migrated the same direction: the load-time validation
issue lost two rules and its remainder "becomes a report, not a refusal," and
the seeded-nothing check became "reports that through the ordinary termination
check." A refusal needs a moment. This property doesn't have one.


Second, and I think this is the stronger one: **a program with no output station
can be completely correct.** Your own issue establishes it — "a program that
writes to disk simply has a station that writes to disk," an ordinary box
needing no designation. A program whose work is entirely side effects produces
nothing at the boundary by design. And the engine cannot tell that program apart
from one where somebody forgot, because a box is a C function and nothing about
it reveals whether it touches the world. So refusing would mean guessing, and
guessing wrong would refuse a category of program the issue explicitly blesses.


So my answer is **report, don't refuse** — it joins the whole-program
remainder that's already a report.


**But I don't think the refusal disappears, I think it relocates**, and I'd like
your read on this because it's the interesting part. The place "this program has
no output" genuinely matters is composition: a parent trying to wire from a
sub-program's output port when there isn't one. That's not a whole-program
property at all — it's **a per-edge check at the exact moment both ends are
known**, which is the rule this engine already runs everything on, and which
runtime rewiring reuses unchanged. It would be an invalid operation, so under
the policy you just set it's fatal, and it names both sides the way every other
wire failure does.


Which would mean: running a program with no outputs is fine and reported.
*Wiring into* one that has none is an error, caught by machinery that already
exists, at a moment that already exists.


Does that split match how you think about it — the program alone being
nobody's business, and the failure belonging to whoever tried to compose it? Or
is there a case for the engine having an opinion about a program that produces
nothing at all, even when nobody's asking it for anything?


--------------------------------------------------------------------------------

### User Request 17

so, we can only wire into and out of a box through the input and output
stations. I think we should enforce the requirement that we have an output
station, and if there's no output wires provided to it then it just doesn't do
anything. Like returning "void" in C - the program still knows that the function
has returned, because now it can move on to the next task. With this system, we
just wouldn't emit anything at all.


--------------------------------------------------------------------------------

### Assistant Response 17

Recording it — and your `void` analogy answers an objection I raised, then
opens a different one.


Recorded and committed. Two things worth pulling out.


**Your answer beats the one I recommended, for a reason I'd argued myself into
missing.** I said the engine can't refuse a program with no output station
because it can't distinguish "deliberately produces nothing via side effects"
from "forgot" — a box is a C function and nothing about it reveals whether it
writes to disk. That's true, and requiring the station *dissolves* it rather
than working around it. The program states which one it is. A side-effect-only
program declares "I produce nothing" by having the door with nothing wired to
it, and is ordinary rather than ambiguous. The thing the engine couldn't infer
becomes a thing the program says.


**And the check has a home, which was my other objection.** I said there's no
moment left to ask a whole-program question, since files no longer end. But the
starting gate exists — workers park there until released, and the termination
design already leans on it. That gate is the one instant a thing stops being
built and starts being a program, so it's exactly where a whole-program
requirement can honestly be asked. Failing it is an invalid operation, so it's
fatal under the policy you set, at exit code 70 or 65 depending on whether a map
file or a caller's code put it there.


**One new open question came out of your analogy**, and it's a real conflict
rather than a loose end.


A `void` function still *returns*, and the return is an event — that's why
`f(); g();` runs `g` afterward. But an output station with nothing wired into it
never receives anything, so it never fires, so **nothing downstream can ever be
triggered by it.** A sub-program that yields no values is a leaf: nothing can
depend on it, because there's nothing to depend on. So the declaration half of
your analogy holds and the ordering half doesn't.


That may well be correct rather than broken — this engine has no sequencing
anywhere. Things happen when their inputs are ready, and "run this after that"
isn't expressible in any other part of the design either. A dataflow engine
arguably shouldn't grow an ordering primitive just for void.


But it collides with something the output-station issue currently promises in
plain words: composition needs "no *finished* to detect, no per-program task
counting." If a void sub-program is meant to be composable, something valueless
has to cross the boundary to say it happened — which is exactly a finished to
detect. **Those two claims can't both stay**, so I wrote it in as still-open
rather than picking for you.


Tally: eight answered, twenty-six open — one closed and one opened, so the
count held steady. Next up is the second half of the output station question: an
output buffer nobody drains grows until memory runs out, and whether there
should be a ceiling. Want that one, or take the void-composability conflict
while it's fresh?


--------------------------------------------------------------------------------

### User Request 18

  But it collides with something the output-station issue currently promises in
  plain words: composition needs "no finished to detect, no per-program task

  counting." If a void sub-program is meant to be composable, something
  valueless has to cross the boundary to say it happened — which is exactly a
  finished to

  detect. Those two claims can't both stay, so I wrote it in as still-open
  rather than picking for you.



not necessarily. We don't need to pass something "valueless" - it should just
compose cleanly and order itself according to how it's wired.


--------------------------------------------------------------------------------

### Assistant Response 18

You're right — I framed that as a conflict and it isn't one. Ordering *is*
wiring, so "nothing can be sequenced after a void sub-program" isn't a gap in
the design, it's what void means stated in the engine's own terms.


Recorded: composition needs no finishing to detect, unqualified, because
everything anyone would detect it *for* is already an edge. And where ordering
genuinely matters, the answer isn't a pulse — it's that the program should
hand over a value, at which point it isn't void. Even an acknowledgment carries
a meaning a bare signal wouldn't.


Nine answered, twenty-five open.


---


## Next: the output buffer with nobody at the other end


The second and last question in **the output station** (209).


Results accumulate at the output station's port and stay there. Not discarded
— this is the one port kind where unwired means *hold*, because throwing away
a program's results means the program did nothing. Writing them to a file was
considered and dropped, since it would have meant the engine owning a file
handle, a thread-safe append, a decision about failed writes, a
platform-dependent path, and a format — the first time the engine touched the
filesystem for program data rather than diagnostics. All of it disappears if the
values just stay put.


**So the buffer grows.** A program nobody drains keeps computing and keeps
piling results up until memory runs out. The issue's answer today is the
warning, and it's a good one: an *ordinary* buffer growing means a consumer is
slower than a producer, which is a performance note summarized at teardown. An
*output* buffer growing means **nobody is collecting the program's results at
all**, which is a different diagnosis entirely and fires loudly from the very
first doubling.


**What's unresolved is whether there should also be a ceiling**, and the issue
is honest that all three answers look bad:


- **No ceiling** — grow until the allocator fails. The warning has been
  screaming since the first doubling, so nobody can claim they weren't told.

- **Refuse new results at some depth** — the program keeps running but
  silently stops recording what it computed, which is the discarding this port
  kind exists to prevent, arriving late instead of immediately.

- **Block the producer** — apply backpressure so upstream stalls. The issue
  calls this worse than both, and I agree: a station wedged waiting on a
  consumer that may never come is indistinguishable from the wedge we spent this
  morning establishing the engine cannot detect from inside.


**Two things changed since that was written, and I think they resolve it.**


Running out of memory is no longer a vague bad ending. It's an invalid operation
under the policy you set — fatal, with the whole diagnostic report written on
the way out, at the exit code reserved for a resource no edit can fix. So "no
ceiling" now means: warn from the first doubling, keep going, and if the
allocator ever fails, die loudly having named every station, every buffer depth,
and which one was swallowing everything. That's a considerably better ending
than it was when this question was written.


And a ceiling would need a number, which is the thing you've already ruled out
once today — any depth I could pick is wrong on a machine with more memory and
wrong differently on one with less.


So my recommendation is **no ceiling**: the warning is the contract, and the
crash is the enforcement.


Here's what I'd want you to push on, though. That reasoning quietly assumes
**the program that fills memory is wrong**. But there's a legitimate shape it
describes: a program deliberately run to completion with nobody attached, whose
results are meant to be drained at the end rather than during. Under "no
ceiling" that program is fine right up until it isn't, and the difference
between working and dead is how much RAM the machine happened to have.


Is that a program you want the engine to support — and if it is, is the
missing piece really a ceiling, or is it that **nobody should be able to start a
program without saying who's collecting from it?** That would make the output
station's consumer part of the interface, checked at the same gate that now
checks the station exists at all.


--------------------------------------------------------------------------------

### User Request 19

no ceiling. we want to get out of the users way, with minimal complexity.


  Here's what I'd want you to push on, though. That reasoning quietly assumes
  the program that fills memory is wrong. But there's a legitimate shape it

  describes: a program deliberately run to completion with nobody attached,
  whose results are meant to be drained at the end rather than during. Under "no

  ceiling" that program is fine right up until it isn't, and the difference
  between working and dead is how much RAM the machine happened to have.


  Is that a program you want the engine to support — and if it is, is the
  missing piece really a ceiling, or is it that nobody should be able to start a

  program without saying who's collecting from it? That would make the output
  station's consumer part of the interface, checked at the same gate that now

  checks the station exists at all.



if the user wants to write that program, they need to set up a system that
watches for the RAM to fill and drains it when it's almost full.


--------------------------------------------------------------------------------

### Assistant Response 19

Ten answered, twenty-four open. That closes the output station entirely — both
its questions are now answered in place.


---


## Next: **the input station** (213), first of three


The input station is the mirror: a pass-through whose **output ports are the
program's input ports**. Values arrive from outside — a parent map, a shell
runner passing the command line, an embedding C application — and flow onward
as ordinary deliveries. Command-line arguments come nearly free, because the
text-to-bytes reader that turns a static's brace syntax into correctly laid-out
bytes gets pointed at the argument list instead, same compiler-computed offsets
and all.


**The question:** several input stations, or one with several ports? The issue
says "same question as the output side and it should get the same answer."


**I don't think it does get the same answer, and I want to check my reasoning
against yours.** The output side chose several stations for one specific reason:
one station owning subsets of its ports "would be the only thing in the engine
with several readiness checks over subsets of its ports. Every other station has
one check over all of them."


That argument is about **input** ports, because readiness is a property of input
ports. An input station's ports are **output** ports — and the engine is
already perfectly comfortable with a station having many of those. A comparator
has three, an iterator has as many as you wire. There's no readiness check on
the output side of anything.


**But there's a real difference that cuts the other way**, and it's the phase 5
sentence: *a port is a choice of destination, not a set of results.* A
comparator's three ports and an iterator's many all carry **the same value**,
routed to one of them. An input station with several ports would need port 0
carrying an `int` and port 1 carrying a `double`, independently and
simultaneously. That's genuinely new — no station in the engine today has
several output ports carrying different values at once.


So the honest read is: the output-side reason doesn't transfer, but a
*different* novelty appears on the input side, and the two aren't the same
shape.


**And one thing I found while checking, which you should probably decide
first.** The output station issue is ambiguous about whether an output station
has *one* input port or several. It says each has "its own inputs" — plural
— and "its own single output port." But the subset-readiness argument only
bites if a station can have several input ports mapping to several separate
program outputs. If each output station has exactly one input port, the argument
is trivially satisfied and the reasoning as written is doing no work.


I suspect that passage is a survivor from the draft where the output station ran
an ordinary box — the issue explicitly says it was corrected away from that,
and the three consequences it lists as following from pass-through don't include
this one.


So: **how many input ports does an output station have?** If one, then "several
outputs means several stations" is true but for a simpler reason than the one
recorded, and I'd fix that text. If several, the recorded reason stands and I'd
want to hear why the subset problem doesn't just disappear when nothing runs.


--------------------------------------------------------------------------------

### User Request 20

  But there's a real difference that cuts the other way, and it's the phase 5
  sentence: a port is a choice of destination, not a set of results. A
  comparator's

  three ports and an iterator's many all carry the same value, routed to one of
  them. An input station with several ports would need port 0 carrying an int

  and port 1 carrying a double, independently and simultaneously. That's
  genuinely new — no station in the engine today has several output ports
  carrying

  different values at once.



yeah we'd need multiple return values from C and that's not really a thing
unless we make a struct and deconstruct it in the box but we're not going to do
that because we don't want the users to have to write special functions - they
should have minimal adjustment to fit into soramech. Instead, let's have
multiple input stations with singular outputs. There can be a function that's
run on the input or not - it could just route the inputs to multiple interior
boxes.


  And one thing I found while checking, which you should probably decide first.
  The output station issue is ambiguous about whether an output station has one

  input port or several. It says each has "its own inputs" — plural — and
  "its own single output port." But the subset-readiness argument only bites if
  a

  station can have several input ports mapping to several separate program
  outputs. If each output station has exactly one input port, the argument is

  trivially satisfied and the reasoning as written is doing no work.



multiple inputs on the output stations. Same as any other box. The output box
can have a function to process the data into new shapes, or not. We can also
have multiple output stations for different output formats - switching between
them is as simple as rewiring to the other output box.


--------------------------------------------------------------------------------

### Assistant Response 20

Two decisions that revise the model rather than fill blanks. Let me rewrite the
affected sections.


Now the input station.


Both doors rewritten. Eleven answered, twenty-three open. What changed is bigger
than a question getting an answer, so here's the shape of it.


**The doorways stopped being a special kind of station.** They're ordinary
stations carrying a designation, and the designation says exactly two things:
the outside world may deliver here, and an unwired output holds its values
instead of discarding them. Nothing else about them differs from any other
station. That deleted a whole "pass-through station kind" from both issues'
implementation steps.


**And the model got more capable while getting simpler.** Because a doorway can
run a box, results can be shaped on the way out — and since that box is just a
box, your point about several output stations for several formats falls straight
out: same results, different shapes, switching by moving one arrow. I wrote that
in as a stated capability rather than leaving it implied.


**The rule for how many doors a program has turned out to rest on something
other than what was recorded.** The issue's reason was the subset-readiness
argument — that one station owning groups of its ports would be the only thing
in the engine checking readiness over subsets. That's true, and I kept it, but
it's the *second* reason. The first is the one you gave: a C function returns
one value, so a station has one output port, so each separate result is its own
station. Faking several returns needs a struct built by one box and taken apart
by another, and **that's a function somebody writes only to satisfy the engine**
— which is the cost this design refuses. I made that the leading argument in
both issues, because it's the one that generalizes: it's also why the input side
gets several stations, where the readiness argument doesn't reach at all.


**One correction to my own framing.** I'd asked whether the output side's
reasoning transfers to the input side. It doesn't — but the answer is the same
anyway, arriving by a different route. Readiness lives on input ports, and an
input station's graph-side ports are output ports, which the engine has always
allowed many of. So the subset argument genuinely has nothing to say there. The
C-returns-one-value argument covers both sides.


**Three things I inferred rather than were told**, flagged because they're now
written into the issues as if decided:


- Results accumulate at the output station's **output** port, not its input
  ports. The old text had a parent reading "whatever landed on the matching
  input," which no longer makes sense once the station is ordinary — a parent
  wires from an output port like it would anywhere else. This makes composition
  *more* ordinary, so I think it's right, but it is a change to where a
  program's results live.

- **A station with no box must have exactly one input port**, necessarily —
  with nothing running, there's no way to combine several arriving values into
  the one value an output port carries.

- Fan-out already covers what people usually mean by "several inputs": one
  argument reaching six interior stations is one output port with six
  destinations, which has worked since phase 2.


Next in the input station is whether an input port accepts several arrows — a
parent and a control socket both delivering to the same program input,
interleaving. Want to keep going?


--------------------------------------------------------------------------------

### User Request 21

  - Results accumulate at the output station's output port, not its input ports.
    The old text had a parent reading "whatever landed on the matching input,"

  which no longer makes sense once the station is ordinary — a parent wires
  from an output port like it would anywhere else. This makes composition more

  ordinary, so I think it's right, but it is a change to where a program's
  results live.



right because the output box function might want to run on the data. Something
to keep in mind is that we should only transfer the inputs to the output IF we
have all the input slots filled with at least one value on the output box. Even
if there's no box function. So if we have 3 results in port A and 1 in port B,
then we should only move one set of values to the output port.


  - A station with no box must have exactly one input port, necessarily — with
    nothing running, there's no way to combine several arriving values into the
    one

  value an output port carries.


um, I think we don't need that kind of rule. If there's always at least one
input, and always at least one output, we can just... wire the input to the
output, and possibly run some box functions in the input boxes and output boxes.


--------------------------------------------------------------------------------

### Assistant Response 21

Point one is a real correction and it changes a diagnosis. Recording both.


Recorded. Your first point turned out to buy more than uniformity.


**Results now leave in complete sets or wait**, and that isn't arranged anywhere
— it falls straight out of the doorway answering to the same readiness check
as everything else. A program reporting several things reports them in step,
with the partial set held rather than emitted. That's the readiness check paying
for something nobody built it for, which is roughly the fourth time that's
happened in this design.


**And it split a diagnosis into two that point opposite directions.** An output
station can now pile up in two places. Values stuck on its **input** ports mean
the program produces its several results unevenly — one branch of the graph
outrunning another, complete sets never assembling. That's a problem *inside*
the program. Values stuck on its **output** port mean sets assemble fine and
nobody outside is collecting. That's a problem with whoever started it. A single
warning covering both would send someone searching the wrong half of the
building, so I wrote it into the implementation steps as two warnings that must
read differently. It extends the backlog taxonomy the project already keeps —
slot pile means uneven inputs, task ring pile means slow consumers, and now the
doorway has one of each.


**On the second point, you were right and I'd over-formalized it.** I'd written
"a station with no box has exactly one input port" as a rule the engine imposes.
It isn't one. One way in, one way out, value crossing untouched — not because
anything forbids otherwise, but because more than one way in needs something to
bring them together, and that something is a function. Having several inputs is
what it *means* to have a box. I rewrote it as a consequence rather than a
constraint, so nobody later goes looking for the check that enforces it.


Twelve answered, twenty-three open.


---


Next in the input station: **does an input port accept several arrows, like any
other port?**


The concrete case the issue names is a parent map and a control socket both
delivering to the same program input. They'd interleave — which is ordinary
behavior for a ring port, since values from two upstream stations already arrive
in whatever order the threads produced them, and arrival order was arbitrary to
begin with. The issue's own instinct is that this is "probably fine, but it is
worth saying rather than discovering."


I think it's fine and follows from what's already decided, but there's a wrinkle
worth your eye: **the input station is the one port in the engine where one of
the writers isn't part of the graph.** Everything else that interleaves is two
stations inside one program, both subject to the same construction surface and
the same wire checks. A control socket delivering from outside is checked
per-delivery, but nothing about the program's shape records that it exists —
so a map dump would round-trip a program that behaves differently than it reads,
because one of its inputs comes from somewhere the file can't mention.


Want to take that one, or would you rather I keep moving and batch a few of the
smaller ones?


--------------------------------------------------------------------------------

### User Request 22

  Next in the input station: does an input port accept several arrows, like any
  other port?


several arrows into an input station is just accepting calls from multiple
locations. The trick is, we return them to specific locations. So, I could see a
central "factory" that takes in inputs from let's say two sources. One source
wants the object returned painted blue, one wants it red. So they each send in
values to two input ports on the same input box - two arguments for the input
box function. the first input argument is "object xyz" and the second is "blue"
or "red" or whatever. Then, the box function would run with the first set of
inputs - object xyz and blue. The factory would do it's thing and then it'd pass
the output to the "blue" output line, and the second set of inputs would get
passed to the "red" output box according to the logic of the box functions and
such.


  I think it's fine and follows from what's already decided, but there's a
  wrinkle worth your eye: the input station is the one port in the engine where
  one of

  the writers isn't part of the graph. Everything else that interleaves is two
  stations inside one program, both subject to the same construction surface and

  the same wire checks. A control socket delivering from outside is checked
  per-delivery, but nothing about the program's shape records that it exists —
  so a

  map dump would round-trip a program that behaves differently than it reads,
  because one of its inputs comes from somewhere the file can't mention.



that's fine.


--------------------------------------------------------------------------------

### Assistant Response 22

Your factory example has a bug in it, and finding it is worth more than the
answer. Recording both.


Both answered, and the factory example surfaced a real defect.


**The answer itself is clean.** Several arrows into one input port is ordinary
— every port has always accepted them, and callers interleave the same way two
upstream stations feeding one port already interleave. The part worth keeping is
the principle underneath your example: **the engine never learns who called, and
doesn't need to.** Where a result should go rides in the values. The blue one
goes down the blue wire because something downstream read the colour, not
because anything tracked the sender. That's the same move as ordering-is-wiring
— everything the engine might have been asked to remember becomes something
the graph carries instead.


**The defect is in the two-port version of the factory.** Object on port 0,
colour on port 1, two callers each sending their own pair. Those pairs will be
matched **arbitrarily** — the first caller's object wearing the second
caller's colour is a completely legal outcome.


The reason is already recorded and deliberate. The input-port record states that
values may leave a port in a different order than they arrived: with fixed
positions gone, a reader takes the first ready cell its scan finds, and rollback
opens gaps wherever it happens. If order *within* one port isn't promised,
correspondence *across* two ports certainly isn't. Nothing pairs them; readiness
just takes one from each.


**The fix needs nothing built** — it's been finished since phase 3. Anything
that must arrive as a unit is one struct on one port: a field table per struct
from the generator, a reader that walks it against brace text with every offset
computed by the compiler, and command-line arguments already speaking that same
brace syntax. The object and its colour become one value, and nothing the
scheduler does can get between them. One input station per caller also works,
but it scales with the number of callers rather than with the interface, which
is the wrong axis.


I wrote it into the guarantees doc's queue as a stated non-guarantee beside the
ordering one, since this is the shape in which that missing promise will
actually draw blood.


**The dump wrinkle recorded as you called it:** a program's file records the
program's shape, and who may deliver into it is no more part of that shape than
a C function's callers are part of its signature. The dump round-trips the
program; it never claimed to round-trip the world around it.


Fourteen answered, twenty-one open.


One left in the input station: a program run from a shell whose input station
has ports the command line doesn't fill. Those ports never receive a value, the
stations behind them never become ready, and the program just sits there.
Refusal at startup, a report, or simply what happens?


--------------------------------------------------------------------------------

### User Request 23

 The defect is in the two-port version of the factory. Object on port 0, colour
 on port 1, two callers each sending their own pair. Those pairs will be

  matched arbitrarily — the first caller's object wearing the second caller's
  colour is a completely legal outcome.


that's fine. If you need to match arguments to each other, which is very common,
then they should be handled in a C function. This is certainly a limitation of
the system - it is meant to be used for systems where each value passing through
the system can stand on it's own, and work with anyone else of the same
function. For specifying exact pairings, the system should be designed to pass a
single struct, instead of two separate input values. But yes it is certainly a
limitation. This is one of the things the system doesn't guarantee - such as
ordering of tasks or timing. What we do guarantee is the full utilization of the
processor cores, as best as we can according to the design of the system.


--------------------------------------------------------------------------------

### Assistant Response 23

That last sentence is the first positive guarantee stated in this whole pass —
let me see where it belongs.


The pairing point is already in the guarantees doc — V1 and V2 state it,
including "if two things were meant to stay together, they were meant to be one
value." So that needs no adding; I'll fix my reference in the issue. But **the
positive guarantee you just stated isn't there at all.**


Your last sentence turned out to be the missing headline of the whole guarantees
page.


**That page listed everything the engine holds true and everything it declines
to, and never said what the declining was for.** Read that way it looks like a
pile of unrelated concessions. It's one purchase, and it now opens the page as
**the bargain**, with the promise stated as a numbered guarantee like everything
else: *no worker sits idle while a task is ready to run.*


What sells for it, spelled out: ordering values would mean a slow write holding
finished ones behind it. Pairing across ports would mean a ready value waiting
for its partner. A batch or a round would mean cores that finished early
standing still for cores that finished late. Each one is a worker not running
one of the ten things that are ready — the single cost this design refuses.


**And your "stand on their own" line gave an existing rule a second face.** The
guarantees doc already said a value carries no relationship to any other value,
written from the *data's* side — so whatever must stay together must be one
thing. From the *scheduler's* side the same sentence reads: nothing belongs to
anybody in particular, so whichever worker is free may take whatever is ready.
Same fact, and the second reading is the one that explains why the first is
worth its price. I wrote both faces in.


I recorded the honest hedge in your phrasing too — held by structure
everywhere except one place. A box that blocks parks a worker on something the
engine can't see and withdraws the guarantee silently for as long as it runs.
That's now the strongest argument on the page for teaching the generator to
refuse blocking calls in box sources, which it's already parsing anyway.


**One correction to myself:** I told you the pairing hazard "belongs in the
guarantees doc." It's been there all along, twice over — a value carries no
relationship to any other, and a station pairs whatever is at the head of each
port, complete with "if two things were meant to stay together, they were meant
to be one value." What the doorway genuinely adds is only a new place to walk
into it, since somebody who just wrote two arguments feels far more like they
sent *a pair* than a graph author splitting one value down two paths does. The
issue now points at what exists rather than asking for it again.


Fifteen answered, twenty open.


The last one in the input station is still waiting: a program run from a shell
whose input station has ports the command line doesn't fill. Those ports never
receive a value, the stations behind them never become ready, and the program
just sits. Refusal at the gate, a report, or simply what happens?


--------------------------------------------------------------------------------

### User Request 24

  The last one in the input station is still waiting: a program run from a shell
  whose input station has ports the command line doesn't fill. Those ports never

  receive a value, the stations behind them never become ready, and the program
  just sits. Refusal at the gate, a report, or simply what happens?


just wait :)


--------------------------------------------------------------------------------

### Assistant Response 24

"Just wait" turns out to already have a mechanism behind it, and it's one you
built in phase 1.


"Just wait" needed no new mechanism, and the previous answer had already forced
it — many callers may knock on the same door, so a port nothing has filled
can't be told apart from one that will be filled in a minute. Refusing would
mean assuming the command line is the last word anybody will ever have, which is
the assumption the door exists to avoid.


**The part worth knowing is what decides whether the wait ends, because you
built it in phase 1 and it's been sitting there unused for this.** An outside
submitter registers a standing promise that more work may come, and termination
waits while any promise is held. So a control socket still open holds one and
the program waits; the shell runner holds one while delivering the command line
and drops it afterward. With nobody promising anything, nothing more can arrive
and the program ends by the ordinary last-sleeper rule — having run what it
could.


Which means **a half-fed program finishes rather than hanging**, and that's a
materially different outcome from the one the question assumed. Better still,
the diagnosis is already legible without adding anything: the stations that
never became ready carry a completed-run count of zero, and those counts are
always on. That names both what didn't happen and why nothing downstream of it
happened either.


**The input station is now fully closed** — all three questions answered, and
between it and the output station, phase 2's two doorway issues have no open
questions left.


Sixteen answered, nineteen open. Where they sit now:


| issue | open |

|---|---|

| growing the station table | 0 |

| what an input port is | 0 |

| one way to build a program | 0 |

| the output station | 0 |

| the input station | 0 |

| stopping on purpose | 0 |

| **destinations without a lock** | 2 |

| **types compared by shape** | 3 |

| **boxes compiled at runtime** | 3 |

| **values back into text** | 2 |

| **the slideshow and transcripts** | 4 |

| **the demos after the pull path** | 2 |

| **the browser workbench** | 5 |


Phase 2 has one issue left with questions — destinations without a lock, which
replaces an output port's linked list of destinations with an immutable array
published by a single atomic write, and needs a scrapyard to reclaim the old
ones. Finishing that closes phase 2's question list entirely. Want it next, or
jump somewhere else?


--------------------------------------------------------------------------------

### User Request 25

Ask the next question?


--------------------------------------------------------------------------------

### Assistant Response 25

## Next: **destinations without a lock** (214), first of two


The change: an output port's destinations stop being a linked list read under
the station's mutex and become one immutable array behind a pointer. Rewiring
never edits it — it builds a whole new array and swaps the pointer in a single
atomic write. A walker reads the pointer once and walks something nobody will
ever modify. That deletes the last thing holding the station's mutex on the hot
path, along with a copy proportional to fan-out on **every value the engine
moves**.


**The question:** destinations were appended at the list's tail, so wiring order
was preserved *by the data structure* — there was no way to get it wrong. An
array rebuilt from scratch on every rewire has to preserve that order
deliberately. The issue notes this is easy, and flags the real concern: **an
invariant that used to be structural becomes something code has to remember.**


That's your guarantees page's own vocabulary. Structure is "it cannot be
otherwise without changing the shape of the data — the strongest kind."
Discipline is "nothing enforces it — the weakest kind, and the ones worth
staring at." This is a demotion from the top of that table to the bottom, bought
with a hot-path win.


Three ways to go, and I'd rank them in an order you may not expect:


1. **Delete the invariant instead of protecting it.** Have the dump write
   destinations in a **canonical** order — station index, then port index —
   rather than in wiring order. Then the rebuild can produce any order it likes,
   because nothing downstream cares. There's no rule left to break.

2. **Make the rebuild structural again** by always constructing the new array by
   walking the old one in order and applying the change. Order is then preserved
   by the shape of the operation rather than by remembering to.

3. **Accept discipline, held by the round-trip test.** There's precedent — the
   dump's round trip is already listed as held by a test rather than by
   structure.


**I'd argue hard for the first, because it makes things strictly better rather
than merely safe.** Wiring order has no semantic meaning anywhere else in the
engine: delivery visits destinations in array order, and the order values arrive
at different stations is already unpromised under the bargain. So wiring order
is decoration everywhere except the dump — and decoration that a test has to
defend is worse than no decoration.


And canonical order buys something new. Today two programs with identical shape,
wired in different orders, dump to different text. Sorted, **the dump becomes a
canonical form**: same shape, same bytes, so you can diff two dumps and learn
whether two programs are the same. That's a capability the project doesn't have
and would probably want. It also satisfies the existing round-trip guarantee
unchanged, since "dumping a dump yields the dump" is exactly what a canonical
form gives you.


The cost is that a dumped map no longer reads back in the order somebody wrote
it, which I think nobody wants and the map-file format never promised.


So the question to you is really the general one underneath: **when a structural
invariant is about to become a disciplined one, is the right instinct to protect
it or to ask why it exists?** Here I can't find anything wiring order is *for*.
Can you — is there something that reads destinations in order and means
something by it?


--------------------------------------------------------------------------------

### User Request 26

so wait, is this question just about... debug reports? Can you ask a little more
plainly?


--------------------------------------------------------------------------------

### Assistant Response 26

Yes, essentially — and let me strip it down.


A station that produces a value can send it to several places. Those places are
stored in a list on the output port.


- **Today:** a linked list, with new destinations added at the end, so it stays
  in the order you wired them.

- **The change:** an array that gets thrown away and rebuilt from scratch every
  time you rewire.

- **The question:** when it rebuilds, does it have to put them back in the same
  order?


**Nothing about how the program runs cares.** Delivery sends the value to all of
them, and the order it visits them in is already not promised — that's part of
the bargain we wrote down earlier.


**The only thing that notices is the dump** — the feature that takes a running
program and writes it back out as a map file you can reload. If the order
changes, the file's text changes. So it's not a debug report, it's the save-file
format, but the question really is only "what does the text look like."


So the choice is:


- **Keep the order.** The rebuild code has to remember to, forever, and a test
  has to watch it.

- **Sort them instead** — by station number, then port number. The rebuild can
  do whatever it wants, because the dump sorts at the end anyway.


**I'd sort them**, for a reason beyond convenience: two programs with the exact
same shape, wired in a different order, currently dump to different text.
Sorted, they'd dump identically — so you could compare two save-files and tell
whether two programs are the same. That's a thing you can't do today.


The only loss is that a dumped file no longer shows the order somebody
originally wired things in, which as far as I can tell nobody wants.


Sort them, or keep the order?


--------------------------------------------------------------------------------

### User Request 27

I think that we don't need to keep track of wiring order. If we're doing arrays
each time, then I don't see how the order could get confused, but let's say it
does - I think that's fine. The whole system is intentionally multithreaded -
timing and pairing are things that can't be guaranteed.


--------------------------------------------------------------------------------

### Assistant Response 27

Recorded: the invariant gets deleted rather than defended. Nothing reads
destination order and means anything by it, so there's no rule to demote and
nothing for a test to guard.


Two notes on it. The save-file still round-trips with nobody arranging it —
dump, load, dump contains no rewire, so the order a file is read in is the order
it's written back out in. And I wrote down what you're declining: sorting would
have made the dump a canonical form, so two identically-shaped programs would
come out byte-identical and could be compared. Not needed today, and since it
asks nothing of the rebuild, it can be added later by whoever wants it.


Seventeen answered, eighteen open.


---


## Next: **does the retire list need its own lock?**


Some background, since this is the mechanism that makes the whole change safe.


When rewiring swaps in a new destination array, some worker may still be walking
the old one. Freeing it immediately is a use-after-free; never freeing it leaks
one array per rewire. So old arrays go on a **retire list** and get freed later,
once nobody can be inside them.


How "nobody can be inside them" gets decided: each worker keeps a private
counter on its own cache line, bumped when a delivery walk begins and again when
it ends — so it's **odd while walking, even while not**. One uncontended
write, no coordination. When rewiring retires an array, it files it alongside a
snapshot of every worker's counter. Later rewires sweep the list first: an array
is safe to free once every worker is either even (not walking) or shows a
different number than the snapshot (finished the walk it was in). Nothing waits,
nothing spins, and an idle worker is even so it passes without having to be
woken.


**The question:** does that retire list need a lock of its own? The issue's own
answer is "probably not — only rewiring touches it, and rewiring is already
serialized by the rewiring lock." And then it flags its own reasoning as
suspect, in a phrase worth repeating: *"that is an argument from a lock existing
elsewhere, which is the kind of reasoning that stops being true quietly."*


I think the instinct to distrust it is right, and here's the concrete version of
the worry. The claim "only rewiring touches the retire list" is true today
because rewiring is the only thing that retires an array. But teardown also
touches it — the issue says anything still filed at teardown gets freed then
— and teardown isn't holding the rewiring lock. That's already a second
toucher, on a path nobody would think of as rewiring.


So my read: it doesn't need its own lock, but it needs its protection **named
after what it actually is** rather than borrowed. Something like "the retire
list is only ever touched under the rewiring lock, and teardown takes that lock
too" — stated as a rule at the list, not inferred from a lock that happens to
be nearby. Then when somebody later adds a third toucher, the rule is sitting
where they'll trip over it instead of being a chain of reasoning they'd have to
reconstruct.


Does that match how you'd want it, or would you rather the list just own a lock
so the question can't come back?


--------------------------------------------------------------------------------

### User Request 28

I'm worried about double freeing an array - these arrays don't move, so I don't
see the harm in mutexing them. Once we get the lock, first check to see if
there's anything still in it, then free it. That way we don't double free.


--------------------------------------------------------------------------------

### Assistant Response 28

Your teardown-versus-sweep worry is the concrete version of it, and a mutex is
genuinely free here. Recording.


The scrapyard gets a mutex. Your double-free instinct was right, and the issue's
suspicion of its own reasoning turned out to be justified in a specific way
worth noting: **the argument was already false when it was written.** "Only
rewiring touches the list" ignored teardown, which empties whatever is still
filed and isn't holding the rewiring lock. Two hands, one of them easy to
overlook, and nothing stopping both from freeing the same array.


The protocol as you described it: take the lock, confirm the array is still
filed, unfile it, free it, all under one hold. Unfiling before freeing under the
lock is the whole mechanism — a second arrival simply doesn't find it.


**And it gives up nothing this issue is for.** The mutex being removed is the
one on the delivery walk, which now reads a pointer and follows an array and
asks nobody's permission. The scrapyard is touched when wiring changes and when
the program ends, never in between, so a lock on it is charged against a path no
value travels.


I added one rule you didn't ask for, and I'll flag it as mine: **nothing may be
acquired while holding that lock.** It's a leaf. That's obvious right up until
the person who knew it isn't around, and nobody builds a lock-ordering cycle on
purpose — they build it by adding one reasonable-looking call inside a
critical section. Writing it at the lock is cheaper than rediscovering it. And
the implementation steps now carry a test for the specific race: a teardown
against a sweep, each array freed exactly once.


**That closes phase 2 completely.** All six of its open issues — the input
port record, growing the station table, the build-a-program capstone, both
doorways, and destinations without a lock — now have zero open questions.


Eighteen answered, sixteen open. What's left is three clusters:


| cluster | issues | open |

|---|---|---|

| **Phase 3** — the build path becoming callable at runtime | types by shape, boxes compiled at runtime | 6 |

| **Phase 4** — text and bytes | values back into text | 2 |

| **Phase 7/8** — documentation and tooling | slideshow and transcripts, demos after the pull path, browser workbench | 11 |


Phase 3 is the one with engine consequences — comparing types by layout
instead of by name, and handing a running program C source to compile. Want to
start there?


--------------------------------------------------------------------------------

### User Request 29

Sure.


--------------------------------------------------------------------------------

### Assistant Response 29

## Phase 3: **types compared by shape** (309), first of three


Right now every type check in the engine is a string comparison — the
generator emits each type's name as text, and the loader compares those.
Meanwhile the generator already emits, for every struct, a table giving each
field a kind, an offset, and a size, all computed by the compiler. **The engine
knows the exact shape of every value it moves and decides compatibility by
comparing labels.** This issue makes it compare shapes instead.


The question is about one specific pair.


A field's **kind** is one of: signed, unsigned, floating, fixed string, or a
nested struct. So a 16-byte text field and a 16-byte array of signed bytes have
the same size and different kinds. **Do they match?**


**The issue's own rule already says no** — shape is defined as kind plus size
plus offset, and these differ in kind. So the real question is whether you want
an *exception* carved for this pair.


The case for the exception: the issue has already decided that a four-float
position wires into a four-float colour without complaint, calling that the
chosen price of caring about the pattern rather than the label. Text and bytes
look like the same situation — same size, same actual bytes, different intent.


The case against: kind is doing real work elsewhere. A 4-byte signed integer and
a 4-byte float are the same size, and wiring those together would be genuine
corruption — the same bits decode to a completely different number. If kind
stops being uniformly part of the shape, the rule becomes "kind matters when the
bytes decode differently, and not when they don't," which is a harder sentence
to remember than "kind, size, offset."


**I'd say no exception: different kinds, different shapes.** Two reasons. It
keeps the rule to one sentence with nothing carved out of it. And the escape
hatch costs the author one word — someone who genuinely wants their text field
to feed a byte-processing box changes the declaration, and then the shapes agree
honestly rather than by special case.


The thing I'd want you to weigh against that: **serialization code does this
constantly.** Treating a text buffer as raw bytes is a real, common thing, not a
hypothetical. Refusing means anybody doing it writes a box that takes one and
returns the other and does nothing — which is exactly the busywork this issue
lists as a problem with the current name-based check.


So: exception, or no exception?


--------------------------------------------------------------------------------

### User Request 30

Tell me, what type checking are we doing? Also, can you walk me through the
loader and what it does? My understanding was that we were just writing sizes,
and trusting the pattern in their data to properly isolate each value from the
others. So a struct with three elements would instead just be those three labels
packed together into some bytes in the ring buffers and such. We explicitely
shouldn't care if we're putting one type of the same size - so this system I
think is weakly typed - is that the right term?


--------------------------------------------------------------------------------

### Assistant Response 30

Let me get you the facts from the code rather than my impression of it.


Good questions, and the answer to the second one changes the first.


## What type checking exists: one line


Exactly one check, in the loader:


```c

if (strcmp(from_box->return_type, to_type) != 0) {

    fprintf(stderr, "map %s:%d: %s -> %s.%d: box returns %s, slot takes %s\n", ...);

    abort();

}

```


That's the whole type system. A `strcmp` on the **type name as text**, at the
moment a wire is drawn. Mismatch prints both names and aborts.


**You're right that everything else runs on sizes.** The registry stores each
parameter as `{type_name: text, size: int}` where the size is a `sizeof` the
compiler computed. A cell is exactly `size` bytes, a delivery is a `memcpy` of
exactly `size` bytes, and a task is one allocation of exactly `task_size`. The
name is consulted once, at wire-drawing time, and otherwise exists only so the
error message can say `vec4` instead of `16`.


## The loader, in order


1. **First pass — make every station.** Look each box name up in the registry;
   an unknown name aborts with what the notes call deliberately the best message
   in the program. Slots default to ring buffers. A comparator gets its extra
   threshold slot, typed to the box's return. Statics get bound. A name-to-index
   table exists only during loading and dies at the end.

2. **Second pass — resolve the arrows.** Two passes exist because a wire can
   name a station declared further down the file, so nothing can be resolved
   until everything exists. Each wire gets the `strcmp` above at the first
   moment both of its ends are known.

3. **Whole-map validation.** Failures collected, then one stop with the whole
   list rather than one error per run.

4. **Seed.** The one scan the engine ever makes: stations that can run
   immediately get enqueued. Zero seeded is fatal.


**Caveat, because that's the loader as built and it's being dismantled:** the
seed sweep is deleted outright — construction's own writes start a program
now. Most of the whole-map validation becomes a report rather than a refusal.
And the build-a-program capstone turns the loader into a *caller* of one
construction surface rather than a mechanism of its own. So steps 3 and 4
largely go away, and step 2's check moves to the shared surface.


## The term you want is nominal versus structural


Strong/weak is a famously mushy axis and not the one you're pointing at. The
precise distinction is:


- **Nominal typing** — two types are the same because they have the same
  **name**. This is what the engine does today. `vec4` and `stats` are
  incompatible even when their bytes are indistinguishable.

- **Structural typing** — two types are the same because they have the same
  **structure**. This is what the shape issue proposes: compare kind, size, and
  offset per field, ignore all names.


And what you described — *"we shouldn't care if we're putting one type of the
same size"* — is a third position, weaker than either:


- **Size-only** — 16 bytes is 16 bytes. Nothing else consulted.


**Which is why my last question may not exist.** "Is a 16-byte text field the
same as 16 signed bytes?" is only a question under structural typing. Under
size-only there's nothing to ask: same width, done.


## What size-only costs, plainly


Two things stop being caught, and both fail silently rather than loudly:


**A 4-byte float wires into a 4-byte integer.** Same width, so it passes. The
bytes get copied and then read as a completely different number — `1.0f`
arrives as `1065353216`. No error, ever, anywhere.


**Two structs with the same fields in a different order become
interchangeable.** Same total width, so they pass, and every field lands in the
wrong place. The issue's sentence for this: the delivery path is a memory copy,
so anything that disagrees about where a field sits produces silently wrong
values rather than an error.


That's the trade. Nominal catches both but blocks honest work. Structural
catches both and allows honest work. Size-only catches neither and blocks
nothing.


**Which of the three do you want?** If it's size-only, the string-versus-bytes
question dissolves and I'll record that instead.


--------------------------------------------------------------------------------

### User Request 31

  Caveat, because that's the loader as built and it's being dismantled: the seed
  sweep is deleted outright — construction's own writes start a program now.

  Most of the whole-map validation becomes a report rather than a refusal. And
  the build-a-program capstone turns the loader into a caller of one
  construction

  surface rather than a mechanism of its own. So steps 3 and 4 largely go away,
  and step 2's check moves to the shared surface.



so basically the thread pool is created with each box's construction task
already in the thread pool, and we just iterate through those in random order
and enqueue the boxes that we can as soon as we make them? I worry that the
wiring won't be complete, so if we can run a task immediately we might do so
before any output is wired correctly. I guess, is that what output buffers are
for? And as soon as we wire, as part of the wiring process (we wire all the
wires as one step, not one-by-one) we deliver values if necessary.


we should do size-only.


--------------------------------------------------------------------------------

### Assistant Response 31

Your worry is exactly the one the build-a-program issue already found — and it
closed it the opposite way from what you're guessing.


**Construction is three ordered steps, not a scramble:**


1. **Create the stations.** Statics get bound here, but their writes are held.

2. **Attach every wire, as one batch.**

3. **Let the construction writes take effect** — and *that's* the moment a
   readiness check can fire and put a task in the pool.


The issue's own words on your exact concern: *"A station created in step 1 and
fed during step 2 could otherwise start running before its own outgoing wires
existed, and its results would be discarded. With the wiring done as one step
there is no such interval."* And on why the writes land last: *"nothing should
start while the picture is half-drawn."*


**So no — output buffers are not what saves you here, and that's deliberate.**
The design specifically avoided needing them: because the window can't open, an
unwired port can keep plain discarding, which it *must*, since an unwired
comparator branch is the normal case and buffering those would grow forever.
Your instinct found the real hazard; the design pays for it with ordering
instead of memory.


Two corrections to your model. Construction isn't tasks sitting in the pool —
it's calls on a surface, and what lands in the pool is work produced by
readiness in step 3. And at startup there's a second belt: workers park at the
starting gate until released, so even an early task can't run. On an
already-running program there's no gate, and the three-step ordering carries it
alone.


## Size-only it is


Recording that now. Worth stating what it does and doesn't catch, because it's
better than my framing suggested: **a size mismatch is still caught.** `int`
into `double` is 4 against 8 and still refuses. What slips through is only
same-width pairs — on x86-64 that's `int`↔`float`, and
`long`↔`double`↔pointer. A small set, and the wider mistakes still fail
loudly.


Size-only recorded, and it collapsed all three of that issue's questions at once
— the text-versus-bytes one, where the comparison lives, and whether the
registry needs to store a shape. None of them exist when the answer is one
integer against another.


**What I kept rather than deleted:** the shape design is still in the file under
"what was considered instead," with the reasoning intact. Partly because you may
want it back, and partly because the field tables it would have used are still
needed by two other things — the reader that turns brace text into bytes, and
the writer that will turn bytes back into text.


**One debt I moved rather than resolved**, and it's the reason I'm flagging it
rather than burying it. Width comparison fixes the first problem that issue
named — identical layouts with different names couldn't be wired — and
**widens the second one.** A box compiled while the program runs arrives
carrying its own idea of every struct it touches, and two disagreeing structs of
the same width now sail through. That was theoretical when the only compiler ran
at build time. It stops being theoretical in **boxes compiled at runtime**,
which is the very next issue in this phase and now owes an answer it didn't owe
an hour ago. I wrote the debt into both files so whoever picks it up finds it.


**One loose end for you:** the issue's filename still says `types-by-shape`
while its title now says width. I updated the heading, so the site reads
correctly, but the file is a slug that no longer matches. Want me to rename it
and update the references, or leave it?


Nineteen answered, thirteen open — that phase 3 issue closed three at once.


Next is **boxes compiled at runtime**, which is where the debt just landed. Its
three questions were written assuming shape comparison would exist, so at least
one of them has changed shape. Want to keep going, or pause here?


--------------------------------------------------------------------------------

### User Request 32

  One loose end for you: the issue's filename still says types-by-shape while
  its title now says width. I updated the heading, so the site reads correctly,
  but

  the file is a slug that no longer matches. Want me to rename it and update the
  references, or leave it?


rename and then fix the git commit


  One debt I moved rather than resolved, and it's the reason I'm flagging it
  rather than burying it. Width comparison fixes the first problem that issue
  named

  — identical layouts with different names couldn't be wired — and widens
  the second one. A box compiled while the program runs arrives carrying its own
  idea

  of every struct it touches, and two disagreeing structs of the same width now
  sail through. That was theoretical when the only compiler ran at build time.
  It

  stops being theoretical in boxes compiled at runtime, which is the very next
  issue in this phase and now owes an answer it didn't owe an hour ago. I wrote

  the debt into both files so whoever picks it up finds it.


all it really needs to know is the size of it's inputs and outputs. Right? That
seems easy to me.


--------------------------------------------------------------------------------

### Assistant Response 32

No remote, so amending is safe. Renaming now.


Rename folded into the commit — five documents updated, two of which had the
old title written into their link text. Site still clean at 99 pages.


Now your point about sizes, which I think deflates the debt I flagged.


You're right, and the debt I flagged was mostly my error. Corrected in both
files from their own sides.


**A box arriving late reports the width of each input and of its output**, by
the same `sizeof` the compiler works out while compiling that box. That's the
whole question the wire check asks. Nothing about arriving late makes the answer
harder to give.


**And it introduces no new kind of failure.** Two structs of equal width and
different layout already wire happily at build time — that's the accepted
price of comparing widths. A late-compiled box makes such a disagreement
*likelier*, because two separately-written sources agree less often than one
does with itself, but likelier isn't different. The hole was already there and
it's the same size. So the runtime-compilation issue's claim that shape
comparison was "a hard prerequisite, not a nice companion" is now marked as the
wrong call it was.


What a late box still owes beyond widths is mechanical: a name to be found by, a
shim to call, and a comparison if its return type is orderable.


**One thing I added that you didn't ask for**, flagging it as mine: a test
written deliberately to fail in an interesting way. A late box whose struct
disagrees in layout but not width gets *accepted* and delivers scrambled fields.
Pinning that in a test is how somebody two years from now learns it was a
decision rather than an oversight — the alternative is they find it in
production and assume it's a bug.


Nineteen answered, thirteen open. The runtime-compilation issue still has its
three, and they're the genuinely hard ones now that typing is settled:


- **Where does the compiler come from?** Requiring one at runtime is a
  dependency on the deployment environment, which the packaging note spends its
  whole length trying to shrink.

- **Can a loaded box ever be unloaded?** The issue already spots that this is
  the same lifetime problem as freeing a destination array and probably wants
  the same answer — retire, sweep, free. Which you just designed a mutex for.

- **Does a runtime-added box survive a dump and reload?** The dump names boxes;
  reloading needs them to exist again, so either the dump records where the
  source came from, or a dumped program only reloads into a process that already
  has them.


Want the first one?


--------------------------------------------------------------------------------

### User Request 33

  - Where does the compiler come from? Requiring one at runtime is a dependency
    on the deployment environment, which the packaging note spends its whole
    length

  trying to shrink.


assume it'll be there. We can build a proper install script that builds the
dependencies from source to be used locally once we're done making the system.


  - Can a loaded box ever be unloaded? The issue already spots that this is the
    same lifetime problem as freeing a destination array and probably wants the

  same answer — retire, sweep, free. Which you just designed a mutex for.


Sounds good.


  - Does a runtime-added box survive a dump and reload? The dump names boxes;
    reloading needs them to exist again, so either the dump records where the
    source

  came from, or a dumped program only reloads into a process that already has
  them.


Yeah we should write them to RAM somewhere. Treat them like a log file. Create
the file when the file is first created, not when we're dumping the status of
the program on an error.


--------------------------------------------------------------------------------

### Assistant Response 33

Three answers, and the second has a constraint you should see. Let me check the
packaging note first since your first answer bears on it.


All three recorded. Two things came out of them worth flagging.


**Your unloading answer gave the scrapyard a second customer, and it needs one
adjustment to serve both.** The destination sweep asks "is this worker inside a
*delivery walk*." Unloading code has to ask "is this worker inside a *box*,"
which happens earlier in the same task. Rather than two counters that would
drift apart, I wrote it as one per worker bumped around the **whole task** —
call, finish hook, free — so it's odd across both windows and answers both
questions. Destination arrays then become freeable slightly later than strictly
necessary, which costs nothing anybody measures, and there's one mechanism
instead of two. That's an inference on my part, so it's flagged as such in both
issues.


**And there's a limit on unloading that gets discovered rather than read**, so I
wrote it down: you can only start retiring a box's code once no station uses it
— and a station can neither change its box nor be removed, since the
station-table issue keeps growth only. So unloading serves the box that was
compiled and then never placed. Narrower than it sounds useful, and I'd still
build it, because the alternative is code accumulating forever in a program
whose entire premise is being rearranged while it runs.


**Your instinct about writing the source at creation is the same one you already
applied to the crash report**, and I linked them. A dump may be happening
*because* the program is dying, and a failure path is the worst possible moment
to discover something needs saving. Write it while writing is easy. The lifetime
that falls out is stated rather than implied: a dumped program reloads in any
fresh process on that machine, and not after a reboot — same lifetime as a
log, which is what you said to treat it as.


**Phase 3's runtime-compilation issue is now closed**, and so is the width
issue. Twenty-two answered, ten open — all of them in documentation and
tooling:


| issue | open |

|---|---|

| the generator, in C | 0 |

| types compared by width | 0 |

| boxes compiled at runtime | 0 |

| **values back into text** | 2 |

| **the slideshow and transcripts** | 4 |

| **the demos after the pull path** | 2 |

| **the browser workbench** | 5 |


The remaining engine-side one is **values back into text** — the mirror of the
struct-constant reader, turning bytes back into brace syntax so the map dump can
write statics out. Two questions. Want it, or would you rather stop here? We've
been at this a while and the phase 1 through 3 issues are all clear.


--------------------------------------------------------------------------------

### User Request 34

I think we should intentionally destroy boxes, not let them be garbage
collected. During the teardown process, we should first remove it from the...
where do we keep pointers to them? How do we find them? Whatever - anyway remove
it from that list, THEN make a snapshot of all the worker's task count, then
create a task to tear down that box if all the worker tasks change. But... What
if there's a long task? We can't exactly spin while we wait. So, next time we
tear down a box, we iterate through the list (not a task struct in the thread
pool anymore, but set aside in a retired list) and if any of them have snapshots
that fully are different than the current state, then we can free that box. Does
that make sense? Is it congruent?


--------------------------------------------------------------------------------

### Assistant Response 34

Let me answer the factual part first, then check congruence.


## Where the pointers live — three places, not one


That's the part that decides whether your design works, so:


| where | what it holds | when it's set |

|---|---|---|

| **the registry** | `box_info` — name, shim, param widths, return width, task size, compare | at build time today; grown at runtime under this issue |

| **the station** | `task_call_t call` — the shim itself, copied out of the registry | at placement, so delivery does a call rather than a lookup |

| **the task** | `task_call_t call` — copied again from the station | when readiness builds the task |


So the shim pointer gets copied twice on its way to being called. `registry →
station.call → task.call → worker`.


**That's why removing it from the registry isn't sufficient on its own.** It
stops new *placements*. It doesn't stop a station that already copied the
pointer, and it doesn't stop a task already built and sitting in the ring
carrying its own copy.


## Yes, it's congruent — you re-derived the same mechanism


Including the move that matters most: noticing you can't spin waiting for
counters to change, and putting the sweep at the *front of the next teardown*
instead. That's exactly what the destination-array design does — "any later
rewiring sweeps the retire list first." Nothing waits, nothing spins, and the
work happens when there's naturally a reason to be there.


**One piece is missing, and without it nothing ever gets freed.** "Free it once
all the worker counts have changed" deadlocks against an idle pool: a sleeping
worker's count will never change, because it isn't doing anything. The test has
to be **changed *or* currently not running anything**, and a sleeping worker
passes on the second clause without having to move.


**Which is why the counter is odd-while-working rather than a plain count.** One
number answers both questions at once — its *parity* says whether the worker
is inside a task right now, its *value* says which task. A plain count only
answers the second. Bump it when a task begins and again when it ends: odd means
inside, even means between.


## The thing your design unlocks, which is bigger than unloading


I recorded earlier that you can only unload a box no station uses, because a
station can't be removed. That's true, and I think it's the less interesting
half.


**Because `station.call` is just a pointer, it can be overwritten.** Which means
you can *replace* a box's code in a running program: compile the new version,
write the new shim into every station using it, retire the old library, sweep,
free. Stations keep their identity, their wires, their buffers, and their
counters — they just start running different code. In-flight tasks finish on
the old code, which is exactly what the sweep is for.


That's a live-editing capability, and it's arguably the whole reason to compile
boxes at runtime. Unloading a box nobody placed is housekeeping; **fixing a box
while the program keeps running is the feature.**


It needs one thing the engine doesn't have: a way to say "this station now runs
that box," with a check that the widths still match. Everything else — the
pointer swap, the retire list, the counters — is what you just described.


Do you want replacing a box's code in scope for this issue, or kept as its own
thing?


--------------------------------------------------------------------------------

### User Request 35

  One piece is missing, and without it nothing ever gets freed. "Free it once
  all the worker counts have changed" deadlocks against an idle pool: a sleeping

  worker's count will never change, because it isn't doing anything. The test
  has to be changed or currently not running anything, and a sleeping worker
  passes

  on the second clause without having to move.



well, freeing a box we no longer need doesn't block any worker threads, so that
concern should be fine?


  I recorded earlier that you can only unload a box no station uses, because a
  station can't be removed. That's true, and I think it's the less interesting

  half.


whoa whoa whoa, this makes me thing there's a misunderstanding about what "box"
and "station" means - can you clarify? Do you mean "box function"?


  Because station.call is just a pointer, it can be overwritten. Which means you
  can replace a box's code in a running program: compile the new version, write

  the new shim into every station using it, retire the old library, sweep, free.
  Stations keep their identity, their wires, their buffers, and their counters
  —

  they just start running different code. In-flight tasks finish on the old
  code, which is exactly what the sweep is for.



if the task list is 400 tasks long, and we edit a box, then the task could get
picked up with a stale pointer.


● Where the pointers live — three places, not one


  That's the part that decides whether your design works, so:


  ┌──────────────┬───────────────────────────────────────────────────────────────────────┬────────────────────────────────────────────────────────────┐

  │ where │ what it holds │ when it's set │

  ├──────────────┼───────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ the registry │ box_info — name, shim, param widths, return width, task
  size, compare │ at build time today; grown at runtime under this issue │

  ├──────────────┼───────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ the station │ task_call_t call — the shim itself, copied out of the
  registry │ at placement, so delivery does a call rather than a lookup │

  ├──────────────┼───────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ the task │ task_call_t call — copied again from the station │ when
  readiness builds the task │

  └──────────────┴───────────────────────────────────────────────────────────────────────┴────────────────────────────────────────────────────────────┘


  So the shim pointer gets copied twice on its way to being called. registry →
  station.call → task.call → worker.


  That's why removing it from the registry isn't sufficient on its own. It stops
  new placements. It doesn't stop a station that already copied the pointer, and

  it doesn't stop a task already built and sitting in the ring carrying its own
  copy.



can you explain this whole thing again?


--------------------------------------------------------------------------------

### Assistant Response 35

You're right on the third point and it breaks what I proposed. Let me do the
vocabulary first, then the chain, then your catch.


## Box versus station


Yes — **"box" means the box function.** A box is a C function somebody wrote:


```c

int add(int a, int b) { return a + b; }

```


That's all a box is. Code.


**A station is a place in the running program where a box is installed.** It has
input slots that fill up, an output port with wires leaving it, a mutex, and
counters. Stations are what a map file lists and what wires connect.


**The same box can be installed at many stations.** One `add` function, and
stations called `total`, `subtotal`, and `tally` all running it. The box is the
recipe; the station is a kitchen using it. Many kitchens, one recipe.


So when I said "you can only unload a box no station uses," I meant: you can
only free the compiled machine code of a function when no place in the graph is
set up to call it.


## The pointer chain, slowly


When you compile a box, you get machine code somewhere in memory. A **shim** is
a small generated function that knows how to call it — it pulls the arguments
out of a task and passes them in. What gets copied around is the address of that
shim.


It gets copied twice:


**1. The registry** holds it, filed under the box's name. This is the phone
book: "the box called `add` lives at this address, takes two 4-byte arguments,
returns 4 bytes."


**2. When a station is placed**, it looks the name up in the registry and
**copies the address into itself** — `station.call`. It does this once, at
placement, so that later on delivery does a call instead of a phone-book lookup
every time.


**3. When a station's inputs are full**, a task gets built, and the task
**copies the address again** — `task.call`. The task is then completely
self-contained: it holds the shim address, copies of all the input values, and
room for the result. A worker can pick it up and run it without consulting
anything else.


That third copy is deliberate and it's a stated guarantee: *a task is
self-contained once built; nothing another thread can change affects it.*


**So: phone book → station → task → worker runs it.** Three copies of one
address.


## Your 400-task catch is correct


Here's the failure, concretely.


Station `total` runs box `add`. Four hundred tasks are queued, and thirty of
them are for `total`, each carrying the address of the **old** `add` shim.


Now you edit `add` and swap the new address into `station.call`. Every *future*
task gets the new address. **The thirty already in the queue still hold the old
one.**


The sweep asks: has each worker's counter moved since the swap? Workers are
busily chewing through other tasks, so yes, they've all moved. The sweep
concludes nobody is inside the old code and frees it.


Then a worker reaches one of those thirty tasks and calls a freed address.


**The counter test proves "no worker is inside the old code *right now*." It
does not prove "no queued task will enter it later."** That's the whole gap, and
it's why the destination-array mechanism doesn't transfer: a destination list is
*read at delivery time and never captured*, so once a walk ends, nothing
remembers it. A shim address gets captured into a task and sits in a queue.


## And a correction on the idle-pool point


I said "without the even-check, nothing ever gets freed." That was overstated. A
sleeping worker's counter doesn't move, so the sweep just doesn't free *yet* —
and frees later when the pool does more work. It's deferred, not broken, and
everything still filed at teardown gets freed there. The even-check makes
reclamation prompt rather than making it possible. You were right to push back.


## Where that leaves replacing a box


Four ways out, and I don't think any is obviously right:


1. **Take the shim out of the task.** The task already carries its station's
   index, so a worker could read `station.call` at the moment it runs. Then
   there's one authoritative copy and a swap takes effect for every queued task
   at once. **Cost: it breaks the self-contained-task guarantee outright**, and
   that guarantee is load-bearing — it's what makes a task's run time equal
   its box's run time with nothing hidden riding along.

2. **Free the old code the first time the pool goes fully idle after a swap.**
   An empty ring with every worker asleep proves no queued task holds the old
   address. Uses machinery that exists. Costs: a pool that never goes idle never
   frees.

3. **Retire and never free.** Every version of every box stays loaded for the
   life of the program. Correct, three lines, and a workbench where somebody
   edits a box two hundred times holds two hundred small libraries.

4. **Don't replace boxes at all** — keep unloading limited to boxes no station
   ever used, which was the original scope, and this whole problem disappears.


My read: option 2 is the most honest, and option 4 is the most defensible if
you'd rather not pay anything yet. Option 1 buys the cleanest behavior at the
cost of a guarantee I'd be reluctant to spend.


Which way?


--------------------------------------------------------------------------------
