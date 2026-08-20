# Conversation Summary: 3ccc0eb0-a576-4361-85ec-79ae4bb357a4

Generated on: 2026-08-20 11:57:00

--------------------------------------------------------------------------------

### User Request 1

hi can you work on this project?


--------------------------------------------------------------------------------

### User Request 2

hi can you identify unanswered questions, and then ask me?


--------------------------------------------------------------------------------

### Assistant Response 2

I'll go find them. Let me sweep the documentation for open questions.


API Error: The response stopped arriving. The response above may be incomplete.


--------------------------------------------------------------------------------

### User Request 3

please continue


--------------------------------------------------------------------------------

### Assistant Response 3

Found 22 files carrying open questions. Let me read them all.


Now let me sweep the docs and notes for questions phrased differently.


I swept every issue blueprint, every document, and the notes. Here's what I
found.


## The sweep


22 files carry an "Open questions" section. Most of those questions are already
**answered** — the section is kept because the reasoning is the design. Seven
questions are genuinely unanswered:


| Where | The question |

|---|---|

| The port record | How does a map file spell a port that has been given no source yet? |

| The port record | How does a map file spell a port's starting buffer depth? |

| Optional parameters | Can a map file write a *present* value into an optional port as ordinary text? |

| Optional parameters | Can a parameter be made absent again after it has been given a source? |

| The lockless claim | Does the deliberately-broken livelock test stay in the tree, or become a note? |

| One way to build a station | Does placing a station by hand — without consulting the registry — survive at all? |

| The demos after the pull path | The numbering convention communicates nothing to a reader who wasn't told the rule. What fixes that? |


One thing worth flagging separately: **`docs/` and `notes/` contain no open
questions at all.** Every question in this project lives in an issue blueprint.
Your own instruction says questions should be spread so that every piece of
documentation gets touched — the ten datapath documents, the guarantees page,
and the first-pass report have not been asked anything in a long while. That's a
gap I can fill after we work through these.


Now, one at a time.


## Question one: spelling a port with no source


Background, since this depends on a mechanism you may not have in mind right
now. A **station** is one placement of a C function in a map, and each of its
input ports carries a tag with three possible values: *ring* (a buffer values
arrive into), *static* (a constant sitting on the port), and *none* (nobody has
said where this port's value comes from). A station holding a *none* port can
never run, no matter what arrives at its other ports. That's what lets you build
a program out of nothing and watch it become runnable one wire at a time.


The map file has words for the first two tags and no word for the third. Today
the dump writes a comment saying the format cannot spell it — which keeps the
dump honest and breaks the round trip, so a half-built program is currently the
one thing a dump cannot promise to reload.


The existing forms on an `in` line are `$0` (fetch the text at statics entry
zero) and `= 5` (the constant is right here on the line). Whatever spells
*unconfigured* has to visibly be neither of those, so a reader never has to
guess whether somebody meant a value.


**[Asked the user]**


*Q1 — Absent port:* How should a map file spell an input port that has been
given no source yet?

- A bare dash — No value in this format is ever a lone dash, so it cannot be
  mistaken for one. The dash already means "wire" on an out line, so this reads
  as a wire that is not there yet. Costs one character and reads quietly.

- The word none — Says exactly what the tag is called in the engine, so the
  file and the source use one vocabulary. Unmistakable to a stranger reading the
  file cold. Costs four characters and could collide with a future box or type
  named none.

- A question mark — Reads as an unanswered question rather than as an absence,
  which is arguably what a half-built program is. Visually loud — you can find
  every unfinished port by eye. Nothing else in the format uses it.

- Omit the line entirely — A port with no line is unconfigured; the format
  already writes only exceptions, and this makes absence the default rather than
  a spelling. Costs the dump its explicitness — you cannot tell a forgotten
  port from a deliberately empty one.

→ **Answered:** A bare dash" selected preview:

station adder = add_two_ints

  in 0 $0          port 0 reads statics entry 0

  in 1 -           port 1 has no source yet

  out 0 - printer.0


station printer = print_int

  in 0 - nothing wired here either. You can now continue with these answers in
  mind.


Recorded — a bare dash. I'll write all the answers into the blueprints in one
pass at the end, so the files don't churn.


## Question two: spelling a port's starting buffer depth


Same line, different thing. Every input port owns a **ring buffer** — a fixed
run of cells, each sized to hold exactly one value of that port's type. A port
carrying four-byte integers starts at ten cells, so forty bytes. When it fills,
growth adds another page of the same size, so guessing low costs a slower
startup and nothing else.


Ten is a magic number and is meant to be one. But a port may be *told* its own
starting depth, and that call exists in C today with no way to say it in a file.
It's a hint rather than a setting — growth covers being wrong, so nobody has
to be right.


The wrinkle: depth is independent of where the port's value comes from. A port
holding a constant still has cells sitting idle, because that's what makes
changing a port's source a single field write instead of an allocation. So a
depth can sensibly appear beside `$0`, beside `= 5`, and beside the dash you
just chose.


**[Asked the user]**


*Q1 — Port depth:* How should a map file say that one port starts deeper than
the default ten?

- A word and a number — Self-describing — a stranger reads it correctly with
  nothing explained. Costs five characters on a line that will almost never
  carry one, which is the right place to spend them.

- A multiplication mark — Compact, and reads as "sixty-four of them" the way a
  parts list would write it. Matches the terseness of the rest of the format.
  One more sigil for a reader to learn, alongside the dollar sign.

- A bare trailing number — Nothing new to learn; the third field on an in line
  can only be one thing. But it does not say what it is, so a reader meeting `in
  0 $0 64` has to be told, and a wrong guess is silent.

- Its own line — Keeps the in line about one thing only — where the value
  comes from — and lets depth be stated for a port whose source you have not
  decided. Costs a line per tuned port and a second place to look.

→ *(no answer recorded)*


Noted — the multiplication mark, with both `x64` and `*64` accepted by the
reader. One clarifier on that, because the dump has to pick one:


The **dump** is the tool that walks the live station table and writes out what
is actually there, and the round trip — dump, load, dump — has to produce
identical text. So while the reader can accept both spellings, the writer emits
exactly one. Worth noting that `x64` is also the name of a CPU architecture,
which in a systems project is a string that turns up in other contexts; `*64`
collides with nothing.


**[Asked the user]**


*Q1 — Canonical mark:* Which depth spelling does the dump write, given the
reader accepts both?

- The star — No collision with anything else a systems project writes. The
  letter form stays accepted for hand-written files, so nothing you type is
  refused — it just comes back as a star.

- The letter — Easier to type, no shift key, and reads as "times" to most
  people on sight. Accepts the star form for anyone who prefers it.

→ *(no answer recorded)*


Retracted, understood — only `x64`, one spelling read and written. That's more
in keeping with the rest of the project anyway.


## Question three: writing a value into an optional port


An **optional parameter** is one a box can do without. The plan is that its type
becomes a small generated wrapper — a struct holding the value plus a presence
flag beside it — rather than a reserved "impossible" value. The reason is that
a box returning an integer has no spare number that could honestly mean
*deliberately absent*; every bit pattern is a quantity somebody might mean. A
reserved value is a convention the author has to learn, remember, and never
violate, and this engine's standing promise is that you can bring existing C
unchanged.


Your dash answer already settled half of this: an optional port left at the dash
hands the box a wrapper whose flag says absent. That falls out for free — the
dash means "no source yet," and for an optional parameter that isn't an error,
it's the answer.


What's left is the other half. When a file wants to put an actual value in, does
it write the value plainly and let the reader wrap it, or does it have to spell
the wrapper?


The struct form is already in the format — `in 2 = { 1.5, 2.5, 3.5 }` writes a
struct constant onto a port — so spelling the wrapper costs no new syntax. It
does let the file express a contradiction, though: a flag saying absent sitting
next to a value.


**[Asked the user]**


*Q1 — Optional value:* How does a map file put a present value into an
optional port?

- Plainly, reader wraps it — The file never mentions the wrapper at all.
  Nothing contradictory can be written, because the flag is not yours to set —
  a value present means present. Cost: the text on the line is an int while the
  port's declared type is a wrapper, so file and registry disagree on paper.

- Spell the wrapper — Uses the struct constant syntax that already exists, and
  the file says exactly what sits in memory. Cost: `{ 0, 5 }` is writable and
  means nothing — absent, with a value — so the reader must refuse a form
  the grammar permits.

- A word before the value — Says out loud that this port is an optional one,
  so a reader meeting the line learns something the plain form hides. Cost: a
  word that appears nowhere else, and it is derivable — the registry already
  knows the parameter is optional.

→ *(no answer recorded)*


That's the right question to ask, and the answer is: **no, C has no such thing,
and nothing in C is quite this.** Let me be precise, because it changes the
verdict.


Let me check what routing already gives us before I answer the second half.


## Answering that directly


**C has no optional parameters.** No version of the language has default
arguments or overloading. What C programmers actually do, in rough order of how
common it is:


| Idiom | How it works | Why it isn't this |

|---|---|---|

| A nullable pointer | `strtol(s, NULL, 10)` — the third-party pointer may be null | Works only because a pointer type has a genuine spare bit pattern. Null can never be a valid object address. An `int` has no such spare value. |

| A reserved in-range value | `-1` for "no timeout", `0` for "default" | Costs you a number you might have meant. The author must learn and never violate the convention. |

| A zero-initialized options struct | `(struct opts){.timeout = 5}` — unset fields are zero, zero means default | The modern idiom, and it is **exactly the little struct you just described**. The caller writes it; the language does nothing. |

| Two separate functions | `foo()` and `foo_with_extra(x)` | No mechanism at all. |

| Varargs with a terminator | `execl(path, arg, ..., NULL)` | Untyped, and dreadful for anything but strings. |


So the wrapper-with-a-presence-flag isn't a C idiom being imported. It's a thing
from other languages that C approximates with the options struct — which the
user writes by hand, at no cost to the platform.


**Your instinct is right, and the reason is stronger than the one you gave.** It
isn't a check inside the box. The mechanism was going to change **the readiness
rule** — the one sentence the entire runtime is built on:


> A station runs when, and only when, every one of its input slots holds a
> value.


Optional parameters would have been the only exemption to that sentence: the
readiness check learning to answer *satisfied* for a slot with nothing in it.
Nothing polls in this engine, nothing scans; the check is the tail end of a
write, and every part of that is simple because the rule has no exceptions.
Spending the design's one invariant on a convenience C itself does not have is a
bad trade, and it is a worse trade at platform level than anywhere else, because
everyone downstream inherits the exception forever.


**And the case the blueprint was actually worried about is on the other side of
the box anyway.** Its motivating example was a box that reads a file and cannot
decline to produce a value when the file is missing — but that's about a box's
*output*, not its input, so optional parameters would never have fixed it. The
engine already answers it in its own vocabulary: the box returns a struct
carrying a validity flag beside the bytes, and a **comparator** station
downstream — which has three output ports, chooses one by comparing the
returned value against a threshold sitting in an extra input slot, and is
already built — sends success down one wire and failure down another. Absence
becomes a value that gets routed, rather than a hole in readiness. That's your
little struct, doing the whole job, with nothing added to the platform.


Deleting it also cleans up three things: the type registry stops gaining wrapper
entries nobody wrote, the dump stops needing to name them, and the
configuration-time check gets simpler — a port with no source feeding a
station is an error, with no exemption to carry.


One thing I won't decide for you: where the record goes. Your rule is that
tickets are never deleted, and this project already has a home for a rejected
mechanism — `056-no-pull-path.md`, a document whose whole job is explaining
why a removed capability isn't there.


**[Asked the user]**


*Q1 — Disposition:* Where should the record of the rejected
optional-parameters design live?

- An implementation note — Becomes a numbered document beside the no-pull-path
  note, titled for why there are no optional parameters, carrying the C survey
  and the readiness-rule argument. The blueprint moves to completed as the thing
  that produced the note. This is the pattern the project already uses for a
  removed capability.

- Rewrite the blueprint in place — The ticket stays in issues/, its intended
  behavior replaced by the decision not to build it, then moves to completed. No
  new document. Cheaper, but the reasoning is buried in a ticket nobody browsing
  the docs will meet.

- Fold it into the port record — The none tag's blueprint gains a section
  saying absence is never handed to a box, and the ticket moves to completed
  pointing at it. Keeps the reasoning next to the mechanism it constrains, at
  the cost of making an already long ticket longer.

→ **Answered:** Rewrite the blueprint in place" selected preview:

docs/implementation-notes/

  056-no-pull-path.md

  057-packaging.md


issues/completed/

  210h-optional-parameters.md     <- moved, rewritten

                                     to record the refusal. You can now continue with these answers in mind.


Recorded. Three questions dissolved with that one — the optional-value
spelling and the make-absent-again question both go with it.


## Question four: the test that has to fail first


Let me read the surrounding design so I can put this to you accurately.


This one is about how you prove a concurrency fix actually fixes something.


The hazard: at a station with two inputs, a worker claims a value from each port
in turn, and rolls back what it claimed if any port has nothing ready. Without a
fixed order, two workers can claim one port each, each fail on the other's port,
each roll back, and retry into the same interleaving forever — nobody blocked,
nobody progressing, and a complete set of inputs sitting there the whole time.
Claiming in **ascending port order** fixes it: both workers reach for the same
port first, one wins outright, and the loser fails at step one having claimed
nothing.


The trouble is proving it. A concurrency test that passes proves nothing on its
own — the interleaving it was written to catch may simply never have occurred
on that run, on that machine, that day. The only way the test means anything is
if the same test has been run **once with the ordering rule deliberately
reversed and seen to stall.** Otherwise it's decoration that everyone trusts.


So: what happens to that reversed version?


**[Asked the user]**


*Q1 — Livelock proof:* How does the deliberately-broken ordering survive as
proof that the livelock test works?

- A second build, run every time — The reversed order lives behind a compile
  switch. The test suite builds that binary separately, runs it, and expects it
  to stall. The proof is live — it re-proves itself on every machine — and
  the wrong order never exists in the real engine. Costs a second compile and a
  timeout in the suite.

- A disabled test in the tree — The reversed version sits beside the real one,
  marked not to run. Anyone curious can switch it on and watch it stall. Costs
  the usual fate of code nobody runs: in two years it stops compiling and
  somebody deletes it without knowing what it was for.

- A note only — A paragraph recording that the reversed run was done, what was
  seen, and on what machine. Nothing in the tree to rot. Costs verifiability
  entirely — the claim rests on somebody's word, and a later reader cannot
  re-check it.

- One test that runs both ways — The order becomes an argument to the test,
  and the engine keeps both paths. Runs every build with no second compile.
  Costs the most: the wrong order stays reachable in the shipped engine forever,
  which is exactly the kind of live-but-wrong path this project treats as an
  error.

→ *(no answer recorded)*


Good catch on the premise — and the answer is the interesting part.


**Each individual claim is atomic. The set of them is not.** A single cell flips
from *ready* to *claimed* with one compare-and-swap, which is genuinely
indivisible. But a station with two inputs needs **two** cells, one from each
port, and there is no instruction that compare-and-swaps two separate addresses
at once. So the walk is: CAS a cell in port 0, then CAS a cell in port 1.
Between those two instructions, everything can happen.


That's the whole hazard in one sentence: **atomic operations do not compose.**
Two atomic steps in sequence are not one atomic step, and the thing that has to
be all-or-nothing here is the station, not the cell.


Here's the concrete interleaving. Station with ports 0 and 1, exactly one value
ready in each:


| | Worker A | Worker B |

|---|---|---|

| 1 | CAS port 0's cell → **wins**, holds it | CAS port 1's cell → **wins**, holds it |

| 2 | looks at port 1 → nothing ready | looks at port 0 → nothing ready |

| 3 | rolls back: flips port 0's cell to ready | rolls back: flips port 1's cell to ready |

| 4 | retries from port 0 | retries from port 1 |

| 5 | …identical to row 1 | …identical to row 1 |


Nobody is blocked — that's what separates this from deadlock. Both workers are
running flat out, and every CAS they issue *succeeds*. They're just undoing
everything they accomplish. And a complete input set is sitting in that station
the entire time; a third worker arriving would build a task from it instantly.


**The roll back is forced, not a choice.** A worker holding port 0's value that
can't get port 1's cannot simply keep it — that would strand the value
permanently and block every other worker from ever building a task at that
station. So it must put it back, and putting it back is what creates the retry.


**Ascending port order removes it by moving the contest to a single address.**
If both workers always reach for port 0 first, one wins that CAS and the other
loses it — and a CAS at one location has exactly one winner, always. The loser
fails at step one having claimed nothing, so it never took port 1's value away,
and the winner walks on to port 1 and finds it ready. This is the same medicine
as lock ordering for deadlock: the failure comes from acquiring several things
in inconsistent orders, and a canonical total order on the resources kills both
diseases. The difference is only whether you wait (deadlock, a hang) or back off
and retry (livelock, a spin).


**One correction to how I framed the test.** Indefinite livelock is rare in
practice, because the operating system deschedules threads at unpredictable
moments and the symmetry breaks by luck. What actually happens is *repeated
episodes* — bursts of workers undoing each other under contention. That shows
up as **throughput collapse**, not as a hang. Which is better news for the test
than a stall would be: completed-invocation counts are already gathered per
station, so the test can compare work completed against claims attempted and
needs no clock at all.


So, re-asking with that in mind:


**[Asked the user]**


*Q1 — Livelock proof:* How does the deliberately-reversed claim order survive
as proof that the livelock test can actually catch something?

- A second build, run every time — The reversed order lives behind a compile
  switch. The suite builds that binary separately and expects its wasted-work
  ratio to blow up. Re-proves itself on every machine, and the wrong order never
  exists in the shipped engine. Costs a second compile.

- A disabled test in the tree — The reversed version sits beside the real one,
  not built. Anyone curious switches it on and watches the ratio collapse. Costs
  the usual fate of unrun code: it stops compiling eventually and gets deleted
  by someone who never learned what it was for.

- A note only — A paragraph recording that the reversed run was done, the two
  ratios seen, and on what machine. Nothing in the tree to rot. Costs
  verifiability — a later reader takes it on faith and cannot re-check it.

- One test, both orders — The order becomes an argument, the engine keeps both
  paths, and the test asserts the contrast every build with no second compile.
  Costs the most: a knowingly-wrong claim order stays reachable in every shipped
  binary, which is the kind of live-but-wrong path this project treats as an
  error.

→ *(no answer recorded)*


You're right, and it's better than "unnecessary" — it's **provable**, which is
a stronger thing to have than a test.


Ascending order is exactly what the design specifies; nothing here is proposing
otherwise. What I got wrong was treating the stress test as the way to establish
the property. Here's the proof it doesn't need one.


**Setup.** A worker claims cells in ascending port order and rolls back at the
first port where nothing is ready. Note a port can hold *several* ready values
at once, so two workers can both succeed at port 0 on different cells — the
contest isn't as simple as one winner per port.


**Claim: if a complete input set exists at a station, some worker completes a
task.**


Take any worker A that rolls back at port *k*. Why was port *k* empty when A
looked? Exactly two possibilities:


- **Nothing ever arrived there.** Then no complete input set exists —
  contradicting the premise.

- **Another worker B took the last ready cell at port *k*.** Then B has claimed
  ports 0 through *k* and is strictly ahead of A.


In the second case B either finishes — progress, done — or B itself rolls
back, and it can only roll back at some port *j* where *j > k*, because B
already passed *k* successfully. So every rollback hands the debt to a worker
standing at a **strictly higher port index**. Ports are finite, so the chain
cannot continue forever; the worker at the end of it has nobody ahead of it, so
its failure must be case one — nothing ever arrived — which contradicts the
premise. Therefore somebody completed.


That's lock-freedom in the technical sense: not every worker makes progress, but
the system always does. It holds for **every** interleaving, which is precisely
what a test cannot do — a test samples interleavings, and there is no number
of samples that covers them.


This project already accepts exactly this kind of reasoning in the same family
of blueprints. The question "what if a writer dies mid-copy and strands a cell
forever?" was answered not with detection or reclamation but with *nothing that
can die is able to get into that window* — a structural argument, no
mechanism, no test. Same shape.


**What the proof does depend on, and what that means for the tree.** The
argument rests entirely on one premise a future edit can quietly break: that the
walk really is ascending, everywhere it happens. The plausible regression isn't
someone reintroducing livelock on purpose — it's someone adding a well-meaning
fast path ("start at the port that was ready last time," "check the cheapest
port first") without knowing the order was load-bearing. That's a much smaller
thing to guard than a concurrency hazard, and it's guardable deterministically,
on one thread, with no clock.


**[Asked the user]**


*Q1 — What replaces it:* Given the proof, what goes in the tree in place of
the livelock stress test?

- The proof, plus a cheap order guard — The induction is written where a
  future optimizer will meet it, and one single-threaded test records which port
  indices a claim walk visits and asserts they only ever increase. No threads,
  no clock, no timing. Catches the one regression that could break the proof.

- The proof alone — The induction goes in the guarantees page and a comment
  sits at the walk. Nothing in tests/. Leans entirely on the reasoning being
  read, which is what the project does elsewhere for structural arguments —
  the stranded-cell question was answered exactly this way.

- The proof, and keep a stress test too — Both: the induction written down,
  the order guard, and the many-threaded test kept as a smoke check that the
  whole path survives contention. It proves nothing about livelock and is not
  claimed to — it is a general concurrency shakeout that happens to hammer
  this walk.

→ *(no answer recorded)*


Let me get the actual removal rationale rather than reconstruct it.


Yes — that's exactly the case, and you've found the one that matters. Port A
holds two ready values, port B holds one. Both workers win a cell at port A
(different cells, both CAS operations succeed honestly). One goes on and takes
port B's value. The second arrives at port B and finds nothing.


**What happens to its port-A value: it rolls back.** It flips that cell from
*claimed* back to *ready* and gives up. Nothing is lost — the value returns to
the pool of ready values and sits there until a second value reaches port B, at
which point whoever asks next takes it.


And the reason that's tolerable rather than a bug is the thing your example
demonstrates: **the first worker completed.** Real work got done. The second
worker's wasted trip was paid for by the first worker's task. That's the whole
induction — a rollback at some port means somebody else got strictly further,
and "further" is bounded by the port count, so the chain always ends at someone
who finished.


Now, your three questions about mutexes.


## Why they came off the hot path


First, precisely: **they didn't come off entirely.** Every station still owns a
mutex and still uses it — for four operations, all rare and all structural:
growing a buffer, rewiring, writing a constant onto a port, and changing what a
port is. What left is the mutex's role in the *claim*, which happens on every
single delivery.


The reason is fan-in. Guarantee S5 in the guarantees page reads *"a station's
ports are read and written only under that station's mutex — every delivery
takes exactly one lock."* At a station where twenty upstream stations converge,
all twenty deliveries serialize on that one lock, one at a time, no matter how
many cores the machine has. The engine's entire promise is that a shape executes
itself across every core — and that promise breaks down precisely at the
places where the graph converges, which are the interesting places.


**And here is the part I should say plainly rather than let you discover: the
payoff has not been demonstrated.** The measured result so far, on the fan-in
apparatus, is that the new machinery made small values roughly **half again
slower** per delivery. That's because the scan replaced index arithmetic with a
compare-and-swap per candidate cell, and those costs are being paid *while the
old lock is still held* — the removal that was supposed to buy them back
hasn't happened yet. The blueprint states the number as the one to beat and says
outright that if the figure doesn't come back down, something other than the
lock was the problem. So your instinct to reconsider is not fighting the
evidence; it's the only position the evidence currently supports.


## Can you lock several addresses in one atomic operation?


**No, not in any way you can build on.** Three things exist and none of them is
it:


| Mechanism | What it actually does | Why it doesn't help |

|---|---|---|

| Double-width compare-and-swap (`cmpxchg16b` on x86-64, `LDXP`/`STXP` on ARM64) | Atomically swaps **16 contiguous bytes** — two adjacent 8-byte words | The two locations must be next to each other. N ports scattered across N port records are not. |

| Hardware transactional memory (Intel TSX, ARM TME) | Exactly what you're asking for: begin, touch several locations, commit or abort atomically | Intel disabled TSX in microcode across most parts after the side-channel vulnerabilities; ARM's version is specified but not broadly shipping. Not something to depend on. |

| Software multi-word CAS | Builds N-location atomicity out of single-location CAS using descriptor objects and cooperative "helping" | More atomic operations than the thing it replaces, and considerably more complexity, for small N. Strictly worse than a lock here. |


So the honest answer is that for N scattered words you take N locks, or you do N
separate atomic operations. There is no third door.


## Your proposal, evaluated seriously


Take the mutexes, flip the N claim flags under them, release, *then* do the
expensive copies outside. Three things about it:


**The split you describe is already the design.** That's what the per-cell state
machine exists for — it was built specifically so the claim (flipping a state,
cheap) could be separated from the copy (moving bytes, potentially large). The
blueprint that built it says it discovered it could not move either copy out of
the lock until cells carried their own states. You've independently arrived at
the same structure.


**But taking N mutexes needs the exact same ordering rule, for the exact same
reason.** Acquiring multiple locks in inconsistent orders is the textbook
deadlock. Ascending port order fixes it. So the ordering rule isn't something
lock-freedom imposed on us — it is what *any* design needs when one worker
must hold several things at once. Adding mutexes doesn't remove the rule; it
converts the failure from a **spin** into a **hang**, which is worse. A spin
burns a core visibly and recovers the moment contention clears. A hang is
permanent and needs a debugger.


**And there's a real argument in your favour that I want to put on the table
properly.** Count the atomic operations. Lockless: one CAS per port, so N.
Mutex: one CAS to acquire, N plain non-atomic writes, one release — call it 2.
**For a wide station, the mutex does fewer atomic operations than the lock-free
version.** That's a genuine advantage and it's counterintuitive enough to be
worth measuring.


What the mutex costs in exchange is concurrency at a single station: under a
lock, exactly one worker builds a task at a time there, always. Without it, when
every port has several ready values, several workers build several tasks at
once. Your two-in-A-one-in-B example is the case where that doesn't pay — but
a station with ten ready values in every port is the case where it does.


So: **fewer atomic operations, no parallelism** versus **more atomic operations,
parallelism proportional to available values.** Which wins is a question about
how deep the buffers actually run at contended stations, and that's empirical.
The fan-in apparatus already exists and already produced the baseline number.


This changes the shape of what I was asking you about. The livelock-test
question is now downstream of a larger one:


**[Asked the user]**


*Q1 — Claim strategy:* Given the lockless claim has so far only cost
performance and not yet repaid it, what should happen to that line of work?

- Finish it, then measure, then decide — Complete the removal so the lock
  actually comes off, re-run the fan-in apparatus, and compare against the
  baseline already recorded. If it does not beat the mutex, the mutex comes back
  with the measurement as the reason. Costs finishing work that may be reverted
  — buys the only number that settles it.

- Build both and measure both — Two claim paths behind one switch, measured
  head to head at several buffer depths and fan-in widths, since the answer
  plausibly differs between them. Most expensive and most conclusive. Risks the
  shipped engine keeping a switch, unless the loser is deleted once the number
  is in.

- Revert to the mutex now — Take the measured slowdown at face value, restore
  the single-hold claim, keep the per-cell states and the ordering rule since
  both are needed either way. Cheapest, and honest about current evidence. Costs
  the fan-in ceiling: one delivery at a time at every converging station,
  forever.

→ *(no answer recorded)*


Let me confirm the fourth state before I answer, since it matters to whether
this works.


The fourth is **reserved** — a writer has taken an empty cell and is copying a
value into it, not yet published. Full machine:


| state | meaning | who may touch the value |

|---|---|---|

| empty | nothing here | whoever wins the CAS to reserve it |

| reserved | a writer is copying in | that writer only |

| ready | holds a complete value | whoever wins the CAS to claim it |

| claimed | a reader is copying out | that reader only |


## Your design works, and the ownership table is why


Two of the four states — *reserved* and *claimed* — mean **exactly one
worker owns this cell and no other worker may look at it.** That's already
written into the header. So a cell in *claimed* is private property: its bytes
cannot be read, written, or moved by anyone but its claimer, and no lock is
needed to protect something nobody else may touch.


That's the whole justification for your split, and it's airtight:


- **Under the lock:** scan the ports, flip one cell in each from *ready* to
  *claimed*. N integer writes plus the scan to find them. No bytes move.

- **Outside the lock:** copy the claimed values into the task struct, then flip
  each cell to *empty*. This is where all the expense lives — a
  two-hundred-byte struct per port — and it runs fully parallel, because every
  worker doing it owns cells nobody else can reach.


So yes, to answer your question directly: **mutexing the claim does not prevent
copying values out.** Several workers copy out simultaneously while a different
worker holds the lock doing its flips. The serialized part is the bookkeeping;
the byte-moving is not serialized at all.


**And there's a second win you may not have counted.** Writers delivering values
don't need the lock either. A delivery is a *single*-cell operation — reserve
one empty cell, copy in, publish it ready — and single-cell operations are
already atomic via one CAS. Only the claim needs group atomicity, because only
the claim must take several cells at once. So under your design, deliveries into
a station never serialize; only task construction does.


**One honest caveat about "just setting some integers."** The flips are
integers, but the *scan* is under the lock too — walking a port's cells
looking for one in *ready*. Each port keeps a bookmark that usually points a
step or two away from the answer, so it's normally short. It is not bounded by
anything except the port's depth, though, so a pathological port makes the
critical section longer than N writes. That's a real cost and I'd rather name it
than have it surprise someone later.


**On your reasoning about measurement — I think you're right, and it's
consistent with how this project already decides things.** The guarantees page
classifies every promise as held by *structure*, *runtime*, or *discipline*, and
structure is plainly the preferred one. A microbenchmark at one fan-in width and
one buffer depth predicts nothing about a graph somebody hasn't drawn yet, and
picking a design by that number means re-picking it every time the workload
changes. Structural choice, stated reason, stick with it.


One thing I need you to settle before I write this into the blueprints, because
it's the difference between one lock and several:


**[Asked the user]**


*Q1 — Lock scope:* What does the claim mutex cover — one lock for a whole
station's cell states, or one per port?

- One per station, covering all its ports' states — A claimer takes one lock,
  flips a cell in every port, releases. No lock ordering needed because there is
  only ever one lock held. Simplest, and the ordering rule stops being
  load-bearing for safety — it stays only as a scan-efficiency habit.

- One per port — Finer grain — two workers claiming at ports that do not
  overlap never contend. But a claimer holds several locks at once, so ascending
  port order becomes mandatory for deadlock safety rather than optional, and
  every port pays for its own lock in memory and cache.

- One per station, but held only per port in turn — One lock, taken and
  released around each port's scan-and-flip rather than around the whole walk.
  Shorter critical sections — but the claim stops being atomic as a group,
  which brings back the roll-back path and the case you worked out with ports A
  and B.

→ **Answered:** One per station, covering all its ports' states" selected
preview:

worker claiming at a 3-port station:


  lock  station.claim_mutex

    port 0: scan -> flip ready->claimed

    port 1: scan -> flip ready->claimed

    port 2: scan -> flip ready->claimed

  unlock


  copy 3 values into the task  (parallel,

  no lock, cells privately owned)

  flip 3 cells -> empty. You can now continue with these answers in mind.


One lock per station, scoped to the cell states. That settles more than it looks
like.


**The roll-back path disappears entirely.** With the whole walk under one hold,
a claimer can *check* every port for a ready cell first and only then flip them
all — because nothing can take a ready cell away in between. Writers don't
need to be excluded: a writer only ever moves a cell empty → reserved →
ready, so it can *add* availability during the check but never remove it, and
only claimers consume, and only one claimer runs at a time. So either all N
ports have a ready cell and the claim succeeds outright, or one doesn't and
nothing was ever claimed. Nothing to undo.


**Which means question four is dissolved, not answered.** No livelock stress
test, no deliberately-reversed build, no induction to write down. The hazard I
was asking you how to test for cannot occur, because no worker can ever hold a
partial claim while another interleaves. Ascending port order survives as a tidy
habit for scan locality, and stops being load-bearing for safety.


**Guarantee T3 survives intact** — *one invocation's inputs are claimed
atomically with respect to each other, all of them, under a single hold* —
which the lock-free direction was going to weaken. **Guarantee S5 needs
rewriting**: it currently says a station's ports are read and written only under
its mutex and every delivery takes exactly one lock. Under your design, cell
*states* are under the lock, value *bytes* are protected by ownership instead,
and a delivering writer takes no lock at all.


One thing I'll flag rather than ask about, because it needs care at build time:
writing a constant onto a port runs the ordinary readiness check on that
station, and both now want the same mutex. That has to be one hold doing both,
not two acquisitions, or a station recursively locks itself.


## Question five: placing a station without the registry


The **registry** is the generated table naming every box the engine knows —
for each one, its parameter types, its return type, and the byte size of each,
all computed by the C compiler as `sizeof` expressions rather than guessed.
Placing a station "by name" means handing over a box's name and letting the
registry supply everything else.


There is also an older path: placing a station by hand, stating the shapes
yourself. It exists because phase 2 built the station table before a registry
existed, and its tests still use it. The blueprint that unifies station
construction into one surface has to decide whether to teach that path the
registry's type names or delete it.


The bind: if those tests must build a registry to run, they've stopped testing
only the station table, and a test that drags in a neighbouring subsystem is a
test that fails for two reasons.


**[Asked the user]**


*Q1 — Hand placement:* Does placing a station by hand — stating its shapes
directly instead of naming a box in the registry — survive?

- Delete it; place by name only — One way to build a station, matching the
  project's one-way habit. Phase 2's tests build a small hand-written registry
  as a fixture. Costs the property that a station-table test tests only the
  station table — a registry bug now surfaces there too.

- Keep it, taught the registry's names — Both paths live: by name for real
  programs, by hand for tests and anything the registry has not heard of. Costs
  a second way to do one thing, which is the exact shape the unifying blueprint
  exists to remove.

- Delete it, and let the tests build stations directly — No hand-placement
  surface, and phase 2's tests stop going through any placement call — they
  construct station records in the test file itself. Keeps those tests
  dependency-free without the engine carrying a second door. Costs the tests
  knowing the record's layout, so a layout change breaks them.

→ *(no answer recorded)*


Genuinely good question, and it reframes the one I asked. Let me check what the
registry actually holds rather than describe it from memory.


The registry exists for exactly one reason, and the build document states it in
one line: **"The map file says `"add"` as text. Something has to turn that into
a function pointer."**


It holds, per box: the name as it appears in a map, the shim pointer, the type
and `sizeof` of each parameter in order, and the type and `sizeof` of the return
value. That buys three things:


- **Text becomes code.** `station adder = add_two_ints` in a file has to find a
  C function.

- **Everything gets sized.** A port's cells are `sizeof` the parameter they
  feed, and a task is one allocation sized exactly for its box. Both numbers
  come from here, emitted as `sizeof` expressions so the C compiler computes
  them and the generator never guesses.

- **Wires get checked.** After compilation there is no type information left
  anywhere in the program. The registry is the last place it still exists, which
  is why wire checking happens at load rather than at build.


And the property that pays for it: **the map file never mentions a type.** If it
did, there would be two sources of truth about what a box takes, and the map
would always be the one that was wrong.


**But here's what I found looking at the station record, and it answers your
question better than any of my options did.** A station holds a shim pointer,
its slot sizes, its return size, and a comparison function — and the header
notes that last one is *"resolved from the registry at placement,"* with the
header itself deliberately kept registry-free. The registry is consulted when a
station is created and **never again**. It is not part of the running engine at
all.


Which means hand placement isn't a second door into the engine. It's the *same*
door, with the caller supplying what the registry would have supplied. By-name
placement is a lookup that fills in the arguments and then does exactly what
hand placement does.


So the honest answer to "why do we need a registry" is: **you need it the moment
a name written in a file has to become a function call.** That's map files, and
map files are the point. If programs were only ever assembled by C code, you
would pass function pointers and `sizeof` expressions directly and never need a
name table at all.


That inverts my question. Hand placement is the primitive; the registry is the
by-name front for it. Deleting hand placement wouldn't remove a way of doing
things — it would hide the primitive behind the lookup and force phase 2's
tests to drag in a subsystem they don't test.


**So unless you disagree, I'd record it this way:** hand placement survives,
explicitly named as the primitive, and by-name placement is documented as
calling it. The unifying blueprint's "one way to build a station" stays true —
there is one construction path — it just has a typed front door and a raw one,
and the raw one is what the typed one is built out of. That costs nothing and
keeps the station-table tests testing only the station table.


Two questions remain from the original sweep. Next one:


## Question six: the numbering nobody can decode


Every file in this project carries an index — `019-station.c`,
`020-delivery.c` — and the numbers run across the whole tree rather than per
directory, so the project reads in one order. The rule works. The problem raised
while numbering the demo runners is that **nothing in the project tells anyone
that's what the numbers mean.** A reader meeting `033-statics.c` beside
`034-gather.c` has no way to learn whether the numbers mean *read these in this
order*, or *these were created in this order*, or *these are related*. The
convention is followed carefully and communicates nothing to anyone who wasn't
told.


**[Asked the user]**


*Q1 — Reading order:* How should the file numbering explain itself to a reader
who was never told the rule?

- A generated reading index — A tool walks the tree, sorts by index, and emits
  a document listing every file in order with a line about each — so the order
  is not just stated but shown, and it cannot go stale because nobody maintains
  it by hand. Costs a generator and a build step.

- A document stating the rule — One short page explaining that indices are a
  reading order across the whole tree, not per directory, and not creation
  order. Cheapest. Costs accuracy over time — a hand-written list of what is
  where goes stale the first time a file is added.

- Both, and the site renders it — The rule stated in prose, the ordered list
  generated beside it, and the documentation site turns the list into the
  sidebar's actual navigation — so a reader clicking through the site is
  walking the reading order without being told anything. Most work, most
  legible.

→ *(no answer recorded)*


What would you like to clarify about this one?


For context on where I'm coming from: the question came out of the work that
numbered the demo runners, and it was raised as being *larger* than that task
— so I may be framing it too narrowly, or too broadly. Some things I'm unsure
I've read correctly, in case one of them is what you want to pull on:


- **Who the reader is.** I wrote the options assuming a stranger arriving at the
  repository cold. It might instead be you in a year, or an agent picking the
  project up mid-stream — those want different things.

- **Whether "reading order" is even the right claim.** The index is described as
  a position in the project's story. I've been treating that as *start at the
  lowest number and read upward*, which may be more literal than you meant.

- **Whether this belongs in the documentation at all.** All three options put
  the answer in `docs/`. It could just as well be a comment convention in the
  file headers, or something the counter file itself carries.

- **Whether the demo runners' letter suffix is part of this.** A runner takes
  its source's number with a letter appended, so `037a` runs `037`. That's a
  second rule layered on the first, and I didn't account for it in any option.


Or it may be something else entirely — what's on your mind?


--------------------------------------------------------------------------------

### User Request 4

we should remove the registry, and just parse from the C files at compile time.
Remember, we're building a system that runs the compiler whenever we add new
functions or whatever, and incorporates them into the running program. So, we'll
need the source-code both for creating boxmapgraphs, and for running them as
well. So, when we start the program, we should parse from the source-code to
build the box structures, rather than trusting them to be described in the map
file.


--------------------------------------------------------------------------------

### Assistant Response 4

Let me read the two blueprints this lands on before I answer.


Right — and this connects to two blueprints that already exist. Let me lay out
what I found, because one part of it constrains the design.


## What the registry actually contains


A generated C file, compiled into the binary, holding per box: the name as
written in a map, **a pointer to its shim**, and the `sizeof` of each parameter
and the return value. Two different kinds of thing are bundled there:


- **Names and types** — text, which a parser can absolutely produce from
  source.

- **Shim pointers and sizes** — which a parser cannot produce, and this is the
  load-bearing bit.


The runtime-compilation blueprint already worked this out and states it flatly:
*"A signature gives you names… What it cannot give you is sizes and offsets
— how many bytes a cell holds, how far into a struct a field sits, how large a
task must be. Those are what the engine actually runs on… **There is no
version of this that skips the compiler.**"*


A parsed signature saying `vec3` doesn't tell you `vec3` is 12 bytes — padding
and alignment are the compiler's answer, not the text's — and the standing
rule is that every size is emitted as a `sizeof` expression so the C compiler
computes it and the generator never guesses. And the shim, which unpacks a
task's bytes into typed C arguments and calls the box, is *compiled code*;
parsing text cannot yield a callable function.


## So the proposal, stated in a way that works


Not "remove the registry" — the registry is the in-memory name→box table the
engine walks when placing a station, and something has to be it. What goes away
is **emitting it at build time**. At startup the engine parses the box sources,
generates shims and `sizeof` expressions, invokes the compiler, loads the
result, and fills the table in memory. Same pipeline the runtime-compilation
blueprint describes, run for *every* box rather than only for late arrivals.


And that's a genuinely strong argument, stronger than the one you gave for it:
**it collapses two mechanisms into one.** Today build-time boxes go through the
generator at build, runtime boxes go through the same generator at runtime, and
those two paths have to agree forever. Make startup the only path and the rare
path stops existing — the code that adds a box while running is the same code
that started the program, exercised on every single run instead of almost never.


One small correction on the stated motive: map files already never describe box
types. The build document is explicit that *"the map file never has to mention a
type,"* precisely because two sources of truth would disagree and the map would
always be the wrong one. So that worry is already handled — but the
one-mechanism argument stands on its own and is better.


## What it costs, plainly


- **The C compiler becomes mandatory for every run.** The runtime-compilation
  blueprint answered the packaging concern with "assume it is there," handled by
  an install script — but that was for an *optional* capability. Making it a
  precondition for starting any program at all is a bigger commitment: a shipped
  program can no longer run on a machine without a toolchain.

- **Startup stops being instant.** Every start invokes the compiler over every
  box source. Without something to avoid repeat work, a program that took
  milliseconds to start takes seconds.

- **Failures move from your machine to the user's.** A malformed box source
  currently stops `make`. It would stop the program. Still loud and still fatal
  — just discovered later, by somebody else.

- **The generator being rewritten in C stops being optional and becomes a hard
  prerequisite.** A Lua generator running at startup would make LuaJIT a runtime
  dependency of every program, which is the exact thing that rewrite exists to
  eliminate.


That last one reorders the phase: the C generator has to land before this can.


The branch I can't call for you:


**[Asked the user]**


*Q1 — Build vs start:* If the box table is built by parsing and compiling at
startup, does the build still do that work ahead of time?

- Startup only, with a cache — The build compiles the engine and nothing else.
  Startup parses, generates, compiles, loads — but keys the compiled artifact
  by a hash of the source, so an unchanged box is loaded rather than rebuilt.
  One mechanism, and startup is only slow the first time after an edit.

- Startup only, no cache — Simplest possible: every start does the full
  parse-generate-compile. Nothing to invalidate, nothing to go stale, no hash to
  get wrong. Costs the compile on every single run, forever, including every
  demo and every test.

- Build pre-warms the same cache — The build runs the identical startup path
  early and leaves its output in the cache, so a normal start finds everything
  already there. Same one mechanism — the build is just its first caller.
  Costs a build step whose only purpose is to make startup fast.

→ *(no answer recorded)*


What would you like to clarify?


Guesses at what might be off, in case one of them is it:


- **Whether "startup" is even the right moment.** I assumed the
  parse-and-compile happens when a program starts. It might belong earlier —
  when a map file is *read*, since that's when box names first have to resolve
  — or later, lazily, when a station is actually placed. Those give different
  answers about what a cache is even for.


- **Whether I've drawn the cache too elaborately.** All three options revolve
  around avoiding repeat compiles. If you'd rather the engine just always
  compile and not carry a hash table of artifacts, that's option two and the
  other two are noise.


- **Whether the compiled artifacts are supposed to be in the RAM-backed scratch
  directory.** The runtime-compilation blueprint already says a box's source
  gets written there the moment the box is created, treated like a log —
  written as it happens, gone at reboot. If compiled shared objects live there
  too, then "cache" isn't a new concept at all, it's the existing ephemeral
  tier, and the question mostly answers itself.


- **Whether I was wrong to say the registry survives at all.** I argued it
  becomes an in-memory table built at startup rather than disappearing. You may
  mean something more thorough — that stations copy what they need at
  placement and nothing persists as a lookup table afterward, with re-parsing on
  demand when a new station is placed.


Or it's something else — what's on your mind?


--------------------------------------------------------------------------------

### User Request 5

I want to get rid of the registry. I think we could do this by requiring map
files to name the file, then function that they're drawing from. Is this
feasible? Then, we can parse the argument names (do we even need them?) and once
the program starts, we can do sizeof() to get their sizes before building box
stations. I think we should find a way to not require the compiler toolchain, if
the user doesn't include any boxes that use it to create new functions or
whatever - a static map that doesn't do any re-wiring or adding of new boxes
shouldn't need to compile anything at all at runtime, but a box that
specifically writes new code and implements it into the already-running process
would. So, what does the registry do? It holds the names of functions, which we
can reference the source-code for. It holds the function arguments, which we can
parse from the source-code. It holds the shims, which we're creating at
startup-time and run-time (if we add new boxes) because they are simple, small
functions that simply pass values from point-A to point-B. And... What else does
it manage?


--------------------------------------------------------------------------------

### Assistant Response 5

Let me look at what's actually in the generated registry rather than answer from
the design docs.


Feasible in part, and the part that isn't feasible is worth being precise about,
because it decides everything else.


## First, the full inventory — including two things you didn't list


Reading the actual record rather than the docs, here's everything, and whether a
parser could produce it:


**Per box:**


| field | from source? |

|---|---|

| name as written in a map | yes |

| parameter count | yes |

| each parameter's type *name* | yes |

| return type *name* | yes |

| **shim pointer** | no — compiled code |

| **each parameter's size** | no — `sizeof` |

| **return size** | no — `sizeof` |

| **task size** (one allocation per invocation) | no — derived from those |

| **compare function pointer** | no — compiled code |


**Per struct — this is the first thing you didn't list:**


| field | from source? |

|---|---|

| struct name, field names, field kinds, array lengths | yes |

| nested-struct links | yes |

| **struct size** | no — `sizeof` |

| **each field's offset** | no — `offsetof` |

| **each field's size** | no — `sizeof` |


These field tables are what let a map file write `in 2 = { 1.5, 2.5, 3.5 }` into
a struct port without a hand-written parser for every type, and what let the
dump turn those bytes back into text.


**And the second: a three-way comparison function per orderable type.**
Comparator stations need them, and they can't be generic byte comparisons — a
negative float reads as *larger* than a positive one when you compare raw bytes.
So each one is a real compiled function that copies bytes into typed variables
first. The primitives get generated ones, a struct supplies its own.


There's also a small thing: the registry gets read *backwards*. A station stores
a shim pointer, and the dump speaks names, so something has to answer "which box
owns this pointer."


## The part that can't work


> once the program starts, we can do `sizeof()` to get their sizes


`sizeof` is a **compile-time operator**. The compiler evaluates it and burns a
literal constant into the machine code. A running program has no types at all
— C erases every bit of type information during compilation, so there is
nothing at runtime for `sizeof` to be applied *to*. You cannot hand a running
program the text `vec3` and get 12 back, because `vec3` doesn't exist anymore;
only bytes do.


You can see the trick in the generated file. Its first real line is `#include
"029-demo-boxes.c"` — it pulls the box source in **whole**, so the types
become visible to the compiler, and only then writes `sizeof a0`. That include
is the entire reason the numbers are right, and it happens at build time.


So a size comes from exactly one of two places: a `sizeof` expression compiled
into the binary, or a compiler invoked at runtime. There is no third door.


## Which puts your two goals in direct opposition


- *Get rid of the registry.*

- *Don't require the toolchain for a static map that adds no boxes.*


**The registry is precisely the mechanism that satisfies the second goal.** It
is the frozen record of everything the compiler already computed, so a static
program can start with no toolchain anywhere in sight. Delete it and every
program start needs a compiler — including the ones you specifically want to
spare.


## What I think you're actually objecting to, and a shape that fixes it


The objectionable thing isn't that sizes are precomputed. It's that they live in
**one global table, frozen at build time, that a map addresses by a bare name**
— so the set of boxes a program can ever place is fixed, and a name in a file
is trusted to hit the right row.


Your file-and-function proposal fixes the naming half, and it's a good idea
independent of everything else. Combine it with one change of scope:


- A map names `math.c:add_two_ints` rather than `add_two_ints`. No collisions,
  provenance is visible, and the map says where to look.

- The registry stops being one global table and becomes **one table per box
  source file, compiled alongside that file** — `math.c` ships with the
  metadata for `math.c`.

- A static map over existing box files: **no compiler at runtime.** Every table
  is already compiled in.

- A new box file arriving while the program runs: generate its table, compile
  it, load it, append. **Compiler needed exactly then and not otherwise** —
  which is the rule you asked for.


That doesn't delete the registry, but it dissolves what's wrong with it: nothing
is global, nothing is frozen, and the metadata travels with the code it
describes instead of being a separate authority that could disagree.


**One bonus from your own question — "do we even need argument names?" No.**
The engine never uses a parameter name. Under comparison-by-width it doesn't use
type *names* either, only widths. Names survive purely for error messages and
the dump. So the per-file table can be much smaller than today's: sizes, a shim
pointer, a compare pointer, and a name to be found by.


The question I'd put back to you: does that per-file, travels-with-the-code
shape get you what you wanted, or is there something about a precomputed table
at all that you're trying to be rid of? If it's the latter, then the toolchain
requirement is the price and I'd rather we decide that deliberately than
discover it.


--------------------------------------------------------------------------------

### User Request 6

  - A map names math.c:add_two_ints rather than add_two_ints. No collisions,
    provenance is visible, and the map says where to look.


yes, and if there's for example two math.c's in separate sub-directories, it
should fail to compile with a specific error message.


  - The registry stops being one global table and becomes one table per box
    source file, compiled alongside that file — math.c ships with the

    metadata for math.c.


instead of a separate "registry table" for each box, can't we just... fill in
the values where they're needed? Or would that pollute the map file with
auto-generated stuff that isn't important for a user writing a ceramic program
by hand?


  One bonus from your own question — "do we even need argument names?" No. The
  engine never uses a parameter name. Under comparison-by-width

  it doesn't use type names either, only widths. Names survive purely for error
  messages and the dump. So the per-file table can be much

  smaller than today's: sizes, a shim pointer, a compare pointer, and a name to
  be found by.



we could just record the argument index, then parse the source-file again and
find the name for the debug report. This could fail if the function is changed
underneath us while the program is running, so... that's fine I guess. Maybe we
could have a line above that section in the debug output dump report that
explains that? And lists the index and argument name?


  The question I'd put back to you: does that per-file, travels-with-the-code
  shape get you what you wanted, or is there something about a

  precomputed table at all that you're trying to be rid of?


So, can we build this table into the compiled binary? Do we have compiled
binaries? If so, can we include the source-code appended to the end of the
binary just... putting the bits at the end, to be easily parsed if the user
wants to? That way we can send one single file, and it could expand if we needed
to look at the exact text, and we wouldn't need to send a directory to the user.
If so, then we can keep the information for the sizeof()'s and such in the box
station's template record inside the binary.


--------------------------------------------------------------------------------

### Assistant Response 6

Working through these in order.


## Two `math.c` files → a build error


Agreed, and it's consistent with something the project already does. The file
index counter enforces a single ordering across every directory rather than
per-directory, so a **flat global namespace of filenames is already the model
here**. Making box source basenames unique is that rule applied once more, and
the error message writes itself: both paths, and the instruction to rename one.


The alternative — the map naming `src/boxes/math.c` with a path — would let
duplicates coexist, but then every map file carries your directory layout and
breaks when you reorganize. Basename plus a hard collision error is the better
trade.


## "Fill in the values where they're needed"


Concretely that means the generator stops emitting a *record* and emits a
**placement function** per box:


```c

static void place__add(station_t *s) {

    s->call      = add__call;

    s->slots[0].elem_size = sizeof(int);   /* compiler folds to 4 */

    s->slots[1].elem_size = sizeof(int);

    s->out_size  = sizeof(int);

    s->compare   = int__compare_g;

}

```


Every number is a compile-time constant, folded by the compiler, stored nowhere.
That is genuinely "filled in where needed" and it deletes the `box_info_t`
record, the parameter arrays, the type-name strings, and `task_size` — most of
the table by volume.


**But it cannot delete the table entirely, and the reason is structural rather
than a design choice.** Something still has to turn the text
`math.c:add_two_ints` into `place__add`. A lookup from a name to a thing *is* a
table; calling it something else doesn't remove it. The only way out would be
compiling the map file itself into C so the name never exists at runtime — and
that destroys the premise the whole project rests on. The build document is
blunt about it: *"the map is not a build input… this is the price of maps
being data rather than code, and it was chosen deliberately."* Maps have to stay
editable while the program runs.


So the honest end state: **the table shrinks to one string and one function
pointer per box.** That's about as close to gone as a name lookup gets.


And to answer the worry directly: **no, none of this touches the map file.**
Nothing generated goes in there. A map stays exactly what a person types by hand
— station names, box references, wires, constants. The generated material
lives in generated C beside the box sources, where it already lives.


## Argument names by index, re-parsed for the report


Your reasoning holds, and your caveat is the right one to have. I'd push it
slightly further, because the project's own rule is that a fallback is a warning
and a warning is an error — and "the names in this report might be wrong" is
exactly a silent fallback.


Cheap fix: record the source file's **modification time or a hash at
placement**, one number per file. The report then says one of two things rather
than hedging:


```

Argument names below were re-read from source at report time.

  math.c — unchanged since placement, names are correct.

  net.c  — CHANGED since placement; names may not match

           the code that actually ran.

```


One number, and a caveat becomes a fact. And as you'll see below, for build-time
boxes this problem disappears entirely.


## Appending the source to the binary


**Yes, and this is the best idea in your message.** Taking the sub-questions in
order:


**Do we have compiled binaries?** Yes — the engine is C, and `make` builds
test binaries and phase demos.


**Can you append data to the end of one?** Yes, and it's an established
technique, not a hack. An ELF binary is defined by its headers: the loader reads
the program headers, maps the segments they describe, and **ignores every byte
past the last one**. So `cat payload >> binary` leaves a working binary.
Self-extracting archives, AppImage, and Go's pre-`embed` asset bundling all work
exactly this way. To find the payload you append its length as a fixed-size
trailer at the very end, read the last few bytes, and seek back.


There are three ways to do it, and I'd not pick the append:


| approach | how | cost |

|---|---|---|

| **Emit the source as a C array** in the generated file | It's just more generated C — `static const char math_c_src[] = "...";` | Nothing new to learn, fully portable, no post-build step. Source text sits in the loaded image. |

| A named ELF section (`objcopy --add-section`) | Data lives in the file but outside any loaded segment | Smaller runtime footprint; needs a build step and is ELF-only. |

| Append past the last segment | Length trailer at EOF, read via `/proc/self/exe` | Works everywhere, but finding your own executable is the fragile part — `/proc` is Linux-specific and `argv[0]` lies. |


The first is the one that fits this project: the generator already `#include`s
each box source *whole* into the generated file so the compiler can see the
types. Emitting that same text as a string array alongside is a few more lines
in a generator that's already reading it.


**And here's what it buys, which is more than portability.** One file becomes
the entire system:


- **Compiled code** — shims and comparisons.

- **The numbers** — every `sizeof` folded into a placement function as a
  constant.

- **The text** — readable, re-parseable, and available to hand to a compiler.


Consequences that fall out:


- A **static map runs with no toolchain and no source directory anywhere.** You
  ship one file.

- A program that **adds a box while running has the source right there** to feed
  the compiler, without shipping a tree alongside.

- **Your debug-report failure mode disappears for build-time boxes.** The
  embedded copy is frozen at build; source on disk can change all it likes and
  the report reads the text that was actually compiled. Only runtime-added boxes
  have live source — and those already save their own source to the RAM-backed
  directory the moment they're created.


So: `sizeof` stays where it has to be, computed by the compiler at build, folded
into a placement function. The text rides along for the humans and for the
compiler. And the only surviving table is a name and a pointer.


---


This is now a real design change touching the registry, the generator, the map
file format, the runtime-compilation blueprint, and the packaging note — plus
the claim-mutex decision from earlier, which is still only in this conversation.
**Say the word and I'll write all of it into the blueprints**, which is where it
needs to be before it evaporates.


Also still open from the original sweep: the numbering-convention question you
wanted to clarify, and the hand-placement one, which this largely answers — if
placement functions replace records, phase 2's tests place stations by calling
one directly and never touch a registry at all.


--------------------------------------------------------------------------------

### User Request 7

  The alternative — the map naming src/boxes/math.c with a path — would let
  duplicates coexist, but then every map file carries your

  directory layout and breaks when you reorganize. Basename plus a hard
  collision error is the better trade.


how about we accept both - try the basename first, if there's duplicates, then
request a specific file path like src/boxes/math.c or similar. Make sure we
check when we add new functions at runtime.


  Every number is a compile-time constant, folded by the compiler, stored
  nowhere. That is genuinely "filled in where needed" and it deletes

  the box_info_t record, the parameter arrays, the type-name strings, and
  task_size — most of the table by volume.


  But it cannot delete the table entirely, and the reason is structural rather
  than a design choice. Something still has to turn the text

  math.c:add_two_ints into place__add. A lookup from a name to a thing is a
  table; calling it something else doesn't remove it. The only way

  out would be compiling the map file itself into C so the name never exists at
  runtime — and that destroys the premise the whole project

  rests on. The build document is blunt about it: "the map is not a build
  input… this is the price of maps being data rather than code, and

  it was chosen deliberately." Maps have to stay editable while the program
  runs.



do we still need math.c:add_two_ints at runtime? If not, could we just...
translate it into place__add at compile time? Then we don't need the registry
anymore right?


  Cheap fix: record the source file's modification time or a hash at placement,
  one number per file. The report then says one of two things

  rather than hedging:


  Argument names below were re-read from source at report time.

    math.c — unchanged since placement, names are correct.

    net.c  — CHANGED since placement; names may not match

             the code that actually ran.



okay good but we don't need to report if it wasn't changed - just elide that
debug output line, and only report if they did change.


  Can you append data to the end of one? Yes, and it's an established technique,
  not a hack. An ELF binary is defined by its headers: the

  loader reads the program headers, maps the segments they describe, and ignores
  every byte past the last one. So cat payload >> binary

  leaves a working binary. Self-extracting archives, AppImage, and Go's
  pre-embed asset bundling all work exactly this way. To find the

  payload you append its length as a fixed-size trailer at the very end, read
  the last few bytes, and seek back.


  There are three ways to do it, and I'd not pick the append:


  ┌────────────────────────────────┬────────────────────────────────────────┬────────────────────────────────────────────────────────────┐

  │ approach │ how │ cost │

  ├────────────────────────────────┼────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ Emit the source as a C array │ It's just more generated C — static │
  Nothing new to learn, fully portable, no post-build step. │

  │ in the generated file │ const char math_c_src[] = "..."; │ Source text
  sits in the loaded image. │



great let's do the first option


--------------------------------------------------------------------------------

### Assistant Response 7

## Both forms, with the path as the tiebreak


Good — that's strictly better than what I proposed. Resolution becomes: a bare
`math.c` is tried as a basename; exactly one match resolves it, more than one is
an error naming every candidate and telling the author to write a path. A map
may always write `src/boxes/math.c:add_two_ints` explicitly and skip the search.


Two consequences worth pinning down now:


**The dump writes whichever form is unambiguous.** Since a dump has to reload
into the same program, it writes the bare basename when that resolves uniquely
and the full path when it doesn't. The round trip holds either way and the file
stays as readable as it can be.


**The runtime check has a choice in it I don't want to make for you.** When a
new box source arrives while the program runs and its basename collides with one
already loaded, either the add is refused, or the add succeeds and
*bare-basename references from then on* fail as ambiguous. I lean to the second
— refusing would mean a legitimate second `math.c` can never be loaded, and
stations already placed resolved their names at placement so they're unaffected
— but it means a map that loaded fine yesterday errors today, which is worth
you agreeing to rather than discovering.


## Do we still need the name at runtime?


**Yes, and it's forced rather than chosen.** The map file arrives at a binary
that has never seen it, carrying text. Translating `math.c:add_two_ints` into
`place__add` at compile time requires the compiler to have *read that map* —
which makes the map a build input, and maps being data rather than build inputs
is the thing the whole project is built on. Read a map at runtime and you must
resolve a name at runtime; those are the same sentence.


**But your instinct pays off anyway, more than I gave it credit for.** I checked
whether anything *else* needs a name lookup at runtime, and nothing does:


- **Compare functions** — needed per station, written in by the placement
  function. No lookup.

- **Struct field tables** — needed when a map writes `in 2 = { 1.5, 2.5, 3.5
  }` into a struct port. But by then the station exists, so the placement
  function can have hung a pointer to the field table on the port itself. No
  lookup.


So after this change there is exactly **one** name lookup in the entire engine,
and it has two columns:


```c

static const struct { const char *name; void (*place)(station_t *); } boxes[] = {

    { "math.c:add_two_ints", place__add },

    { "math.c:multiply",     place__mul },

};

```


Everything else — sizes, task size, shim, comparison, field layouts — is
folded into `place__add` as compile-time constants and reached through the
station afterward. That is small enough that "registry" is the wrong word for
it, and I'd retire the word even though the mechanism survives.


**There is one way to have literally no table, and I want you to know it exists
before I recommend against it.** Compile with `-rdynamic` and the linker exports
every symbol into the binary's dynamic symbol table; then `dlsym(NULL,
"place__math_c__add_two_ints")` looks a function up **by name in your own binary
at runtime**, using the linker's table instead of yours. It genuinely works and
you emit nothing.


Why I'd not: the mangling from a filename to a C identifier becomes an ABI you
can't change, and it reintroduces exactly the collision you just eliminated —
`math.c` and `math_c` mangle to the same symbol. `-rdynamic` exports everything,
not just what you meant. And a failed `dlsym` returns null with no reason, so
the most common error a map author will ever see gets worse. You'd trade a
two-column array you control for a symbol table you don't, in a project that
just spent a whole blueprint shedding a dependency for portability's sake.


## The other two


**Elide the unchanged line** — agreed, and better. The report says nothing
when nothing moved, and a line appears only when a source changed under a
running program. No news is no line.


**Source as a C array in the generated file** — decided. It slots in cleanly:
the generator already reads each box source and `#include`s it whole so the
compiler can see the types, so emitting the same text as a string array is a few
more lines in a pass that's already running.


---


That's the design settled. It spans a fair amount: the registry shrinking to a
name table, placement functions replacing records, the `file:function`
addressing with path fallback, source embedded in the binary, the toolchain
becoming conditional rather than mandatory, and the argument-name reporting.
None of it is written down yet, and neither is the claim-mutex decision from
earlier in this conversation — the one-lock-per-station scoping that made the
roll-back path disappear.


Want me to write all of it into the blueprints now?


--------------------------------------------------------------------------------

### User Request 8

  Yes, and it's forced rather than chosen. The map file arrives at a binary that
  has never seen it, carrying text. Translating

  math.c:add_two_ints into place__add at compile time requires the compiler to
  have read that map — which makes the map a build input, and

  maps being data rather than build inputs is the thing the whole project is
  built on. Read a map at runtime and you must resolve a name at

  runtime; those are the same sentence.



um, we need to have the map at both compile time and runtime. Because... If we
don't have it as a build input, then how do we know which functions from which
files to load?


  There is one way to have literally no table, and I want you to know it exists
  before I recommend against it. Compile with -rdynamic and the

  linker exports every symbol into the binary's dynamic symbol table; then
  dlsym(NULL, "place__math_c__add_two_ints") looks a function up by

  name in your own binary at runtime, using the linker's table instead of yours.
  It genuinely works and you emit nothing.



would prefer not to do this approach


--------------------------------------------------------------------------------

### Assistant Response 8

You're right, and I was conflating two different things. Let me separate them,
because the distinction is the whole answer.


**What the build does today:** the generator reads *whatever sits under the box
source directory* and emits a shim for every function it finds. So today's
answer to "how do we know which functions to load" is **we don't ask — we
compile in all of them.** That works and it's why the map has never needed to be
a build input. It also means a program using three boxes out of five hundred
carries five hundred shims.


**The two questions I ran together:**


| question | what it decides |

|---|---|

| Does the build read the map to know *what to include*? | binary size, and whether the build can check that named boxes exist |

| Does the runtime resolve a name from text? | whether maps are data or code |


You can answer **yes to both**, and that's what you're describing. The map is a
**manifest** at build time — it tells the build which files and functions to
pull in — and it's still **text** at runtime, resolved through the name table.
The build never turns `math.c:add_two_ints` into a call; it just learns that it
needs to bring that function along. Maps stay data.


## What that buys


- **The binary carries only what the map uses**, and so does the embedded
  source. A three-box program embeds three boxes.

- **The build validates every box reference.** Naming a function that doesn't
  exist, or a file that doesn't exist, or a basename that's ambiguous with no
  path given — all of it fails on your machine at `make` time, naming the map
  line. Today none of that is checkable, because the build has never seen a map.
  This doesn't change wire checking, which still needs load time since it
  depends on how stations are actually connected — but "you named a box that
  isn't there" stops being a runtime discovery.

- **The build knows exactly which sources to embed** as C arrays, rather than
  embedding the whole directory.


## What it costs, and how it lands on your toolchain rule


The binary becomes specialized to the maps it was built for. Load a map naming a
box that wasn't compiled in, and it fails at load — loudly, naming the box and
the file it expected to find it in.


And that failure is exactly the case the runtime-compilation path exists for. So
your rule stops being a stipulation and becomes something the design derives:


- **Map uses only boxes the binary carries** → no toolchain, ever. Ships as
  one file.

- **Map names a box the binary doesn't carry** → the toolchain is needed, and
  it's needed precisely because new code is genuinely arriving.


One loose end: a program that builds its stations by calling C directly, with no
map file at all, gives the build no manifest to read. So what the build includes
should be **declarable more than one way** — a list of map files, or
"everything under this directory," same mechanism at different scope. The
directory form is what a general-purpose binary or the test suite would use; the
map form is what a shipped program would.


**And `dlsym` is dropped** — noted, not pursuing it.


---


That settles it. To write down, I have:


- The claim mutex scoped to a station's cell states, and the roll-back path
  disappearing

- The map file's dash for an unconfigured port, and `x64` for a starting depth

- Optional parameters refused, with the readiness-rule reasoning

- `file:function` addressing, basename first with path as tiebreak, collisions
  fatal

- The registry reduced to a two-column name table, with placement functions
  carrying the constants

- Box source embedded in the binary as generated C arrays

- The map as a build manifest, and the toolchain requirement becoming
  conditional

- Argument names re-parsed for reports, reported only when the source changed


Shall I write these into the blueprints now?


--------------------------------------------------------------------------------

### User Request 9

  What the build does today: the generator reads whatever sits under the box
  source directory and emits a shim for every function it finds.

  So today's answer to "how do we know which functions to load" is we don't ask
  — we compile in all of them. That works and it's why the map

  has never needed to be a build input. It also means a program using three
  boxes out of five hundred carries five hundred shims.



yeah, uh, we should only compile in the functions we actually need. So, the ones
that are specifically mentioned in the map file, and anything that they require.


  ┌──────────────────────────────────────────────────────┬─────────────────────────────────────────────────────────────────────┐

  │ question │ what it decides │

  ├──────────────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────────┤

  │ Does the build read the map to know what to include? │ binary size, and
  whether the build can check that named boxes exist │

  ├──────────────────────────────────────────────────────┼─────────────────────────────────────────────────────────────────────┤

  │ Does the runtime resolve a name from text? │ whether maps are data or
  code │

  └──────────────────────────────────────────────────────┴─────────────────────────────────────────────────────────────────────┘


  You can answer yes to both, and that's what you're describing. The map is a
  manifest at build time — it tells the build which files and

  functions to pull in — and it's still text at runtime, resolved through the
  name table. The build never turns math.c:add_two_ints into a

  call; it just learns that it needs to bring that function along. Maps stay
  data.



I answer yes to both


  - Map uses only boxes the binary carries → no toolchain, ever. Ships as one
    file.

  - Map names a box the binary doesn't carry → the toolchain is needed, and
    it's needed precisely because new code is genuinely arriving.


okay but... We won't know that the compiled file doesn't contain boxes the
binary doesn't carry until runtime, right? If we edit the map file, we should
need to re-compile. Ideally, incremental recompilation, so we don't have to do
the entire process again. Then, if it changes between compilation and runtime,
we can just ask to re-compile, right? Or do we even need the source, and we
wouldn't know that it changed, because we'd just be running the binary?


  One loose end: a program that builds its stations by calling C directly, with
  no map file at all, gives the build no manifest to read. So

  what the build includes should be declarable more than one way — a list of
  map files, or "everything under this directory," same mechanism

  at different scope. The directory form is what a general-purpose binary or the
  test suite would use; the map form is what a shipped program

  would.



I feel like it needs at least one map file with at least one station that does
the rest of the building, right?


--------------------------------------------------------------------------------

### Assistant Response 9

## Only what's needed, at file granularity


Agreed, with one mechanical caveat about *"and anything that they require."*
Computing which helper functions a box calls means understanding C — function
pointers, macros, conditional compilation — and the generator deliberately
doesn't. It recognizes declarations and nothing more.


The way to get the same result without that: **include the named files whole and
emit shims only for the named functions.** The map names `math.c:add_two_ints`,
so the build pulls in `math.c` entire — every helper it needs comes along
automatically, because it was always in the same translation unit — and emits
exactly one shim. Unused functions in that file produce no shims, and the linker
discards their code if nothing reaches it.


That gets you the thing you actually want. A program using three boxes out of
five hundred compiles in three shims and however many source files those three
live in, rather than five hundred shims. And the generator never has to know
what a call is.


## Editing a map means rebuilding


Yes, and it falls out of what the build already does. The Makefile currently
regenerates whenever a box source or the generator is newer than its output; map
files join that list. Edit a map, `make` re-runs the generator. Incremental
comes free if the generated material is split per box source file, so touching
one map rebuilds only what that map's changes touched.


## But you don't need to detect that the map changed


This is the part worth pushing back on slightly, because I think it's simpler
than you're allowing.


The **only** thing a map edit can do that the binary can't handle is name a box
the binary doesn't carry. Everything else — new stations using boxes already
present, rewiring, changed constants — works without any rebuild at all, and
*has* to, because editing a running program is a feature rather than an
accident.


So the binary doesn't need to know whether the map changed. It needs to **fail
well on the one thing it can't do**, and that failure is already specific: *this
map names `math.c:divide`, which this binary does not carry — rebuild, or hand
the source to the running program.* Two ways forward, both named. A hash check
comparing the map against the one built from would fire on every harmless edit
too, which turns a feature into a nuisance.


**And your second half retires something we designed two messages ago.** You
asked: do we even need the source, given we're just running the binary? Right
— and because the source is now *embedded*, the binary carries the exact text
that was compiled into it. Someone editing `math.c` on disk afterward changes
nothing the binary can see, and that's correct, because the running code is the
old code. So the modification-time check for the debug report is **unnecessary**
and should be dropped: the embedded text is by definition the text that ran. The
only box whose source can drift is one added at runtime, and those already save
their source when they're created.


## A map with a station that builds the rest


Appealing, and it makes "every program begins by reading a map" universally
true, which gives the build a manifest every time and removes the second
declaration form entirely.


**It collides with a standing guarantee, though, and I'd rather show you the
collision than route around it.** The guarantees page holds that *a box cannot
write a static* — and the reason is recorded as buying something specific:
reaching a map from inside a box required a process-wide map pointer, and that
pointer was what limited a process to one map. Removing it is what allows
several maps to run at once without seeing each other. A box that builds
stations needs to reach a map, which is the thing that pointer was for.


There is a resolution: **the map handle travels as an ordinary value on a wire**
rather than as a process-wide pointer. It arrives through the program's input
station — the door that already exists for values coming from outside — and
a construction box takes it as a parameter like any other. No global, so several
maps still coexist.


What it costs is that a box acquires a side effect far larger than any box has
today. Every box in the engine currently takes values and returns one; this one
reshapes the program. That's not obviously wrong for a system whose stated
ambition is being edited while it runs, but it is a real widening of what "a
box" means, and it's the kind of thing that should be decided rather than
absorbed.


**[Asked the user]**


*Q1 — Bootstrap:* Should every program start from a map file, with
construction beyond that done by a station?

- Yes — map handle rides as a value — Every program has a map. A
  construction box receives the map handle through the input station, as an
  ordinary value on an ordinary wire, so no process-wide pointer returns and
  several maps still run side by side. Widens what a box may do.

- Yes for programs, no for tests — A shipped program always starts from a map.
  Test binaries declare their box set by directory instead, and build stations
  by calling the surface directly. Keeps boxes as pure value-transformers
  everywhere, at the cost of the second declaration form staying.

- No — keep construction outside boxes — A map is data a program reads; the
  thing that reads it is ordinary C. Programs that build themselves do it in
  their own main function, which already works and needs no guarantee revisited.
  The build accepts either a map list or a directory.

→ *(no answer recorded)*


What would you like to clarify?


Places I might have the wrong end of it:


- **What "does the rest of the building" meant.** I read it as a box that adds
  stations and wires while the program runs. You may have meant something
  narrower — a map that names the other maps to load, or a station that reads
  a second map file — neither of which needs a box to touch the graph at all,
  and neither of which collides with any guarantee.


- **Whether I've overweighted the guarantee.** The rule that a box can't reach
  the map is recorded as buying the ability to run several maps in one process
  without them seeing each other. If you don't care about that ability, the
  whole collision evaporates and the answer is just "yes, let boxes build."


- **Whether the test binaries are really a separate case.** I keep treating
  phase 2's tests as needing a way in that shipped programs don't. If every
  binary having a map is fine — tests included, with a one-line map — then
  option two is pointless and I've invented a distinction you don't want.


- **Whether the construction box is a box at all.** It might be a different kind
  of station entirely, the way the input and output stations are already
  special-shaped rather than ordinary. That would sidestep "what a box may do"
  completely, because it wouldn't be one.


What's on your mind?


--------------------------------------------------------------------------------

### User Request 10

  It collides with a standing guarantee, though, and I'd rather show you the
  collision than route around it. The guarantees page holds that a

  box cannot write a static — and the reason is recorded as buying something
  specific: reaching a map from inside a box required a

  process-wide map pointer, and that pointer was what limited a process to one
  map. Removing it is what allows several maps to run at once

  without seeing each other. A box that builds stations needs to reach a map,
  which is the thing that pointer was for.



this guarantee is deprecated, because maps and boxes are the same thing now.


Computing which helper functions a box calls means understanding

  C — function pointers, macros, conditional compilation — and the generator
  deliberately doesn't. It recognizes declarations and nothing

  more.



I think we should be able to just search for #include statements, right? Because
you can't function pointer to something that wasn't included, and macros need to
include in order to reference something in another file. Could we apply the
macros first? That's during the linker step of the compiler, right? Then we can
just ignore macros since they'll already have applied before we looked at the
source-code. Not sure what you mean by conditional compilation, could we just...
evaluate the conditionals, and then include after that? Would we need to patch
the compiler for that? Sounds like a lot of work if so. Any other potential
issues with just, grepping for #include or whatever?


--------------------------------------------------------------------------------

### Assistant Response 10

## The guarantee


Understood — a box cannot write a static is retired, on the grounds that a map
wrapped in its input and output stations *is* a box, so "a box reaching a map"
is just a box reaching a box.


Worth noting what survives that: **several maps in one process, invisible to
each other**, holds as long as the handle travels as a value on a wire rather
than as a process-wide pointer. That was the thing the old restriction was
actually buying, and the handle-as-value shape keeps it. So one guarantee goes
and the other stands, and the guarantees page needs both edits.


## The `#include` question


**One correction on ordering, because it changes the answer in your favour.**
Macros aren't expanded at the linker step — they're expanded *first*, by the
preprocessor, before the compiler proper ever runs:


```

preprocessor  →  compiler  →  assembler  →  linker

   cpp             cc1           as           ld

```


Which means you can get exactly what you're asking for with a stock flag and no
patching. **`gcc -E file.c`** runs only the preprocessor and prints the result:
every `#include` inlined, every macro expanded, every conditional resolved. What
comes out is plain C with no directives left in it. All three of your worries
handled in one step.


**Conditional compilation** is `#ifdef` / `#if` — code that's present or
absent depending on flags. Your own tree does it: the generated registry wraps
its clock reads in `#ifdef SORA_STATS`, so the same source compiles to two
different programs. A parser reading raw text sees both branches and can't know
which one is real; the preprocessor sees the flags and emits one. The catch is
that you must preprocess with the *same* flags you compile with, or you resolved
the conditionals for a program you aren't building.


Two practical notes if you go this way: `-E` output is enormous, because system
headers inline to tens of thousands of lines — but it carries `# 47 "math.c"`
line markers saying which file each chunk came from, so filtering back to your
own code is mechanical. And the generator would now invoke the compiler, which
is free at build time since a C compiler is already the one thing this engine
requires.


**The soundness hole in following includes, though.** Your premise — you can't
reference something that wasn't included — is true of well-behaved C but not
of C. A file can write `extern int helper(int);` by hand and call `helper`
having included nothing at all. **The linker resolves symbols, not includes**;
`#include` only controls what the *compiler* sees. So an include graph is a good
approximation and not a correct one, and the cases it misses are exactly the
sloppy ones you'd want caught.


## Which is why I'd let the linker do it instead


The linker already computes precise reachability, correctly, including every
case parsing gets wrong. Compile every box source to an object file, emit shims
**only** for the functions the map names, and link with:


```

-ffunction-sections -fdata-sections -Wl,--gc-sections

```


Each function lands in its own section, and the linker discards every section
nothing reaches. What survives is the engine, your named shims, and exactly what
those shims transitively call — through includes, through hand-written
externs, through function pointers taken by name, all of it. **The generator
never looks at an `#include` at all**, and never has to understand C beyond
declarations, which is the property it was built around.


So the question I'd put back to you is what *"only compile in what we need"* was
aiming at:


- **Binary size** — linker garbage collection delivers it exactly and soundly,
  with zero parsing. You compile 500 files and ship 3.

- **Build time** — it doesn't help at all; you still compile all 500. Getting
  that needs the include-following heuristic, with the extern hole accepted, or
  `gcc -E` per file to find dependencies precisely at the cost of running the
  preprocessor over everything anyway.


My read is you meant size, in which case the linker settles it and the whole
parsing question goes away. If you meant build time, that's a different and
harder problem and worth naming as its own thing rather than folding into this
one.


--------------------------------------------------------------------------------

### User Request 11

Hmmmm. It's okay if build time is slow. It's less okay if runtime compilation is
slow.


--------------------------------------------------------------------------------

### Assistant Response 11

That settles the build: **linker garbage collection**, no include parsing, no
heuristics. Compile everything, ship what's reachable.


Let me check whether the tool for the other half is already sitting in your libs
directory.


Not installed here — you have `gcc` and `clang`, no `tcc`, and it's not in
your libs directory either. That matters for what I'm about to recommend, but
the runtime-compilation blueprint already anticipated exactly this: it answers
the dependency question with *an install script that builds the dependencies
from source into a local place, written once the system is finished*. This would
be the first real entry on that list.


## Where the time actually goes


Invoking `gcc` on one small box source spends it roughly like this:


| cost | what it is |

|---|---|

| process spawn | `gcc` is a *driver* — it forks and execs `cc1`, then `as`, then `collect2`, then `ld`. Four processes, not one. |

| preprocessing | system headers inline to tens of thousands of lines before your code is even reached |

| code generation | the actual compiling, and the part that scales with optimization level |

| linking to a shared object | writing a file, then `dlopen` reading it back, then `dlsym` |


For one tiny function that's tens to low hundreds of milliseconds. Fine once.
Miserable if you're editing a graph and watching it respond.


Notice how much of that isn't compiling. Spawning four processes and
round-tripping a file through the filesystem is overhead you're paying to talk
to a compiler *as a program* rather than *as a library*.


## The answer is a compiler you can call


**TinyCC**, as `libtcc`, compiles C **in-process**: you hand it source as a
string, it compiles into a memory buffer you own, and you ask it for function
pointers by name. No subprocess. No shared object written. No `dlopen`, no
`dlsym`, no file touched at any point. It was built for precisely this use —
runtime C compilation is its reason to exist, not a side capability — and it's
roughly an order of magnitude faster than GCC on small inputs. (Order of
magnitude, not a measured figure; I haven't run it here.)


It also lands neatly on your RAM-artifact habit. Compiling straight into memory
doesn't just *avoid* the disk — there's no artifact to place in a tier at all.


**What it costs: TinyCC generates unoptimized code**, roughly what you'd get
from `gcc -O0`. It does no serious optimization and isn't trying to.


Which points at the shape:


- **Build time — GCC or Clang, fully optimized.** Slow build, fast code.
  You've just said that's the right trade.

- **Runtime — TinyCC, in-process.** Fast compile, slower code. You're editing;
  responsiveness is what matters.

- **Optionally, later — recompile a hot box with GCC and swap it in.** The
  runtime-compilation blueprint already designs the machinery for this: retire,
  sweep, free, with a per-worker counter marking when nobody is inside a box's
  code. Swapping a slow implementation for a fast one is the same lifetime
  problem as unloading one.


## The risk worth naming, and why it's already covered


Two compilers means two answers to `sizeof`, and this engine's standing rule is
that the compiler computes every size and nothing guesses. If TinyCC and GCC
ever disagreed about a struct's layout, a box compiled by one would be reading
fields at offsets the other didn't write.


For standard types they won't — struct layout is fixed by the platform ABI,
not by the compiler. Where they could diverge is the exotic edges: bitfields,
`long double`, packed attributes, GCC-specific extensions TinyCC doesn't
implement. That's a real constraint on what a box may be written in, and it
should be stated rather than discovered.


**But the check that catches it already exists.** A box compiled at runtime
reports the width of each input and its output — that's how it participates in
wiring at all, under comparison-by-width. So if TinyCC computed a different size
than GCC did for the same type, the wire between a runtime box and a build-time
box **fails the ordinary width check**, at the moment it would first matter,
with the existing error message. No new mechanism, no new failure mode. The
safety net was already strung.


---


Still holding the full list to write down, now with: linker garbage collection
for the build, TinyCC in-process for runtime compilation, the guarantee about
boxes reaching maps retired while the several-maps-per-process one stands, and
the map handle traveling as a value.


Want me to start writing, or is there more to settle first?


--------------------------------------------------------------------------------

### User Request 12

  Two compilers means two answers to sizeof, and this engine's standing rule is
  that the compiler computes every size and nothing guesses. If

  TinyCC and GCC ever disagreed about a struct's layout, a box compiled by one
  would be reading fields at offsets the other didn't write.


um, this sounds like a critical problem. I think we should probably just stick
with gcc or similar. What would it take to support multiple compilers? Is there
any reason to? Are some compilers only usable on certain platforms maybe?


--------------------------------------------------------------------------------

### Assistant Response 12

Let me correct myself, because I overstated it and that's what prompted the
concern.


## How real the risk actually is


Struct layout on a given platform is fixed by the **platform ABI**, not by the
compiler. On Linux x86-64 that's the System V AMD64 ABI, and every compiler
targeting it must follow the same rules — otherwise a GCC-compiled object
couldn't link against a Clang-compiled one, which people do constantly and
routinely. `sizeof` for standard types and plain structs is the same number
across GCC, Clang, and TinyCC, and it has to be.


Where they can genuinely differ is narrow: bitfields (the fiddliest corner of
the ABI), `long double`, `__attribute__((packed))` and `#pragma pack` — which
are *extensions*, not ABI — and compiler extensions generally. And for that
last category the realistic failure isn't a wrong number, it's **TinyCC refusing
to compile** something GCC accepted, which is loud rather than silent.


So: narrower than I made it sound, and the width check covers the remainder. But
your conclusion still stands on a better argument, which I'll get to.


## The tension dissolves anyway


You said build time can be slow and runtime compilation shouldn't be. I reached
for a second compiler to get that. **You don't need one.**


**Optimization level does not change struct layout.** It can't — layout is
ABI, and `-O0` and `-O2` are the same compiler emitting the same offsets. So:


- **Build time:** `gcc -O2`, fully optimized. Slow, fast code.

- **Runtime:** `gcc -O0`, plus `-fsyntax-only`-style leanness where possible.
  Much faster to compile, slower code — and **zero** ABI risk, because it's
  the same compiler that built the binary.


That gets you most of what TinyCC was for, at none of the cost. The remaining
overhead is process spawn and the file round-trip, which precompiled headers and
the RAM tier both attack without introducing a second answer to anything.


And if runtime latency still hurts later, the move that keeps this decision is
to take the compile **off the critical path**: hand the source over, return
immediately, and let the station go live when the compile finishes. The program
is already asynchronous by construction; a box that isn't ready yet is a station
that isn't ready yet, which is a state the engine already has a word for.


## Your two questions


**What would it take to support several?** Not much — one interface (*"compile
this source, hand me function pointers"*) with a backend behind it, which is a
dispatch table rather than a branch, the way this project prefers. A subprocess
backend covers gcc and clang identically since their command lines agree on
everything that matters here.


**Is there a reason to?** Yes, but the reason is **availability, not speed**:


| compiler | targets | present here |

|---|---|---|

| GCC | essentially everything | yes |

| Clang | most of what GCC does | yes |

| TinyCC | x86, x86-64, ARM, ARM64, RISC-V — and no more | no |

| MSVC | Windows, where neither GCC nor TCC is native | n/a |


On an unusual target TinyCC simply doesn't exist, and on Windows the native
toolchain is neither. So an engine hardwired to one compiler is an engine that
doesn't run in places it otherwise could — which is exactly the distance the
packaging note exists to measure.


## What I'd write down


**One compiler per program, and it is the compiler that built the binary.**
Record which one at build time; use that same one for every runtime compile.
Then there is exactly one answer to `sizeof` in a given program **by
construction** — not checked, not mitigated, structurally impossible to
violate. The multi-compiler support isn't "the engine can mix compilers"; it's
"the engine isn't hardcoded to any particular one," which buys portability and
gives up nothing.


That also means a box source is constrained to what *your* compiler accepts,
which is the same constraint every other file in the program already has.


---


Ready to write when you are. Current list:


- Claim mutex scoped to a station's cell states; roll-back path gone

- Dash for an unconfigured port; `x64` for a starting depth

- Optional parameters refused, with the readiness-rule reasoning

- `file:function` addressing, basename first, path as tiebreak, collisions fatal

- Registry reduced to a two-column name table; placement functions carry the
  constants

- Box source embedded as generated C arrays; the modification-time check dropped
  as unnecessary

- Map as build manifest; linker garbage collection decides what ships

- Runtime compilation via the build's own compiler at low optimization; one
  compiler per program

- The box-reaching-a-map guarantee retired; several-maps-per-process kept,
  handle travels as a value


--------------------------------------------------------------------------------

### User Request 13

Okay sounds good. Let's allow the user to pick their compiler, and just use one
per run. Does Clang work on windows? If so, would it make sense to just pick
that one? Or is gcc faster at runtime or something...?


--------------------------------------------------------------------------------

### Assistant Response 13

## Clang on Windows


Yes, in two distinct modes, and the distinction matters:


- **`clang-cl`** — an MSVC-compatible driver, using the MSVC ABI and Windows
  runtime. This is what you use to interoperate with MSVC-built code.

- **Clang targeting MinGW-w64** — GNU-style, GCC-compatible ABI, MinGW
  runtime.


But **Clang on Windows doesn't ship the platform's headers and libraries.** It's
a compiler, not a toolchain. You still need the Windows SDK and MSVC, or
MinGW-w64, for the headers and the linker. So picking Clang doesn't remove the
platform dependency — it moves it somewhere less obvious, which is worse than
leaving it visible.


**And there's a bigger Windows problem that has nothing to do with the
compiler.** Loading compiled code at runtime is `LoadLibrary` and
`GetProcAddress` there, not `dlopen` and `dlsym`, and the artifact is a `.dll`
with explicit export declarations rather than a `.so`. So the
runtime-compilation path needs a Windows implementation no matter which compiler
you choose. That's the real Windows cost, and it's a separate piece of work.


## Is GCC faster than Clang at this?


Broadly comparable, varying by version and by the code. Clang has historically
had an edge at `-O0`; GCC has closed most of it and sometimes wins now.


More usefully: **that difference is the wrong term to optimize.** Ordered by
size, the costs in a runtime compile are roughly:


1. Process spawn — `gcc` is a driver that forks `cc1`, then `as`, then the
   linker. Four processes.

2. The file round trip — write a shared object, `dlopen` reads it back.

3. Optimization level — `-O2` versus `-O0`.

4. Which compiler it was.


Picking between GCC and Clang optimizes the smallest term. The first three are
where the time is, and none of them cares which compiler you chose.


## So: the user picks, and it's the ordinary convention


`CC` at build time — `make CC=clang` — which is how every C project already
works and requires inventing nothing. The build records which compiler it used,
and the runtime invokes that same one.


**No version check needed, and this is the neat part.** I was about to suggest
recording the compiler version and comparing at runtime, but it's unnecessary:
the thing you'd be checking for is disagreement about sizes, and **the width
check already catches that directly.** A newly compiled box reports the width of
each input and output; if any type it shares with the binary came out a
different size, the wire fails at the moment it's drawn, with the error message
that already exists. Checking versions would be checking a proxy for the thing
you can check exactly.


So the recorded compiler is only there so the engine knows what to *invoke*. It
isn't a validation token.


One detail from your own house rules: if the engine has to *discover* a compiler
because none was recorded — falling back through `CC`, then `cc`, then `gcc`
— that's a fallback, and the rule is that fallbacks are warnings and warnings
are errors. So discovery has to say what it picked: *no compiler recorded; using
`/usr/bin/gcc`.* Loud, not silent.


---


That's the whole design settled as far as I can see. Writing it up now unless
you say otherwise — the blueprints affected are the port record, the
lockless-claim one, optional parameters, the registry and generator ones,
runtime compilation, the map file format, the guarantees page, and the packaging
note.


--------------------------------------------------------------------------------

### User Request 14

um, let's just stick with gcc then and worry about windows support and other
compiler options later.


--------------------------------------------------------------------------------

### Assistant Response 14

Settled — GCC only, Windows and other compilers deferred.


One thing I'd do now and only this: keep the compiler invocation in **one
place** in the source, so the later change is a local edit rather than a hunt.
Not an abstraction — there's nothing to abstract over yet — just not
scattering `gcc` across three files. And the packaging note gains a stated
limitation: this engine requires GCC specifically, and the reason it isn't more
general yet is that nobody has needed it to be.


Now let me write all of it down.


Index counter at 062, issues numbered by phase. Starting with the map file
decisions.


Writing the port record answers first.


Now the map file format document. Let me see the station line section.


Now the optional-parameters refusal.


Splice was off by one line. Repairing.


Now the biggest rewrite — the claim design. Let me write it.


The parent edit mangled those lines. Repairing.


Now the guarantees page.


Now the registry work — a new parent issue with children, in phase 3.


Now the runtime-compilation blueprint gets the compiler decisions.


Now the hand-placement question in the station-build issue, which the placement
functions answer.


Now the build-path document, whose registry section is now wrong.


Fixing that block properly.


Now the packaging note and the phase 3 progress page.


Now the packaging note gains the compiler decision.


Let me verify the documentation builds and check for broken links.


All tests pass. Let me check what's staged before committing.


That's this session's transcript — same conversation ID as my scratchpad. All
ten transcript files are one tool run, so they go with the work. Committing.


Committed as `acb31ff`. Here's what landed and what's still open.


## Written down


**New blueprints — the registry work**, as a parent with four children in
phase 3:


- **The registry dissolved** — the parent. Inventories what the table actually
  holds, establishes the two things that can't move (a size is computed by a
  compiler; a name in a text file must be resolved at runtime), and shows that
  everything else folds away.

- **Boxes addressed by file** — `math.c:add` on a station line, basename-first
  with paths as tiebreak, collisions fatal at build time, plus what happens when
  a late-arriving source creates an ambiguity.

- **Placement instead of records** — the generator emits a function per box
  that fills a station directly. Also answers the hand-placement question from
  the original sweep: it survives, and turns out to be the *primitive* rather
  than the alternative.

- **Source rides in the binary** — box sources emitted as C string arrays.
  This also retired the modification-time check we'd designed two messages
  earlier, since embedded text is by definition the text that compiled.

- **The map as a manifest** — build reads maps to know what to include, linker
  garbage collection trims, and box references get checked at `make` time.


**Rewritten:**


- **The claim issue**, renamed from *The claim takes no lock* to *The copies
  leave the lock*, since the title said the opposite of the design. It records
  the reversal, the proof about ascending order that made the lock-free version
  work, and why none of it is needed once one hold covers the walk. All ten
  references across the tree updated.

- **Optional parameters**, rewritten as a refusal record with the C survey and
  the readiness-rule argument, moved to `issues/completed/`, links repointed.

- **The build-path document**, whose registry section is now a box-table
  section.

- **The map file format**, which gained the dash, the `x64` depth, and
  file-and-function addressing, with the example rewritten to show all three.

- **The guarantees page** — S4 and S5 and T6 rewritten for the narrowed lock,
  G11 retired, G12 restated as what G11 was retired in favour of.

- **The packaging note**, which gained the GCC decision as its decision zero.

- Phase 2 and phase 3 progress pages.


**Verified:** `make test` passes end to end, and the documentation site
regenerates with **0 unresolved references** across 111 pages.


## This is in progress, not finished


Four open questions stand, so by your own rule the task isn't done:


| where | question |

|---|---|

| The demos issue | **The numbering convention communicates nothing.** From the original sweep — you asked to clarify and we went to the registry instead. Still unanswered. |

| Placement instead of records | **How does the dump name the box a station was placed as?** Carry the string on the station (one pointer each, simplest) or the table row index (smaller, needs the table stably ordered — true at build, less obviously true once rows are appended at runtime). |

| The map as a manifest | **Can the build now check wires, and should it?** New consequence I hadn't seen: with maps as build inputs, the generator sees both ends of every wire. It still can't compute a size, but it can emit a `_Static_assert` making the *compiler* compare them — moving a whole class of error from load time on a stranger's machine to build time on yours. Runtime-drawn wires still need the load check, so this adds a second checker rather than replacing one. |

| The map as a manifest | **May a program declare several maps and carry the union of their boxes?** A workbench binary that opens whatever you hand it wants this. |


One more thing from the sweep I haven't fixed: **every open question in this
project lives in an issue file.** The ten datapath documents, the guarantees
page, and the first-pass report carry none. Your rule says questions should be
spread so every document gets touched — that gap is still there.


Which would you like to take next?


--------------------------------------------------------------------------------

### User Request 15

  One more thing from the sweep I haven't fixed: every open question in this
  project lives in an issue file. The ten datapath documents, the

  guarantees page, and the first-pass report carry none. Your rule says
  questions should be spread so every document gets touched — that gap

  is still there.



don't worry. It's fine if they stay in issue files.


can you ask the four remaining questions to me one at a time? Let's take another
pass and clear up the last concerns.


--------------------------------------------------------------------------------

### Assistant Response 15

Good — that closes the docs gap by decision. Four questions, one at a time.


Let me check what a source file header looks like, since it bears on how I frame
the first one.


The numbers interleave engine files with their tests — 018 station, 019
station, 020 delivery, then 021–024 testing exactly those. So the order really
is a story: a thing, then its proof, then the next thing. Every file already
opens with a header naming itself and saying what it is.


## Question one: making the numbering explain itself


A reader meeting `033-statics.c` beside `034-gather.c` has no way to learn
whether the numbers mean *read these in this order*, *these were made in this
order*, or *these are related*. The convention is followed carefully and says
nothing to anyone who wasn't told.


Your standing rule is never to create things manually but to write the tool that
generates them — which rules out a hand-maintained list, since it goes stale
the first time a file is added. So the real question is **where a reader meets
the explanation**, not which document to write.


**[Asked the user]**


*Q1 — Reading order:* Where should a reader encounter what the file numbers
mean?

- In each file's own header — A tool adds a line to every file's existing
  header comment naming what comes before and after. The reader learns the
  convention from whatever file they happened to open first — which is the
  actual failure case, since someone browsing source may never open a document
  at all. Regenerated on every build, so renumbering fixes itself.

- A generated reading index — A tool walks the tree, sorts by index, and
  writes a document listing every file in order with a line about each — taken
  from the header each file already has. Gives the whole shape at once, which no
  per-file line can. Costs a reader having to find it.

- Both, and the site walks it — The per-file lines, the generated index, and
  the documentation site's navigation ordered by index with next/previous at
  each page foot — so clicking through the site is walking the reading order
  without being told. Most work; the reader meets it wherever they arrive.

- Leave it undocumented — The numbers order the tree for whoever already
  knows, and a reader who doesn't simply reads files in whatever order they like
  — which mostly works. Costs nothing and admits the convention is for you
  rather than for strangers.

→ *(no answer recorded)*


Understood — index means reading order and nothing else, never "these are
related." Let me check the phase 7 numbering before writing the issue.


Question one is written up as a very-low-priority issue. On to the second.


## Question two: how a dump names the box a station was placed as


The **dump** walks the live station table and writes out a map file describing
whatever the program has grown into — and the round trip has to hold, so dump,
load, dump produces identical text. To write a station line it needs to say
`math.c:add`.


Today a station stores a pointer to its shim (the generated function that
unpacks a task's bytes and calls the box), and the dump searches the registry
backwards to find which box owns that pointer. Once the registry is a two-column
table of names and placement functions, that backwards search has nothing rich
to search. So the station has to carry its own identity forward.


The wrinkle is that the box table is stably ordered at build time but grows at
runtime when new boxes arrive, and rows are appended and never removed.


**[Asked the user]**


*Q1 — Station identity:* How should a station remember which box it was placed
as, so the dump can name it?

- The row number — One integer per station, indexing the box table. Rows are
  appended and never removed, so a number stays valid for the life of the
  program. Smallest, and the same reasoning that makes a wire an index rather
  than a pointer — a position survives the storage moving, a pointer does not.

- The name string — A pointer to the same string literal the table holds —
  not a copy, so no allocation and nothing to free. The dump prints it without
  consulting anything. Costs 8 bytes instead of 4, and reintroduces a pointer
  into a record where the project has deliberately preferred indices.

- The placement pointer — The station keeps the placement function it was
  built by, and the dump searches the table for the row holding that pointer.
  Closest to today's shape — a backwards search — so the least change. Costs
  a linear scan per station at dump time, over a table that now grows.

→ *(no answer recorded)*


What would you like to clarify?


Some things I may have assumed wrongly, in case one of them is the sticking
point:


- **Whether the station should carry this at all.** All three options add a
  field to the station record, which is deliberately fixed-size and which the
  project keeps lean. The alternative I didn't offer is that the *dump* keeps
  its own side table — station index to box name — built once when the map
  is loaded or constructed, and the station record stays untouched.


- **Whether the row number is safe under everything, not just appends.** I
  claimed rows are only ever appended so a number stays valid forever. That
  rests on the runtime-compilation design never removing a row — which is true
  today because unloading a box only serves one nobody placed. If a row could
  ever be reclaimed, the index breaks and the string doesn't.


- **Whether this is even a real choice.** It's four bytes versus eight bytes
  versus a scan, on a path that runs when somebody asks for a dump. If the
  answer is "pick whichever and don't spend a question on it," say so and I'll
  take the row number and move to question three.


- **Whether "which box" is the right question.** A station placed by hand —
  through the raw placement path rather than by name — has no box table row
  and no name at all. So there may be stations the dump genuinely cannot name,
  and the real question is what it writes for those.


What's on your mind?


--------------------------------------------------------------------------------

### User Request 16

the dump process takes a currently running process and produces an output
artifact. This artifact is meant to be used to re-produce the map at it's
current status, for debug or other purposes, correct? My understanding is that
this includes the current value of all of it's ring buffers and statics and
such, it's wiring, the box functions, etc... So, is there a reason we can't just
do the compilation process again, and re-create a new binary with all the newly
created box functions written as actual C code, as if it were a regularly
compiled binary? And include their source just like we do the rest of the
source-code at the end of the binary's bits. And, we could have a little report
that explains the current values of the ring buffers, statics, whether or not
any box functions were changed, that sort of thing. So, not just describing the
program, but also creating it so it can be re-started at that exact point with
those exact values already pre-filled. It should just... work, easily.


--------------------------------------------------------------------------------

### Assistant Response 16

Let me check what the dump actually captures today, because part of your
understanding of it doesn't match what it does.


One correction first, because it changes the size of what you're proposing.


## What the dump captures today


**Only the schematic.** The map format document is explicit: *"It is a
schematic, not a save file… it does not hold data that persists between
runs."* The dump walks the live station table and writes out stations, box
names, kinds, wiring, and the constants sitting on ports.


It does **not** capture:


- values sitting in ring buffers — the work in flight

- tasks already built and waiting in the pool

- an iterator's cursor, which the station header calls *the one memory a station
  keeps*

- the statistics counters


So what you're describing isn't an extension of the dump. It's a different
artifact: **an image** — the Smalltalk sense, or Emacs's dumper, or CRIU on
Linux. A program frozen and revived rather than a program described and rebuilt.


## And this project is unusually well suited to it


The thing that normally makes checkpointing impossible is hidden state — a
running program's data scattered through static variables, open handles, buffers
nobody registered. **This engine forbids exactly that.** The first of the three
nouns says a box *"is not permitted to remember anything between calls."* There
is no hidden state to miss, by construction. Everything a program is lives in
the graph, and the graph is already walkable because the dump walks it.


That's a much stronger position than most systems are in when someone proposes
this.


## What would have to be captured


| what | difficulty |

|---|---|

| graph shape, box names, wiring | **built** — the dump does it |

| constants on ports | **built** |

| runtime-added box source | **nearly free** — embedding source is already planned, and a runtime box already saves its own text when created |

| values sitting in ring buffers | **new, moderate** — bytes and cell states exist; the map format needs a form for *this port holds these N values in this order* |

| iterator cursors | **new, easy** — one integer per station |

| statistics counters | **easy, and probably shouldn't be restored** — a revived program's run counts are a fresh question |

| tasks built but not yet run | **the hard one** — a task lives outside any station, holding copies of values that are no longer in the buffers they came from |


## The hard part is the moment, and it's already designed


To capture a consistent state you have to stop the world: workers are mid-task,
holding claimed cells, copying bytes. Issue 106 already builds that machinery
for a different purpose — *stop handing out new work, quiet what can be
quieted, collect, say it, then die*. A checkpoint is that same sequence with a
different payload instead of a death at the end.


**And there's a simplification that deletes the hardest row above.** If you
*drain* rather than *freeze* — stop accepting new work, let every task already
in flight run to completion, and only then capture — there are no in-flight
tasks to serialize. What's left is values sitting in buffers, which is
straightforward bytes. The cost is that a checkpoint takes as long as the
longest task still running, and a program with a genuinely stuck box never
reaches the capture.


## Binary or data file


Here the toolchain rule we just set answers it cleanly:


- **No boxes were added at runtime** → the artifact is a **data file**, and
  the same binary reloads it. No compiler involved. This is the common case and
  it should be the fast, easy one you're describing.

- **Boxes were added at runtime** → their C has to be compiled into a new
  binary to make the artifact self-contained. That needs the toolchain — which
  is exactly the case where we already decided a toolchain is required, because
  new code genuinely arrived.


So "produce a new binary" isn't the general mechanism; it's what happens when
the program grew code that the old binary doesn't have.


**On "it should just work, easily"** — honestly: the graph half is done, the
buffer half is a moderate piece of work, the drain is designed but unbuilt, and
producing a binary is more a build-system problem than an engine one. Not easy,
but nothing in it is *hard* in the way concurrency is hard, and the
box-can't-remember rule is why.


This wants its own issue. Before I write it:


**[Asked the user]**


*Q1 — Capture moment:* When a running program is captured, what happens to
work already in flight?

- Drain first, then capture — Stop accepting new work, let every task already
  running finish and deliver its result, then walk the graph. Nothing is in
  flight at the moment of capture, so tasks never have to be serialized at all
  — the hardest item on the list stops existing. Costs waiting for the slowest
  task, and a genuinely stuck box means the capture never happens.

- Freeze where it stands — Halt workers mid-task and serialize the tasks too
  — each one a copy of its inputs plus which box it was going to call.
  Captures instantly, no waiting on a slow or stuck box. Costs the most work: a
  task is a shape the map format has no vocabulary for, and reviving means
  re-enqueueing tasks that were never in a buffer.

- Drain, and say what would not drain — Drain as above, but bounded: if tasks
  are still running after a stated point, capture anyway, name every station
  still busy, and mark the artifact as incomplete. Gets a usable capture out of
  a wedged program — which is when you most want one — at the cost of an
  artifact that admits it lost something.

→ **Answered:** Drain, and say what would not drain" selected preview:

1. stop building new tasks

2. wait for the pool to go quiet

3. still busy? capture anyway:


   # INCOMPLETE CAPTURE

   # 2 tasks still running at

   #   capture: parser, hasher

   # their inputs are lost; their

   #   results never delivered


4. walk everything else normally. You can now continue with these answers in
   mind.


Drain with a bound, and an artifact that admits what it lost. Let me write that
up.


Back to the outstanding question two, which I'll frame more honestly this time:
it's a small choice, and one part of it is more interesting than the part I
asked about.


**The small part:** a station needs to remember which box placed it so the dump
can write `math.c:add`. Four bytes (a row number into the box table), eight
bytes (a pointer to the table's own name string), or a backwards scan. Rows are
only ever appended and never reclaimed, so a row number stays valid for the life
of the program.


**The interesting part:** once hand placement is the primitive, a station can be
built through the raw path with **no box table row and no name at all** —
which is exactly what phase 2's tests do. The dump's whole value is that it says
what is actually there, so it has to write something for such a station.


**[Asked the user]**


*Q1 — Nameless station:* What should the dump write for a station placed
through the raw path, which has no box name?

- The raw path takes a name too — Every placement carries a name, so every
  station can be named and no such case exists. A test placing by hand passes
  whatever string it likes. Those names will not reload — nothing on disk is
  called that — so such a dump describes faithfully without round-tripping,
  which is honest for a program nobody loaded from a file.

- A comment saying it cannot be named — Matches how the dump already handles a
  port the format cannot spell: it writes a comment admitting the gap rather
  than lying or omitting. Keeps the raw path free of a name it does not need.
  Costs a second kind of unspellable thing in the format.

- Refuse to dump such a program — A dump that cannot round-trip is not a dump,
  so a program containing a nameless station refuses with the station named.
  Keeps the round-trip promise unqualified. Costs the tool being useless in
  exactly the case — a hand-built program mid-construction — where somebody
  most wants to see what they have.

→ *(no answer recorded)*


What would you like to clarify?


Guesses at what might be off:


- **Whether nameless stations should exist at all.** I've been treating the raw
  placement path as something tests use and therefore something the dump must
  tolerate. You may think the opposite: that every station is placed by name,
  full stop, and phase 2's tests get a small hand-written box table rather than
  a special door. That makes the question disappear instead of answering it.


- **Whether I've got the round trip's scope wrong.** I keep saying the dump must
  reload into the same program. That's a promise the project made for programs
  *loaded from a file*. Whether it was ever meant to cover a program somebody
  assembled by calling C directly, I'm not sure — and if it wasn't, option one
  is obviously right and the other two are inventing a problem.


- **Whether this should wait for the capture work.** Given the artifact you just
  described — a program written out whole and revived — the dump's naming
  problem may look different from inside that design than it does from here. It
  might be worth deciding once, there, rather than twice.


- **Whether it's too small to be worth your time.** It's four bytes versus eight
  bytes plus one edge case. If you'd rather I pick the row number and the
  name-carrying raw path and move to questions three and four, say so and I'll
  do that.


What's on your mind?


--------------------------------------------------------------------------------

### User Request 17

sorry I need more background - what's the box table? Sounds like a registry,
which we don't want. What do you mean by "a station needs to remember which box
placed it" - in this case, you mean a box template placing a box station, right?


--------------------------------------------------------------------------------

### Assistant Response 17

Your framing is better than mine — **box is the template, station is one
placement of it.** That's exactly the project's own vocabulary, and I'll use it.


Now the honest part, because I think my issue title misled you.


## The registry was not eliminated. It was shrunk.


When you said "get rid of the registry," I went looking for whether that was
possible and found it isn't — and I wrote that up in the parent issue under
*the two things that cannot change*. But I titled the issue **"The registry
dissolved,"** which overclaims, and then renamed the surviving thing to "box
table," which reads like I was hiding it. That's on me.


Here is what actually survives and why.


**The chain from a map file to running code:**


1. A map file is text on disk. A line says `adder math.c:add p`.

2. `math.c:add` is **bytes** — characters, not a pointer to anything.

3. To make a station out of that, the engine needs the **shim** (the generated
   function that unpacks a task's bytes into typed C arguments and calls your
   box) and the **size of each parameter**.

4. Both of those exist only in the compiled binary — the shim as machine code,
   the sizes as numbers the compiler burned in.

5. Something has to connect the text in step 2 to the things in step 4.


That connection is a table. Calling it something else doesn't remove it. **The
only way to have no table is to compile the map file itself into C** — and
then maps stop being data you can edit while the program runs, which is the
thing the whole project is built on.


**What did change:** the lookup used to hand back a fat record — parameter
type names, every size, the return type, the task allocation size, a comparison
pointer. Now it hands back a **function** that fills the station in directly,
with every number folded into it as a constant:


```c

{ "math.c:add", place__math_c__add }

```


Two columns. That's the whole of it. So: smaller than a registry by a lot, still
a registry in kind. I should not have implied otherwise, and I'd retitle the
issue to say what it does — something like *"The registry shrinks to a name
and a door"* — unless you'd rather it kept the stronger name.


## Now the actual question, restated in your terms


A station is a placement of a box template. **The station needs to remember
which template it came from**, because the dump walks stations and writes map
lines, and to write `adder math.c:add p` it has to know this station is a
placement of `math.c:add`.


Today it works that out backwards: the station holds its shim pointer, and the
dump searches the registry for whichever row owns that pointer. Once the rows
are just a name and a placement function, that backwards search still works but
is a scan over a table that now grows.


So: **should the station carry its template's identity directly** — a row
number into the table, four bytes — or keep the backwards scan?


And the part I found more interesting: **a station can be placed without naming
a template at all.** The raw path takes the sizes and the shim directly, which
is what phase 2's tests do and what by-name placement is built out of. Such a
station is a placement of no named box, so the dump has nothing to write on its
station line.


Does that clear it up enough to answer, or is there more you want unpacked
first?


--------------------------------------------------------------------------------

### User Request 18

  That connection is a table. Calling it something else doesn't remove it. The
  only way to have no table is to compile the map file itself

  into C — and then maps stop being data you can edit while the program runs,
  which is the thing the whole project is built on.


the map file should be a blueprint for the compilation process. When the program
is running, when we edit the map file we're not editing the on-disk map file
(unless we save a dump of the program, or save the map edits or something) -
we're editing the in-RAM, in-process datastructures. To connect step 2 and 4, we
just burn math.c:add's bytes into the compiled binary the same way we do the
shim, and the sizes.


Could we just make it part of the shim, and store it there? Could we derive the
name from the shim's name?


--------------------------------------------------------------------------------

### Assistant Response 18

You're right, and my claim was too strong. Let me redo it.


## Where I went wrong


I said the table is irreducible because a map arrives as text. That's only true
**if the binary has to load a map it wasn't built for.** You've just said it
doesn't: the map is a blueprint for the compilation, and the thing you edit
while running is the in-RAM structure, not the file.


If that's the model, the build can emit the construction calls directly:


```c

void build_program(map_t *m) {

    map_place(m, 0, place__math_c__add,   PLAIN);

    map_place(m, 1, place__io_c__print,   PLAIN);

    map_wire (m, 0, /*out*/0, /*to*/1, /*port*/0);

    map_set_static(m, 0, /*port*/1, 5);

}

```


**No name appears anywhere.** The text `math.c:add` was consumed by the
generator and turned into a pointer at build time. And this doesn't weaken *one
way to build a program* — the generated function calls the same construction
surface a person would. The loader stops being a parser and becomes generated
code.


Runtime editing is untouched, exactly as you say: adding a station means handing
over a placement function, wiring means station indices, and none of it involves
a string.


## Your two suggestions, answered


**"Could we make it part of the shim and store it there?"** — Yes, and
slightly better than the shim: **the placement function writes the name onto the
station.**


```c

static void place__math_c__add(station_t *s) {

    s->box_name = "math.c:add";      /* a literal, for the dump */

    s->call     = add__call;

    s->slots[0].elem_size = sizeof(int);

    s->slots[1].elem_size = sizeof(int);

    s->out_size = sizeof(int);

}

```


The station carries its own name because the thing that placed it gave it one.
The dump reads the field. **There is no table to search, forwards or backwards**
— which dissolves the question I was asking you, rather than answering it.


The string costs nothing meaningful: it's a literal the compiler puts in
read-only data, and the station holds a pointer to it. Nothing allocated,
nothing freed.


**"Could we derive the name from the shim's name?"** — You could mangle
backwards, but don't. `place__math_c__add` can't tell you whether the file was
`math.c` or `math_c`, because both mangle to the same identifier — the same
collision we just spent an issue eliminating. Storing the literal is exact and
costs the same rodata the mangled symbol name was already costing.


## What a name table is actually for, then


Exactly one thing: **a binary that wants to load maps it wasn't built for.** A
workbench that opens whatever you hand it, or a general-purpose runner. That
binary declares its box set as a directory rather than a map, carries every
placement function, and needs a name table to find them.


So the table stops being a requirement and becomes **a capability a program opts
into.** A shipped program compiled from its own map has none. That's a much
better answer than the one I gave, and it's yours.


Consequences, all of which you'd already accepted:


- **Editing a map means rebuilding** — you said this three messages ago.

- **A dump is a map file, and running it means compiling it** — which is
  exactly the artifact-producing model you described for capture.

- **A box added at runtime** gets its placement pointer from the freshly
  compiled object it came in, which is an ordinary symbol lookup in that object
  rather than a table of ours.


I'd rewrite the parent issue around this — the title *"The registry
dissolved"* actually becomes true rather than overclaiming, since a normal
program ends up with no table at all.


One thing I want to check before I rewrite, because it decides how much of the
loader survives:


**[Asked the user]**


*Q1 — Map reading:* Should the engine still be able to read a map file at
runtime, or is compiling the only way a map becomes a program?

- Compiling only, with an opt-in reader — A normal program is built from its
  map and carries no name table. A binary that wants to open arbitrary maps
  declares its boxes by directory, gets the table, and keeps a runtime reader.
  Two shapes, and which one you get follows from what you declared.

- Compiling only, no reader at all — The runtime map reader is deleted
  outright. Every map becomes a program by being compiled, including a dump you
  want to re-run. Simplest possible engine — one path, no parser in the
  binary. Costs the ability to ever open a map you did not build, which the
  browser workbench and any general runner would want.

- Both always, table always present — Every binary carries the reader and the
  name table, and compiling the map is an optimization rather than the model.
  Closest to today. Costs the thing you were trying to get rid of — every
  program carries a registry whether it needs one or not.

→ *(no answer recorded)*


What would you like to clarify?


Places I might have framed it wrong:


- **What "reading a map at runtime" would even be for, now.** I listed the
  browser workbench and a general runner, but the workbench never runs anything
  — it draws a graph and downloads a file you compile. So the case may be
  thinner than I made it sound, and if there's no real user for it, option two
  is just correct.


- **Whether the two shapes are really two shapes.** I've been describing
  "shipped program" and "general runner" as different kinds of binary. They
  might be one binary that happens to declare its box set differently, in which
  case this isn't a fork in the engine at all — it's one mechanism where the
  table is present or absent depending on what you declared, and there's nothing
  to choose.


- **Whether a dump should be loadable at all.** You described capture as
  producing something you can restart from. If restarting always means
  compiling, then nothing ever loads a map at runtime and the reader has no
  purpose. If restarting sometimes means "same binary, reload the values," then
  the reader survives for that — but only for maps the binary already knows.


- **Whether I should just write it up and stop asking.** You've said maps are
  build inputs and editing one means recompiling. Option two follows from that
  fairly directly, and I may be manufacturing a decision you already made.


What's on your mind?


--------------------------------------------------------------------------------

### User Request 19

  "Could we derive the name from the shim's name?" — You could mangle
  backwards, but don't. place__math_c__add can't tell you whether the

  file was math.c or math_c, because both mangle to the same identifier — the
  same collision we just spent an issue eliminating. Storing the

  literal is exact and costs the same rodata the mangled symbol name was already
  costing.



why don't we just transcribe the name into english, so instead of math.c it'd be
math_dot_c and instead of math_c it'd be math_und_c?


  So the table stops being a requirement and becomes a capability a program opts
  into. A shipped program compiled from its own map has none.

  That's a much better answer than the one I gave, and it's yours.


I don't want to have many configurable options like this, no opt-in things -
this is the minimal implementation, so we should just have something that is
rigid and dependable.


  Consequences, all of which you'd already accepted:


  - Editing a map means rebuilding — you said this three messages ago.



just incremental compilation for the parts that were changed, if you're talking
about editing-while-running.


  - A dump is a map file, and running it means compiling it — which is exactly
    the artifact-producing model you described for capture.


also, creating it implies compiling it. Running it only means compiling it so
far as running any other map file means compiling it.


  I said the table is irreducible because a map arrives as text. That's only
  true if the binary has to load a map it wasn't built for. You've

  just said it doesn't: the map is a blueprint for the compilation, and the
  thing you edit while running is the in-RAM structure, not the

  file.



well, if we add a new function while the program is running, we'd be running
code that it wasn't originally built for. I'm not sure I understand why you say
this?


--------------------------------------------------------------------------------

### Assistant Response 19

Four things, in order.


## The escaping scheme works — and solves a different problem than you aimed it at


`_dot_` for `.` **and** `_und_` for `_` is exactly right, and the second half is
what makes it correct. Escaping the escape character is what makes an encoding
reversible; it's the same reason percent-encoding has to write `%` as `%25`.
Checking it:


| file | symbol |

|---|---|

| `math.c` | `math_dot_c` |

| `math_c` | `math_und_c` |

| `math_dot_c` | `math_und_dot_und_c` |

| `math.dot.c` | `math_dot_dot_dot_c` |


That last one looks alarming and decodes cleanly: scanning left to right, `math`
+ `_dot_` + `dot` + `_dot_` + `c`. No two distinct filenames produce the same
symbol.


**But the job it does is uniqueness, not recovery.** Recovering a name from a
symbol requires the symbol name to be readable at runtime, and these are static
functions whose names may not survive a stripped binary — so you'd have to
emit the string anyway, and then you have the string and don't need to decode
anything.


What the scheme genuinely prevents is a **duplicate-symbol link error**: without
it, `math.c` and `math_c` both generate `place__math_c__add`, the linker sees
two definitions, and the build fails with a message about a symbol rather than
about a design mistake. So: adopt it for generated symbol names, and store the
literal `"math.c:add"` on the station for anything a person reads. Two jobs, two
mechanisms, neither doing the other's work badly.


## No opt-in — so there is one shape, and it's the rigid one


Taking that at face value: **every map becomes a program by being compiled. Full
stop.** No runtime map reader in the engine, no name table in any binary, no
declaration that switches behaviour. A program is its map, compiled.


I'll stop offering the workbench and the general-runner as reasons to keep a
second path. The workbench never runs anything anyway — it draws a graph and
hands you a file.


## Incremental compilation


Agreed, and worth separating into two acts that look similar:


- **Editing the map file** → rebuild, incrementally, only what changed.
  Splitting the generated material per box source is what makes that possible.

- **Editing the running program's structures** — adding a station that places
  a box already present, moving a wire, writing a constant — → **no
  compilation at all**, because no new code exists. It's pointers and indices in
  RAM.


So the compiler enters only when new *code* appears, which is the same rule
everywhere else.


## Why I drew the distinction, and you're right that I stated it badly


You're correct: adding a function while running is running code the binary
wasn't built for. Here's the actual difference, which is about *who does the
lookup*:


**Adding a box at runtime.** You hand over C. It's compiled into a fresh shared
object and loaded. To get its placement function you call `dlsym(handle,
"place__math_dot_c__add")` — a name lookup, yes, but the **dynamic linker's**,
scoped to the object it just opened, resolving a symbol *we emitted seconds ago
and therefore know the exact spelling of*. We maintain nothing.


**Loading a map the binary wasn't built for.** Here the boxes are already
compiled into *our own* binary, as static functions. Static functions aren't
reachable by name from outside — that's what `static` means — so finding one
from a string requires either exporting every symbol in the program
(`-rdynamic`, which we rejected) or maintaining our own table.


So the two cases need name resolution from opposite directions: one gets it free
because the code just arrived through a door that resolves names, the other
would need a table because the code has been sitting inside us since the build
with its names compiled away.


Which is why deleting the second case deletes the table, and keeping the first
costs nothing.


---


That's the design. Let me rewrite the parent issue around it — the title *"The
registry dissolved"* becomes literally true, since a program ends up with no
table at all — and fold the escaping scheme in. Then I'll come back to
questions three and four, which are still outstanding.


--------------------------------------------------------------------------------

### User Request 20

  No opt-in — so there is one shape, and it's the rigid one


  Taking that at face value: every map becomes a program by being compiled. Full
  stop. No runtime map reader in the engine, no name table in

  any binary, no declaration that switches behaviour. A program is its map,
  compiled.


  I'll stop offering the workbench and the general-runner as reasons to keep a
  second path. The workbench never runs anything anyway — it

  draws a graph and hands you a file.



but, the general runner and the workbench that creates new map files and then
runs them are important use-cases? I think we can achieve them let's just figure
out how. What exactly is the blocking problem...?


--------------------------------------------------------------------------------

### Assistant Response 20

There's no blocking problem. I framed the runner wrong.


## The mistake: I made the runner an interpreter


I was picturing a general runner that **reads a map and looks its boxes up** —
text arrives, it finds the matching function inside itself, it builds stations.
That needs a name table, because its own boxes are static functions with their
names compiled away.


But that isn't what a runner has to be. **It compiles the map instead.** Hand it
`foo.map` and it:


1. runs the generator over that map and the box sources it names,

2. compiles the result — including a `build_program` function holding direct
   pointers, no names,

3. loads that object and calls it,

4. runs it.


No lookup, because the generator resolved every name at generation time. Which
is the *same* path a box added at runtime already takes, just with a whole
program's worth of scope instead of one function.


## So there is one shape, and it covers everything


**A map becomes a program by being compiled.** That's the whole rule. What
varies is only *when* the compiling happens:


| case | when the map is compiled | toolchain at run time? |

|---|---|---|

| a shipped program | at `make` time | no — it ships as one file |

| the general runner | when you hand it a map | yes, that is what the tool is |

| the workbench | it emits a map; the runner compiles it | yes, on the runner's side |

| a box added mid-run | when the source arrives | yes |

| a capture, re-run | when the artifact is compiled | yes |


Same generator, same compiler, same load. No configuration, no declaration that
switches behaviour, no second code path in the engine, and **no name table
anywhere.** The engine never reads a map as data to interpret — only as source
to compile, which is what the generator already does with everything else.


The honest cost, which isn't configurability: **a general runner needs a
toolchain, because that is what that program is** — the way `python foo.py`
needs Python present. It isn't a flag on the engine; it's a different program
that happens to invoke the generator, the way `make` invokes GCC.


## One consequence worth naming


If the runner already has `math.c` compiled in and you hand it a map naming
`math.c:add`, the newly generated object includes `math.c` **again** — its own
shim, its own placement function, its own copy of the code. The alternative
would be reaching into the runner's existing symbols, which means exporting
everything with `-rdynamic`, which we rejected.


**Duplicated code is harmless here, and the reason is the rule the whole engine
rests on.** A box may not remember anything between calls, so two compiled
copies of the same pure function are indistinguishable — same inputs, same
output, no state to diverge. Duplicated code isn't duplicated *state*. The only
thing that must agree is the widths, and they do, because it's the same compiler
on the same machine.


That's the same property that made capturing a running program tractable. It
keeps paying.


---


So the design closes with no opt-in and no blocker. Let me rewrite the parent
issue around it: the title becomes literally true, the escaping scheme goes in,
the runner and workbench are described as compiler front-ends rather than
interpreters, and the whole *"which shape did you declare"* business I invented
goes away.


Then questions three and four are still waiting.


--------------------------------------------------------------------------------

### User Request 21

okay, uh... where does that leave us?


--------------------------------------------------------------------------------

### Assistant Response 21

Fair — that was a long thread. Here's the state.


## Committed already (`acb31ff`)


The first batch: the map file's dash and `x64` depth, optional parameters
refused, the claim mutex narrowed to cell states, `file:function` addressing,
the registry work as five issue files, plus the guarantee and document updates.
Tests pass, docs site builds clean.


## Written but not committed


Two new issue files sitting on disk from this pass:


- **711 — an index means reading order.** Your answer to question one: numbers
  mean sequence, never relatedness; renumber by tool; very low priority.

- **712 — capturing a running program.** Your dump-as-an-artifact idea: drain
  the pool with a bound, write out every value still in a port, revive exactly
  there, and say what didn't drain.


## Decided in conversation, not yet written down


This is the gap, and it's the big one. Over the last several exchanges the
registry design changed **again**, past what I committed:


- A map is compiled into construction calls. No name lookup exists at runtime,
  in any binary.

- Which means **there is no table at all** — the issue I titled *"The registry
  dissolved"* was overclaiming when I wrote it, and is now accurate.

- Generated symbol names use your escaping scheme: `.` becomes `_dot_`, `_`
  becomes `_und_`, so no two filenames collide.

- A station carries its box's name as a string literal, written by its placement
  function, purely so the dump can print it.

- The general runner and the workbench are **compiler front-ends**, not
  interpreters — they compile a map rather than reading it.

- Editing a map file means an incremental rebuild; editing the running
  structures means no compilation at all.


None of that is in the files. The five committed issues still describe a
two-column table that no longer exists.


## Still open


- **Question three:** should the build check wires with static assertions, now
  that maps are build inputs? (This one got *more* interesting — if the map
  compiles to construction calls, the generator sees every wire and both its
  types.)

- **Question four:** may a program declare several maps? (This may have
  dissolved with the runner-as-compiler answer — worth checking rather than
  assuming.)


## What I'd do next


Rewrite the registry issues to match what you actually decided, commit those
together with 711 and 712, and then put questions three and four to you against
the corrected design rather than the stale one.


Want me to go ahead?


--------------------------------------------------------------------------------

### User Request 22

Yeah sounds good


--------------------------------------------------------------------------------

### Assistant Response 22

Rewriting the registry family to match. Starting with the parent.


Now 311a gains the escaping scheme, and its resolution moves to generation time.


Now 311d becomes "the map becomes code," and a new 311e covers the runner.


Now the build-path document, which still describes a two-column table.


A stale page was left behind by the rename — the site generator doesn't clean
orphans. Removing it and recording the gap.


Committed as `215907c`. Now question three, which changed shape with the
redesign.


## Question three: letting the compiler check the wires


A **wire** is legal when both ends count the same number of bytes — the source
box's return width against the destination parameter's width. Today that's
checked when the wire is drawn, at run time, and a mismatch kills the program.


Now that a map compiles into construction calls, the generator sees both ends of
every wire in the file. It still can't *compute* a size — only the compiler
can — but it can emit an assertion that makes the compiler do the comparison:


```c

_Static_assert(sizeof(int) == sizeof(int),

               "foo.map:12  adder.0 -> printer.0: widths differ");

```


**One correction to how I posed this before.** I said it risks two checkers that
could disagree. They can't — both compare the same `sizeof`-derived numbers,
one at compile time on the type expressions and one at run time on the values
those expressions produced. The real cost is just more generated code.


The wires drawn *while a program runs* still need the run-time check, since
nothing saw them at build. So this adds a build-time gate in front of an
existing one rather than replacing it.


**[Asked the user]**


*Q1 — Build-time checks:* Should the generator emit compile-time assertions
for what it can see in a map?

- Everything the compiler could check — Not just wires — every type-shaped
  claim a map makes. A comparator's threshold slot against its box's return
  type, a constant's shape against the port it lands on, anything else where the
  generator knows two type names that must agree. Whatever the compiler can
  settle, it settles, and the message names the map line.

- Wires only — The narrowest useful version: one assertion per wire, nothing
  else. Catches the mistake people actually make — connecting two boxes that
  do not fit — without the generator growing opinions about everything else a
  map says.

- Nothing; startup is soon enough — A bad wire in a file already kills the
  program the moment the generated construction runs, which is the first thing
  that happens — loud, immediate, and naming the wire. Adding a build gate
  buys minutes, not correctness, and costs generated lines forever.

→ *(no answer recorded)*


Clear — the build's concern is which functions to compile in, not whether the
graph is complete. Recording that.


Recorded. Now the last one.


## Question four: the runner and repeated compiles


The original version of this — *may a program declare several maps and carry
the union of their boxes* — dissolved when the table did. There's no union to
carry; a program is its own map, compiled.


What's left is about the **general runner**, the tool that takes any map and
compiles it. Two things are undecided there, and the sharper one is what happens
when you hand it the same map twice.


Handing it `foo.map` runs the generator, the compiler, and a load — hundreds
of milliseconds. Run the same unchanged map ten times in a row and that happens
ten times. Since this is a development tool you'd sit in front of while
iterating, the wait is the thing you'd feel.


The catch with fixing it: anything that reuses a previous compile has to decide
when the previous one is stale — which means tracking the map, every box
source it named, and the compiler itself. That's a second thing that can be
wrong, and being wrong means silently running code you already edited.


**[Asked the user]**


*Q1 — Runner caching:* Should the general runner reuse a previous compile when
you hand it the same map again?

- No cache — Every run compiles from scratch. Nothing to invalidate, nothing
  to go stale, no way to silently run code you already changed. Costs the
  compile every single time, including the run where you changed nothing.

- Cache keyed by content — Hash the map and every box source it names, plus
  the compiler's own identity. Same hash means the previous artifact is reused.
  Cannot go stale on a changed file, because a changed file changes the hash —
  it is the content that is the key, not a timestamp.

- Cache, and say when it is used — As above, but the runner prints that it
  reused an artifact and what it was keyed on. Turns an invisible optimization
  into a visible fact, which is the house rule about never letting a shortcut be
  silent. Costs a line of output on every run.

→ *(no answer recorded)*


What would you like to clarify?


Guesses at what might be off:


- **Whether this is the right last question.** You asked for four, and the
  fourth was *may a program declare several maps* — which I decided had
  dissolved and substituted this in its place. That substitution was my call,
  not yours. If you'd rather I put the original question to you, or something
  else that's still nagging, say so.


- **Whether caching is a decision or an implementation detail.** It's a
  development tool's start-up time. You've said elsewhere that build time is
  allowed to be slow and only runtime compilation shouldn't be. A runner's
  compile sits awkwardly between those two — it's a build, but you're waiting
  on it — and I may be inventing a fork where "just don't cache it, we'll see
  if it's annoying" is the whole answer.


- **Whether it collides with incremental compilation.** You said earlier that
  editing a map means an incremental rebuild of the parts that changed. That
  already implies some notion of what's stale, so the runner may just inherit
  whatever mechanism does that rather than needing a cache design of its own.


- **Whether the RAM tier already answers it.** Compiled artifacts go to the
  shared-memory directory, which empties at reboot. So there's a natural
  lifetime already — the question might only be whether anything looks in
  there before compiling, rather than whether a cache exists.


What's on your mind?


--------------------------------------------------------------------------------

### User Request 23

okay we're gonna need more context here. Tell me exactly what you mean by
"general runner" and "workshop" so we can be sure to note whether or not they're
possible.


--------------------------------------------------------------------------------

### Assistant Response 23

I've been saying "workbench" — issue 801, *The workbench in the browser*. If
you meant something else by "workshop," tell me and I'll redirect. Here's
exactly what each one is.


## The workbench


**A page in the documentation site. It draws graphs and writes map files. That
is all it does.**


Concretely, from the blueprint and the note that started it:


- A canvas where you place box stations, drag them around, and draw wires
  between them.

- You type a box's name — and now, its file — and fill in constant values on
  ports.

- A button downloads a map file.

- **It never runs anything.** Your own note is explicit: *"The canvas we're
  creating doesn't run the map files, it simply helps generate them."*

- It holds no C, reads no C, and compiles nothing. The phase was deliberately
  narrowed to that: *the workbench is for layout*.

- Nothing goes to a server; drawings live in browser storage, several on a
  shelf.

- The only compiled-to-WebAssembly piece is the map file **writer**, because
  that is the one thing the page could get subtly wrong.

- It admits in a sentence beside the download button that it checks the shape of
  the graph and not the types, because it cannot see the C.


**Is it possible? Yes, and nothing we decided touches it.** It never needed a
registry and never ran a map. One small consequence: since a station line now
says `math.c:add`, the canvas needs a file field beside the function name —
which the originating note already anticipated.


## The general runner


**I invented this three exchanges ago.** It is not in any earlier blueprint and
was not something you asked for. I proposed it to answer a problem I had
created: once there's no name table, how do you open a map you weren't built
for?


What I meant by it:


```

$ soramech-run foo.map

```


...which runs the generator over that map and the box sources it names, compiles
the result, loads it, and runs it.


**And having now written that down, I think I over-dignified it.** Compare:


```

$ soramech-run foo.map

$ make MAP=foo.map && ./foo

```


Those are the same thing. The "general runner" is a convenience wrapper around
the build you already have. It isn't a new kind of program, it isn't a mode of
the engine, and it probably isn't an issue file — it's a line in the Makefile
or a small script.


**Is it possible? Yes, trivially, because it already is `make`.**


## Which collapses my question four


I was asking whether the runner should cache compiled artifacts and how it would
decide when a cache is stale. **`make` already does that** — dependency
tracking is the whole of what make is. If the runner is a wrapper around the
build, it inherits incremental rebuild for free and there is no cache to design,
no staleness rule to get wrong, and nothing to decide.


So question four dissolves, the same way question two did.


## What I'd do about it


Fold 311e back down. The capability — *run a map the binary wasn't built for*
— is real and is worth stating, but it belongs as a short section in the
map-becomes-code issue saying "this is what `make` already does, invoked later,"
rather than as a separate blueprint describing a tool that doesn't need to
exist.


Unless you want a real command with its own name and behaviour, in which case it
stays and I'd want to know what it should do that `make` doesn't.


--------------------------------------------------------------------------------

### User Request 24

Okay uh... the binary contains the map. The map is just a collection of boxes. A
box could contain a map. So, we could have recursive maps with maps inside them,
because maps are boxes and boxes can be included in maps. To run a map the
binary wasn't built for, we would treat it like any other C function - collect
the functions and such that the newly added (at runtime I'm assuming is what
you're wondering?) and compile them in, then connect the wires and make it work.
Right? What am I missing? A "general runner" is just a map that dynamically
re-wires itself and adds new functions at runtime, which is exactly what we're
trying to implement.


--------------------------------------------------------------------------------

### Assistant Response 24

You're right, and this collapses it properly. Let me confirm the pieces and then
say what I think you're missing.


You're not missing anything. The pieces are all already there, and I'd built a
tool to do a job the engine was designed to do itself.


## Confirming the chain


- **A map is usable as a box** — the input and output station issues exist
  precisely for this, and one of them says a parent *"wires from a sub-program's
  output port exactly as it would wire from any station."*

- **Composition needs no mechanism** — no "finished" to detect, no per-program
  task counting, and **one thread pool serves every program**, already decided.

- **A program can grow while it runs** — stations, ports, and wires, at any
  moment.

- **A box's C can be handed to a running program** and compiled in.

- **A box may reach a map**, since the handle travels as a value on a wire.


So a "general runner" is a map containing about five boxes:


| box | takes | returns |

|---|---|---|

| read a map file | text | a description of stations and wires |

| compile a box source | C text | a placement function |

| add a station | map handle, placement function | a station index |

| draw a wire | map handle, four indices | an acknowledgement |

| write a constant | map handle, station, port, bytes | an acknowledgement |


Wire those together and you have a program that runs other programs. It needs no
tool, no mode, and nothing added to the engine. **311e should be deleted**, not
folded — it described a wrapper around a capability that is the point of the
system.


## Three things that follow, which I should write down


**The map parser doesn't disappear — it becomes a box.** I wrote into the
map-becomes-code issue that *"the engine's runtime map parser is deleted."*
That's still right about the *engine*: nothing in the runtime parses maps. But a
runner-map needs a parsing box, so the parser moves from engine code into a box
source. Same functions, different home — and the generator, which is becoming
a C program anyway, can link the same implementation. One parser, two callers,
neither of them the engine.


**The construction surface needs box wrappers.** Add-a-station, draw-a-wire, and
write-a-constant are C calls today. For a map to perform them they have to be
boxes: plain functions taking values and returning one. That's a small addition
to the construction issue rather than a new design.


**And here's the one real hazard, which I don't think you've hit yet.** Under
comparison-by-width, a wire is legal when both ends count the same number of
bytes. A map handle is a pointer — eight bytes. A placement function is a
pointer — eight bytes. A station index is four. So **any eight-byte value
wires happily into "add a station" where a map handle was meant**, including a
completely unrelated pointer, and the failure is a crash rather than a refusal.


That's the accepted cost of width comparison pointed at something much sharper
than usual: the existing statement of it is *"two structs of the same width and
different layouts already wire without complaint."* True, and mild when the
values are data. Less mild when the value is a pointer the engine is about to
call through. Worth deciding deliberately — wrapping each handle kind in a
struct of a distinctive size would buy the distinction back — rather than
discovering it the first time somebody miswires a self-modifying map.


## One thing I want to verify rather than assert


When a station places a map-as-a-box, do that sub-program's stations live in the
parent's station table or in their own? I've read the fragments that say
composition works and the pool is shared, but not the part that says where the
stations sit — and it matters for how a runner adds stations to a map it is
running rather than to itself.


Want me to go find that, then write up the three items and delete 311e?


--------------------------------------------------------------------------------

### User Request 25

  When a station places a map-as-a-box, do that sub-program's stations live in
  the parent's station table or in their own? I've read the

  fragments that say composition works and the pool is shared, but not the part
  that says where the stations sit — and it matters for how a

  runner adds stations to a map it is running rather than to itself.



I thought we don't have station tables anymore? We're trying to eliminate global
state.


--------------------------------------------------------------------------------

### Assistant Response 25

Let me check the source rather than argue from memory.


Confirmed, and I need to separate two things I let blur together.


## The registry was global. The station table never was.


Here's the map record from the source:


```c

typedef struct map {

    station_t *stations;

    int        n_stations;

    pool_t    *pool;

    ...

    pthread_mutex_t rewire_mutex;

    ...

};

```


The stations live **inside a map**, reached only through a map handle. Nothing
about them is process-wide. The source is emphatic on exactly this point — the
comments there say reaching a map from inside a box *"requires a process-wide
map pointer, and a process-wide map pointer means a process can only ever run
one map,"* and that such a pointer *"deserves the suspicion a global"* gets.


So:


| | scope | status |

|---|---|---|

| the registry | one per **process**, shared by everything | being deleted — this is 311 |

| a station table | one per **map**, a field inside it | stays; it is what a map *is* |


Deleting a map's stations would be deleting the map. What we're eliminating is
the global, and the station table isn't one.


## And that answers my own question


I can derive it rather than go hunting. **A wire is an index** — a station
number and a port number, valid forever, never a pointer. An index is only
meaningful inside one station table. So if a parent wires into a sub-program's
input station *"exactly as it would wire from any station,"* that input station
has to be in the parent's table.


**So composition merges.** A map containing a map produces **one** station table
with both sets of stations in it, joined at the input and output stations that
mark the seam. Maps-inside-maps is real recursion, resolved entirely when the
graph is built, costing nothing at run time. Which fits everything else here.


## Which splits "running another map" into two different acts


- **Compose it** — merge its stations into the map you're already in, wire
  across the seam. One map, one station table, full wiring, no isolation. A
  crash or a wedge is yours too.

- **Start it separately** — a fresh map, its own station table, its own rewire
  lock, sharing the pool. You can't wire to it, because indices don't cross
  maps. You reach it the way anything outside does: by delivering into its input
  station, which the input-station design already allows — *"several callers
  delivering to one program input is several arrows into one port."*


**A runner almost certainly wants the second.** It should be able to run a
broken map without becoming broken, and the input and output stations are
exactly the door for talking to something you didn't merge with.


That's a genuine distinction I hadn't drawn, and it's the thing worth writing
down out of this whole detour — more than the tool I invented.


Shall I write up those two acts, the parser becoming a box, the construction
surface needing box wrappers, and the pointer-width hazard — and delete 311e?


--------------------------------------------------------------------------------

### User Request 26

yeah ok


--------------------------------------------------------------------------------

### Assistant Response 26

Now the pointer-width hazard, which belongs with the width-comparison decision.


Now the parser becoming a box, in the map-becomes-code issue.


Done and committed. Three commits from this session.


## What the last one landed


**311e deleted.** The tool I invented was unnecessary twice: compiling a map is
what `make` already does, and a program that runs other programs is a map, not a
tool. The parent issue keeps a short note saying why there's no fifth child, so
nobody re-proposes it.


**Composing and starting, separated** — written into the construction issue.
The distinction falls out of what a wire is:


- **Composing merges.** A wire is a station index and a port index, meaningful
  only inside one station table. So a map containing a map produces *one* table
  with both sets of stations, joined at the input and output stations. Recursion
  is real and resolves entirely when the graph is built — no boundary checks,
  no per-program bookkeeping, nothing at run time.

- **Starting is separate.** A fresh map, its own station table, its own rewiring
  lock, sharing the pool. You can't wire to it; you deliver into its input
  station, which is the door that already exists for anything outside. A program
  that runs other programs wants this, because it should survive what it runs
  dying.


**The parser becomes a box.** Nothing in the *engine* parses maps, but a
runner-map needs a box that reads one — so the parsing functions move out of
the engine into a box source, and the generator links the same implementation.
One parser, two callers, neither of them the engine.


**Box wrappers for the construction surface** — add a station, draw a wire,
write a constant, each a plain function taking values and returning one. That's
what lets a map build a map without anything added to the engine.


**The handle hazard, named.** Wires are checked by counting bytes. That cost was
accepted when everything flowing was data — a bad wire gave a wrong number.
Once programs build programs, some values are pointers the engine calls through,
and a map handle, a placement function, a `double`, and a file offset are all
eight bytes. Nothing proposed: the cheap fix would make the type system lie
about sizes to buy back a distinction, which is worse than the thing it
prevents. But it's written down in the width-comparison issue as a decision
waiting rather than a discovery pending.


**One correction on terminology.** The registry was global — one per process.
A station table isn't, and never was: it's a field inside `map_t`, reached only
through a handle. Deleting it would be deleting the map. I'd blurred the two.


## Where the four questions ended


| | outcome |

|---|---|

| the numbering | answered — index means reading order only; renumbering filed at very low priority |

| how a station names its box | dissolved — the placement function writes the name literal; no table to index |

| build-time wire checks | answered — no; a half-wired map is legitimate, and the build's business is which functions to compile |

| several maps declared | dissolved — no union to carry; a program is its own map |


Everything verified: 113 pages, zero unresolved references, all tests pass.


--------------------------------------------------------------------------------

### User Request 27

  The parser becomes a box. Nothing in the engine parses maps, but a runner-map
  needs a box that reads one — so the parsing functions move

  out of the engine into a box source, and the generator links the same
  implementation. One parser, two callers, neither of them the engine.


can you tell me more about this? background and context and reasoning and such?


--------------------------------------------------------------------------------

### Assistant Response 27

Let me pull the actual pieces first — what the parser is today, and what an
iterator does, since the second bears on the interesting part.


Good question to push on — I asserted that more confidently than I'd earned,
and there's a real unsettled piece inside it.


## What the parser is today


Two files, with a clean division of labour:


- **`041-mapfile.c` — the parser.** Reads text into a *description* and
  **constructs nothing**. Its own notes put it well: whether the map makes sense
  is the loader's question; whether it is *well-formed* dies here, with file and
  line. It handles the keyword dispatch — `in` and `out` attach to whichever
  station line is open, `statics` opens the numbered section, anything else must
  be a three-word station line. Duplicate names, trailing words, dangling `in`
  lines: all refused by name.

- **`042-loader.c` — the loader.** Takes that description and builds. First
  pass creates every station from the registry; second pass resolves arrows by
  name, which is *why* there are two passes — forward references. Then
  whole-map validation, collected and reported together.


Both live in the engine and run when a program starts.


## Why they were going to be deleted


Once the generator reads the map at build time and emits `build_program()` with
direct pointers, a shipped program never sees map text. The parser and loader
become dead weight — carried by every binary, used by none.


## Why they can't just be deleted


Your point about the runner. A program that runs other programs has to read a
map file at run time. If the only parser lives in the generator, that's
impossible.


## Three possible homes, and why a box wins


| home | what it costs |

|---|---|

| **stay in the engine** | every shipped program carries a parser it never calls. Worse, it's a capability present whether or not anyone asked — the exact shape we spent this session removing from the registry. |

| **generator only** | a running program cannot read a map, so a program that runs programs is impossible. |

| **a box** | a program that wants to read maps places a station running it. A program that doesn't, doesn't. |


The box wins, and the linker work from the map-becomes-code issue is what makes
it free: with `--gc-sections`, code nothing reaches is discarded. **A parser
nobody places is a parser nobody ships.** No flag, no declaration, no
configuration — the same rigidity you asked for, arrived at by the linker
rather than by a setting.


It also fits the architecture's own shape: the engine is small, capability lives
in boxes. And it makes the parser testable the way a box is testable — hand it
text, look at the value that comes out.


## Why one implementation


Two things need to read map text: the generator, at build time, and this box, at
run time. If those were separate implementations they would **drift**, and the
failure mode is nasty — a map the generator accepts and the box refuses, or
worse, one that both accept and read differently. That's two sources of truth
about a grammar, which is exactly the disease the "map carries no types" rule
exists to prevent elsewhere.


So: one set of functions, compiled twice. Linked into the generator binary;
compiled as a box source into whatever program places it. Neither copy is *in*
the engine.


## The part I glossed, which is genuinely open


**What shape is the box?** A box is a plain C function taking values and
returning **one** value, remembering nothing between calls. A map file describes
an arbitrary number of stations and wires. Those two facts don't obviously fit
together, and I waved at it.


Three shapes, none of them chosen:


- **One big box that does everything.** `load_map(map_handle, path) → result`.
  Reads, parses, compiles what it names, places stations, draws wires — all
  inside one invocation, returning whether it worked. Legitimate: takes values,
  returns one, remembers nothing. Big, but boxes are allowed to be big. Costs
  the graph any visibility into the work.

- **A box returning a description by pointer**, with later boxes consuming it
  and a final box freeing it. Ownership travels on a wire. But that's a pointer
  flowing through a system that checks wires by counting bytes — **exactly the
  hazard I named two commits ago**.

- **Split into several boxes with the iteration in the graph.** Most in keeping
  with the project. But I need to correct my own instinct here: I was about to
  suggest an iterator station does this, and it doesn't. An iterator sends each
  *successive value* down the *next port*, rotating — it's a **spreader**, not
  a loop over a collection. Pulling N stations out of one file isn't what it
  does.


There's also a wrinkle underneath all three: today's loader creates stations
*from the registry*, and the registry is being deleted. So a runtime load box
has to compile the boxes a map names before it can place them — meaning it
calls the runtime-compilation path, not just the parser.


That's the honest state of it. Want me to record the box-shape question in the
map-becomes-code issue, since I stated it as settled when it isn't?


--------------------------------------------------------------------------------

### User Request 28

okay but why is the parser it's own box? is there a use-case for it being
separate? or could we just have "add_a_box()" as a possible function that can be
added to a box station and other similars? More abstraction if we can't think of
other use-cases.


--------------------------------------------------------------------------------

### Assistant Response 28

No, there isn't a good use case, and you've caught a real error in how I phrased
it.


## Correcting myself: the parser isn't a box


What I said was "the parser becomes a box." That's wrong. The parser is a set of
C functions that turn text into a description. What should be a box is the
**loader** — and the parser is just C it calls.


So:


- **`041-mapfile.c`'s functions** — ordinary C, living in a source file
  outside the engine. Not a box. Not reachable by a wire.

- **Two things link them**: the generator, at build time, and a box source, at
  run time.

- **The box is `load_map(map_handle, path) → result`** — reads the file,
  parses it, compiles whatever boxes it names, places the stations, draws the
  wires, returns whether it worked.


One box, whole job. The parser is an implementation detail inside it, the way
`memcpy` is an implementation detail inside delivery.


## And that kills the hard problem


The thing I flagged as unsettled — *what does a parsing box return, given a
map describes arbitrarily many stations and a box returns one value* — **only
existed because I'd split it.** A description flowing on a wire needs to be a
value, which means a pointer, which is the eight-byte hazard, which needs an
owner, which needs a freeing box. All of that was manufactured by a split nobody
needed.


Inside one box, the description is a local variable. It's built, walked, and
freed before the box returns. Nothing crosses a wire.


## The use cases for separating, and why they're thin


I looked for them honestly:


- **Validating a map without running it** — a linter. But the build already
  does this, at build time, where it belongs.

- **Transforming a map** — read one, rename things, write it out. Speculative;
  nobody has asked.

- **Displaying a map** — a viewer. The dump already produces text, and the
  workbench draws from its own storage.


None of them justify the cost, and the cost is real: every split point is a
wire, wires here carry pointers, and pointers through a width-checked system are
the hazard we just wrote down. **More abstraction, as you say** — keep it
coarse until something demands otherwise.


## So the boxes are two independent things


| box | for |

|---|---|

| `load_map(handle, path) → result` | "run this file" — the whole job in one invocation |

| `add_station`, `draw_wire`, `write_constant` | "build a graph from values I computed" — the construction surface, wrapped |


**Neither needs the other, and neither needs a shared description on a wire.**
The load box calls the same underlying C functions the construction boxes wrap.
A program that runs map files places the first; a program that assembles graphs
from computed values places the others; a program doing both places both.


One consequence worth naming: **`load_map` is a slow box.** It compiles, so it
holds a worker for hundreds of milliseconds. Nothing preempts a box here — it
runs to completion — so on a machine with several workers that's fine, and on
a single-worker pool it stalls everything. Worth stating rather than
discovering.


Let me correct the map-becomes-code issue to say this, and record the coarseness
decision with the reasoning, since the split I described is already committed.


--------------------------------------------------------------------------------

### User Request 29

  ┌────────────────────────────────────────┬────────────────────────────────────────────────────────────────────────────┐

  │ box │ for │

  ├────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────┤

  │ load_map(handle, path) → result │ "run this file" — the whole job in
  one invocation │

  ├────────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────┤

  │ add_station, draw_wire, write_constant │ "build a graph from values I
  computed" — the construction surface, wrapped │

  └────────────────────────────────────────┴────────────────────────────────────────────────────────────────────────────┘



are these four functions the only functions in our "standard library"? What's
the difference between load_map() and add_station()? We'll also need
"remove_station()"


--------------------------------------------------------------------------------

### Assistant Response 29

Let me check what the construction surface actually is, and what's settled about
removal.


Three separate things there. Let me take them in order, and correct my own table
first.


## The surface is four operations, and I named them badly


From the construction issue:


> There is one surface, and it has four operations: **create an empty program,
> add a station, configure an input port, and connect or disconnect a wire.**


So my table was wrong twice. `write_constant` isn't its own operation —
binding a value onto a port *is* configuring that port, alongside making it a
ring buffer or setting it to no-source-yet. And I omitted "create an empty
program" and the disconnect half of wiring.


## `load_map` and `add_station` aren't peers


**The difference is scope, not kind.** `add_station` is one operation on one
station. `load_map` performs *many* of them — possibly hundreds of adds and
wires — driven by a file, plus compiling boxes the program doesn't already
carry.


So it's two tiers, not two columns:


| tier | what |

|---|---|

| **the surface** | create a program · add a station · configure a port · connect or disconnect a wire |

| **built on it** | `load_map(handle, path)` — read a file, compile what it names, then perform a great many of the four |


`load_map` sits above the surface and calls it, the way the generated
`build_program()` does. It isn't a fifth operation.


## `remove_station()` collides with a standing guarantee


Guarantee **S2**: *a wire is an index, never a pointer; a wire written down
today is valid forever* — held by "stations can never be removed or reordered,
only added." The reason is mechanical: remove station 5 and you either leave a
**hole** every walk must skip forever, or **renumber** and invalidate every wire
pointing past 5 at once.


But you can have most of what you want without touching that, because
**disconnecting is already in the surface**:


- Set every input port to *none* → the station can never become ready, so it
  can never run.

- Remove every destination from its output ports → nothing downstream hears
  from it.


That's removal in every observable sense. What's left is **memory** — and the
machinery for reclaiming it already exists in the destinations-without-a-lock
issue: mark it retired, sweep until no worker is inside it, then free. Applied
here, that frees the **buffers** while the slot stays. The slot is a hundred-odd
bytes; a port's ring cells can be megabytes. So you reclaim nearly everything
and no index ever moves.


So `remove_station(handle, index)` can exist and mean: *disconnect everything,
retire the buffers, keep the slot.* No guarantee broken.


**What real removal would cost**, if you ever want the slot back: a wire becomes
`(index, generation)` and removing bumps the generation, so a stale wire fails a
check and is refused loudly instead of silently hitting whatever got put there
instead. That's four bytes per wire and a comparison **on every delivery** —
hot-path cost, paid forever, for an operation that happens rarely. I'd not take
it, but it's the honest price and it should be written down rather than left as
"impossible."


## And no, these aren't a standard library


They're the engine's own construction surface exposed as boxes — not a
convenience library. The distinction matters because a surface has to be
complete and rigid, while a library is whatever people found useful.


**This project has no standard library concept**, and the one time it came up it
was deliberately cut. Your original workbench note asked for *"some of our own
functions in a drawer to the side… pretty common ones that interface with some
of the software that I've made / hardware that I've worked with."* The workbench
issue records the drawer being removed when the phase narrowed to layout:
*"There is no drawer, and no upload either."*


So whether there should be a standard library — and what would be in it — is
genuinely unanswered. It's a separate question from this one, and worth its own
issue if you want it.


Shall I record all four of these: the corrected surface, the two tiers,
`remove_station` as disconnect-and-retire with the generation-counter
alternative named, and the standard-library question as open?


--------------------------------------------------------------------------------

### User Request 30

  Guarantee S2: a wire is an index, never a pointer; a wire written down today
  is valid forever — held by "stations can never be removed or

  reordered, only added." The reason is mechanical: remove station 5 and you
  either leave a hole every walk must skip forever, or renumber

  and invalidate every wire pointing past 5 at once.



um this guarantee is not something we want to keep. We want to be able to fully
re-configure our programs at runtime. We have more than one way to deal with
holes in arrays - can you give me a list of possible solutions? If we don't
remove stations, we will have memory leaks. Not ideal. Can you tell me any other
possible memory leaks?


  What real removal would cost, if you ever want the slot back: a wire becomes
  (index, generation) and removing bumps the generation, so a

  stale wire fails a check and is refused loudly instead of silently hitting
  whatever got put there instead. That's four bytes per wire and a

  comparison on every delivery — hot-path cost, paid forever, for an operation
  that happens rarely. I'd not take it, but it's the honest

  price and it should be written down rather than left as "impossible."



um, what is "generation" here? why would we need it?


--------------------------------------------------------------------------------

### Assistant Response 30

Let me check one thing first, because it changes how bad holes actually are.


Checked: nothing walks the station table during delivery. Every `n_stations`
reference on that path is a **bounds check**; the only full walks are the dump,
loading, and teardown — all cold. So my "a hole every walk must skip forever"
was overstated, and that changes the answer.


## What "generation" means, and why you'd need one


It solves a problem that only exists if you **reuse** slots.


Station 5 is removed and its slot goes free. Later a new station is created and
takes slot 5. Now two kinds of wire point at "station 5": old ones that meant
the removed station, and new ones that mean the new one. **A wire is just the
number 5** — the engine cannot tell them apart, so an old wire silently
delivers into the new station. Not a crash. A wrong answer that looks right.


A generation counter is a number on the slot that increments each time the slot
is reused. A wire stores `(index 5, generation 3)`, and delivery asks: *does
slot 5 still have generation 3?* If not, this wire is stale, and it's refused
loudly instead of misdelivering.


This is the ABA problem — a thing changed from A to B and back to A, and a
stale observer can't tell "nothing happened" from "everything happened."


**So the generation is the price of reuse, not the price of removal.** Never
reuse a slot and an index means the same station forever, and there is nothing
to disambiguate.


## Solutions for holes, with what each actually costs


| | what it does | hot-path cost | reclaims the slot? |

|---|---|---|---|

| **A. Tombstone** | mark dead, free the buffers, keep the slot forever | **none** | no |

| **B. Reuse after proving it's safe** | at removal, sweep every station's destinations; if nothing points at slot 5, it's free to reuse with no tag | **none** — cost is at removal | yes |

| **C. Generation counters** | reuse freely, tag every wire, check on delivery | one comparison per delivery, forever | yes |

| **D. Compaction at a quiet moment** | drain the pool, shift stations down, rewrite every wire | none while running | yes, all of them |

| **E. Handle indirection** | a wire holds a handle; a table maps handle → slot | a pointer chase per delivery | yes |


**B is the interesting one and I don't think it's been considered here.** The
only things that point at a station are the destination lists on other stations'
output ports. That's walkable — every station, every output port, every
destination. It's O(stations × destinations) at removal time, which is a rare
operation, and it buys slot reuse with **zero** hot-path cost. If any wire still
points there, you refuse the removal and name the wire, which is more useful
than silently allowing a dangling reference anyway.


**D deserves a mention because you already have the machinery.** The capture
design drains the pool and waits for quiet. Compaction at that same moment is
nearly free — nothing is running, so rewriting wires is safe. It's the one
option that reclaims *everything* including fragmentation.


**A is where I'd start**, because it's already almost true and costs nothing:
the surface can disconnect a station today, and the retire-sweep-free machinery
frees the buffers. A slot is a hundred-odd bytes; a port's ring can be
megabytes. Then B or D when the slot leak actually bites.


## Other memory leaks I can find


Some confirmed from the designs, some I'd want to check in the code:


| what | why it accumulates | confidence |

|---|---|---|

| **ring buffer pages** | growth adds a page and nothing ever shrinks. A port that spiked to ten thousand deep stays that deep for the life of the program, holding memory for a burst that ended hours ago. | high — the growth design has no shrink path |

| **station slots** | the one we're discussing | high |

| **retired destination arrays** | rewiring rebuilds the array and files the old one for sweeping. If the sweep never runs — no worker ever exits a delivery walk — the list grows. | high |

| **compiled shared objects** | a box can be unloaded only if no station uses it. Compile a replacement for a placed box and the old code stays mapped forever. | high |

| **box source in the RAM tier** | every runtime-compiled box writes its source there at creation and nothing removes it. Bounded by reboot rather than by the program. | high |

| **station names** | one string per station, held for the dump's benefit, freed only when the map is | high |

| **sub-maps** | a program that starts other maps and never tears them down holds all of them | needs checking — I don't know if map teardown is reachable from a box |


The first one may be worse than the station slots, incidentally: a station slot
is a fixed hundred-odd bytes, but a ring that grew to absorb one bad afternoon
holds that memory permanently, and there are as many rings as there are input
ports.


Want me to write this up — the guarantee changing, the five options with B and
D drawn out, and the leak inventory as its own issue?


--------------------------------------------------------------------------------

### User Request 31

wait hold on. What array are we even walking here? We don't have a global
registry anymore remember? so, a wire being an index... into what? a station has
wires, and those wires are just indexes, but what do those indices refer to?


--------------------------------------------------------------------------------

### Assistant Response 31

Right — I've been using "table" for two completely different things. Let me
draw the actual memory.


## The array is a map's own station array


```

map_t

  ├─ pool ──────────> (shared with every other map)

  └─ stations ─────>  ┌──────────┬──────────┬──────────┬──────────┐

                      │station 0 │station 1 │station 2 │station 3 │

                      └──────────┴─────┬────┴──────────┴─────▲────┘

                                       │                     │

                         station 1's output port has a       │

                         destination record:                 │

                              { station: 3, slot: 0 }  ──────┘

                                       │

                         meaning: write into

                              stations[3].slots[0]

```


From the source, a wire is literally this:


```c

typedef struct destination {

    int32_t station;   /* index into map.stations   */

    int32_t slot;      /* which input port of it    */

    struct destination *next;

};

```


So when station 1's box returns, delivery reads that record and copies the value
into `stations[3].slots[0]`. **The index is into `map.stations`** — the array
of stations that *is* the map. One per map, living inside `map_t`, reached only
through a map handle.


A naming trap worth knowing, since the code says both: **`slots` are a station's
input ports** and **`ports` are its output ports.** The port record issue calls
this out as *"a naming debt this family does not pay off."*


## And the registry is unrelated


| | what it maps | scope | status |

|---|---|---|---|

| **the registry** | a *name* like `math.c:add` → a box's shim and sizes | one per **process** | being deleted — nothing needs it once the generator resolves names |

| **`map.stations`** | an *index* like `3` → a station | one per **map** | stays; deleting it would delete the map |


They were never the same mechanism. The registry answered *"what is `add`?"* at
load time. The station array answers *"where do I put this value?"* on every
single delivery.


## So the removal problem, restated correctly


Remove station 2 from a four-station map and you're changing `map.stations`. Two
choices:


- **Leave a hole.** Index 3 is still 3, so that `{3, 0}` destination record
  still points at the right station. Nothing else breaks. The cost is a dead
  slot sitting there.

- **Shift everything down.** Station 3 becomes station 2 — and that `{3, 0}`
  record now points past the end, or at whatever moved into 3. Every destination
  record in the whole map that names a station above 2 is now wrong, and there
  is no cheap way to find them all mid-flight.


That's the entire difficulty, and it's local to one map. Nothing global is
involved, and none of it survives or falls with the registry.


Does that clear it up? If so I'll write up the five options against this
picture, and the leak inventory.


--------------------------------------------------------------------------------

### User Request 32

  A naming trap worth knowing, since the code says both: slots are a station's
  input ports and ports are its output ports. The port record

  issue calls this out as "a naming debt this family does not pay off."


this is wrong, we should correct this and hold no debts. A port is either an
input or an output port, and ports hold information about how to store data or
pass it along - shims live in output ports I think. Input ports hold ring
buffers. The actual spot where the data is stored is the slot. Please correct
this. A port might have many slots, for example, ring buffers hold many slots.


  So the removal problem, restated correctly


  Remove station 2 from a four-station map and you're changing map.stations.


why don't we just re-use the map.stations slot for the newly added stations?
Because old wires might point to it. Can you tell me what the station deleting
process looks like? Or could look like? Because we want to remove all the wires
when we do so - if we fully disconnect any wires that are pointing to it, then
there should be no problem in re-using the map.stations slot. The slot is a
fixed size, right? Like, a "larger" station wouldn't take more size? Well, even
if it did, that's okay, because we can just walk map.stations until we find an
open spot that's large enough to hold the newly added station.


--------------------------------------------------------------------------------

### Assistant Response 32

Both good. Taking them in order.


## The vocabulary correction


You're right, and the documentation already agrees with you — it's the
**source** that's wrong. The port record issue defines it exactly as you just
did:


> **A port** is the standing interface for one input of one station… **A
> slot** is one place where one value physically sits — a cell inside a port's
> ring buffer, or the bytes reserved for one argument inside a task. **The port
> decides how a value is stored; the slot is where it lands.**


And then admits: *"The source calls the port a slot, which is a naming debt this
family does not pay off."* So the debt is real and it's in the C. Here's what
paying it means:


| code today | what it actually is | should be |

|---|---|---|

| `slot_t *slots` on a station | its **input ports** | ports |

| `port_t *ports` on a station | its **output ports** | ports |

| "cell", inside a ring buffer | one storage spot | **slot** |

| `{station, slot}` in a destination | names an **input port**, not a slot | port |


So a station has input ports and output ports, both called ports. An input port
owns a ring buffer, which holds many **slots**. A task's argument area also
holds slots. That's your definition exactly.


**One correction to your guess: the shim doesn't live in an output port.** It's
a field on the station itself — `task_call_t call` — which is right, because
a station places one box and a box has one shim. What output ports carry is
where the result *goes*: one for a plain box, three for a comparator, as many as
you wire for an iterator.


The rename reaches the station header, delivery, statics, the loader, the dump,
and a document whose **filename** contains the wrong word —
`002-stations-and-slots.md`. Worth doing deliberately in one pass, which is
exactly why it hasn't happened by accident.


## Station slots are fixed size


Confirmed, and it's deliberate. The source says so: *"Fixed-size on purpose: the
array of these must stay indexable, and growing a buffer must never move a
station."* Everything variable — the ports, the buffers, the destination lists
— is **pointed to**, not stored inline. So every station occupies exactly the
same bytes and there is no such thing as a larger one. Your fallback of walking
for a big-enough hole isn't needed; any free slot fits any station.


## What deleting a station looks like


Your reasoning is right, and it removes the need for a generation counter
entirely. Here's the sequence:


1. **Take the rewiring lock.** Deleting and wiring must not interleave, or
   someone draws a wire to a station being deleted.

2. **Find every wire pointing at it.** A wire only ever exists as a destination
   record on some station's output port, so walking every station × every
   output port × every destination finds all of them. Nothing else in the
   engine names a station.

3. **Remove those records.** Now nothing can deliver into it.

4. **Remove its own outgoing destinations**, so anything it has already produced
   goes nowhere.

5. **Wait until no worker is inside it.** This is the only subtle step: a task
   built from this station may already be running, and when it finishes it will
   touch this station's counters and output ports; a delivery may be mid-flight
   into its buffers. The runtime-compilation issue already designs the mechanism
   — one counter per worker, bumped at the start and end of a whole task,
   swept until everyone has crossed a boundary.

6. **Free its ports, its buffers, its name.**

7. **Mark the slot free** for the next station.


**And that's why no generation counter is needed.** The counter exists to catch
a *stale* wire — one written before the slot was reused. Step 3 guarantees no
such wire survives, because removal is what removes them. There is nothing stale
to catch.


The guarantee doesn't get weaker, it gets **sharper**. Today: *a wire written
down today is valid forever*, held by never removing anything. It becomes: **a
wire never names a station that isn't there**, held by removal removing the
wires first. That's a stronger statement and it costs nothing on the delivery
path.


## The one gap I can see


A **caller outside the map**. The input station accepts deliveries from outside
— several callers into one program input — and the engine deliberately does
not remember who called. So if you delete an input station while an outside
caller still holds a reference to it, nothing in step 2 finds that caller,
because it isn't a wire.


That's not fatal, and there are two honest answers: don't delete a station that
outside callers use, or have outside delivery take the rewiring lock and
re-check that its target still exists. The second costs a lock on a path that
already isn't hot. Worth deciding rather than leaving.


Shall I write all of this up — the naming debt paid, the deletion sequence,
the guarantee restated, the outside-caller gap, and the leak inventory?


--------------------------------------------------------------------------------

### User Request 33

  A caller outside the map. The input station accepts deliveries from outside
  — several callers into one program input — and the engine

  deliberately does not remember who called. So if you delete an input station
  while an outside caller still holds a reference to it, nothing

  in step 2 finds that caller, because it isn't a wire.



let's just say that it's undefined behavior if you do something like this. We
should have a guarantee that maps with sub-maps can only interface with one
another through their input and output ports, unless you do something weird and
break the engine to pass pointers and such. Then you can write down everything
except the memory leaks - let's circle back to those.


--------------------------------------------------------------------------------

### Assistant Response 33

Clear. Writing it all up now, leaving the leaks for you.


Now the guarantees page — S2 restated, and the interface guarantee added.


Now the issues that assert stations can't be removed.


Now 211 and 212 — the table gains a shrink, and the surface gains an
operation.


Let me fix that block cleanly.


Now the phase 2 progress page and a build check.


Written and committed as `39d86a4`. Tests pass, 115 pages, zero unresolved
references.


## What landed


**Two new blueprints in phase 2:**


- **215 — ports and slots.** The naming debt, scheduled. Your definition is
  what every document already says; it's the C that's backwards. The mapping:
  what the code calls `slots` on a station are its **input ports**, what it
  calls `ports` are its **output ports**, and what it calls a *cell* inside a
  ring buffer is the actual **slot**. So the true sentence *"a port has many
  slots"* currently can't be said without contradicting the code. One correction
  to your guess — the shim isn't in an output port, it's a field on the
  station, because a station places one box and a box has one shim. Output ports
  carry only where the result goes. The rename reaches the header, delivery,
  statics, the loader, the dump, the error messages, and the filename
  `002-stations-and-slots.md`.


- **216 — removing a station.** The sequence you described, written out: take
  the rewiring lock, walk every station's output ports to find every wire naming
  the target, cut them, cut its own outgoing wires, wait until no worker is
  inside it, free it, mark the slot free. Station records are fixed-size on
  purpose — confirmed in the source — so any free slot fits any station, and
  your "walk until we find one large enough" isn't needed.


**Why no version tag is needed**, recorded in full: a generation counter exists
to catch a wire written *before* a slot changed hands. Cutting the wires first
makes such a wire impossible, so there's nothing to catch — saving four bytes
per wire and a comparison on every delivery, forever, in exchange for a walk
during a rare operation.


**Guarantee S2 replaced, and it got stronger.** Was: *a wire written down today
is valid forever*, held by never removing anything. Now: **a wire never names a
station that is not there**, held by removal removing the wires first.


**Guarantee S7 added:** *a program is reached only through its input and output
stations.* Anything else held across a boundary — a station index, a pointer
into buffers — is undefined. That's what makes the wire walk sufficient, since
a wire lives on an output port and can be found while an outside caller cannot.


**Three issues updated to match:** the growth issue becomes the growing half of
a table that also shrinks and prefers a free slot before adding a shelf; the
construction surface goes from four operations to five; and the
runtime-compilation issue's reasoning about unloading boxes gets much stronger
— it had assumed a station could never be removed, so unloading only ever
served a box that was compiled and never placed. Both also share their hardest
step, the wait until no worker is inside the thing being freed, which should be
built once.


## Left for you


The memory leak inventory. I surveyed seven candidates and the one I'd put first
isn't station slots — it's **ring buffers that grow and never shrink**. A slot
is a fixed hundred-odd bytes; a port that grew to absorb one bad afternoon holds
that memory permanently, and there are as many rings as there are input ports.


--------------------------------------------------------------------------------

### User Request 34

okay can you give me a roadmap for the issue files?


--------------------------------------------------------------------------------

### Assistant Response 34

Let me verify the blockers before I draw it.


Thirty-two open files, two of which are parents with no work of their own.
Here's the shape.


## Two fronts that don't block each other


```

FRONT A — the port and the station          FRONT B — the build path


  401 statics on the port                     308 generator, in C

       │                                           │

       ▼                                           ▼

  210b the port record ──┐                    309 types by width

       │                 │                         │

       ▼                 ▼                         ▼

  210c cell states    210f converting         310 boxes at runtime

       │                 │                         │

       ▼                 │                         ▼

  210d copies leave      │                    311a addressed by file

       │  the lock       │                         │

       ▼                 │                    ┌────┴────┐

  210e growth adds ──────┤                    ▼         ▼

       a page            │                  311b      311c

                         ▼                  placement  source in

                    210g build a station     │         the binary

                         │                   └────┬────┘

                         ▼                        ▼

                    211 grow the table       311d the map becomes code

                         │

                         ▼

                    212 build a program

                         │

                         ▼

                    216 remove a station ◄──── needs 310's quiescence sweep

```


**Front A's real head is 401** — statics moving onto the port. Half the port
family waits on it, because a port can't be given room for a constant while a
global register still owns the bytes. The ring-buffer half (`210b`'s cells, then
`210c`→`210d`→`210e`) is *unblocked today* and touches no static at any
point.


**Front B's head is 308** — the generator in C. Everything downstream needs
the parser callable outside a build.


## What is ready to start right now


| issue | why it's free |

|---|---|

| **401 — statics on the port** | nothing blocks it, and it unblocks the most |

| **308 — the generator, in C** | nothing blocks it, and it heads the whole build front |

| **309 — types by width** | independent; a hard prerequisite for 310 |

| **210b (ring half)** | the cells and the *none* tag need no static |

| **214 — destinations without a lock** | independent of everything |

| **211 — growing the station table** | independent |

| **213 / 209 — the input and output stations** | independent; they're what make a map a box |

| **106 — stopping on purpose** | independent, and 712 later needs its drain |

| **705 — the HTML set** | independent; 709 and 801 both wait on it |

| **710 — demos after the pull path** | the pull path is already gone |


## The convergence, and what it unlocks


`212` and `311d` are the two capstones, and together they are the thing this
project is for: **212** makes a program constructible while it runs, **311d**
makes a map into code with no name lookup anywhere. After both:


- `216` — removing a station (also needs 310's sweep)

- `712` — capturing a running program (also needs 106's drain)

- `801` — the workbench, which emits maps that 311d compiles


## Three scheduling notes worth having


**215 (the renaming) has a window, and it's now or much later.** It touches the
station header, delivery, statics, the loader, the dump, and a filename. Doing
it *before* `210b` means every issue after uses the right words. Doing it
*during* the port family means renaming records that are simultaneously being
restructured — pure churn. If it doesn't go first, it should wait until after
`210e`.


**Two cross-front dependencies are easy to miss:** `216` needs the per-worker
quiescence counter that `310` designs, and `712` needs the drain that `106`
designs. Neither is in its own front. Both should be built once and shared
rather than twice.


**711 is marked very low priority and should stay there.** Nothing waits on it
and nothing is broken.


## If I were picking an order


1. **401**, because it unblocks the most and is currently the single biggest
   jam.

2. **308**, in parallel — different files, different front, no contention.

3. **210b** complete, then **210f**, then **210g** — the port family closing.

4. **309** → **310**, which delivers the quiescence sweep 216 needs.

5. **211** → **212** — the construction capstone.

6. **311a** → **311b**/**311c** → **311d** — the build capstone.

7. **216**, **712**, then presentation: **705** → **709**/**801**.


Want me to file this as a document? It'd sit beside the existing roadmap, which
describes the phases as clusters of functionality rather than an order of work
— this is the other view, and the two don't conflict.


--------------------------------------------------------------------------------

### User Request 35

okay can you add 401 and 308 to your todo list and then work on them until
they're done?


--------------------------------------------------------------------------------

### Assistant Response 35

Let me read 401 completely before touching anything.


**401 says "Done."** Its current-behavior section opens with it. Let me verify
against the code rather than take the file's word.


**401 is already built.** All seven of the tests it specifies exist and pass:


```

== 035-test-statics

  a static fed 50 runs without ever being consumed

  an all-static station is never woken by delivery

  brace text became bytes identical to a compiled initializer

  four malformed statics each died where they were given

  4000 claims raced a writer through the station's own mutex: zero torn, last write visible

  two ports given one value are independent afterwards

  two maps ran side by side and did not see each other

```


The port record carries `constant`, `constant_string`, `constant_set`; the
header says outright *"There was a statics table here and it is gone (issue
401)"*; and the bytes-to-text formatter exists as `slot_constant_text`. The one
thing its file lists as not done — an arrow delivering into a static port —
it explicitly assigns to 405.


So it's finished work sitting in the wrong directory. Let me complete the
paperwork.


401 being done clears a blocker that two other issues still describe as active.
Let me fix those.


401 filed. Now 308 — the generator in C. Let me read it and size the job.


683 lines of Lua to translate. Let me get the full context — the build, the
layout, and the generator itself.


Now the generator itself. Reading it in full.


Now the existing generator tests, since they're the specification for parity.


I have the full picture. Let me check the index counter and start building.


Counter is stale — `tests/063` and `064` exist. Bumping and starting at 065.


Now a test for the support machinery, before anything builds on it.


Support machinery holds. Now the parser.


Now the emitter, matching the Lua output exactly.


Now main, and then a parity check against the Lua generator.


Compiles clean. Now the parity check against the Lua generator.


Both modes byte-identical. Now the error-conduct tests, which are the harder
specification.


All conduct cases pass. Now let me compare refusal messages across a batch of
malformed sources.


Only two differences, both improvements: exit code 65 instead of 1 (matching the
project's own code table), and the unterminated-comment error now names the file
instead of `(input):0`. Now the build wiring.


Two seam errors. Fixing.


Now update the CLI test to drive the C generator, and add the parity test.


Clean build, all tests pass, byte-identical output. Now the paperwork. Let me
check the info-file format first.


All tests pass. Now the issue paperwork — 308's current behavior.


Now the second commit — retiring the deprecated script and the parity test
that guarded it.


One orphan page from the generator's removal — the gap I recorded in 705. Let
me sweep for any others.


More orphans than my check caught — from this session's renames. Cleanest fix
is to regenerate the whole set.


Both issues are done and filed. Five commits.


## 401 — statics on the port


**It was already built.** The port record carries `constant`, `constant_string`,
`constant_set`; the station header says outright *"There was a statics table
here and it is gone"*; and all seven tests the issue specifies pass. It was
finished work sitting in the wrong directory.


What it had bought, now recorded as complete: two ports given the same file
entry are strangers afterwards (they used to share bytes shaped by whichever
bound first, so two ports of different types could each read one value their own
way); everything one invocation reads is true at one instant (a constant used to
be fetched after the station's lock was released, behind a second lock that
couldn't nest inside the first); and a process can hold several running programs
that can't see each other, which the shared table had made impossible.


**It was also blocking half the port family**, and two issues still described
that blocker as live. Cleared. The deadlock is worth keeping: two pieces each
waited for the other to provide the room, and it broke when somebody noticed
that *whoever deletes a thing is the one who has to know what replaces it* —
so the deleter built the storage it spends rather than waiting to be handed it.


## 308 — the generator in C


**Built, proven, and the Lua script retired.** 683 lines of Lua became four C
files:


| file | what |

|---|---|

| `065/066-gentext` | arena, growable string, growable array, string helpers |

| `067/068-genparse` | comment blanking, declaration parsing, validation |

| `069-genemit` | comparisons, shims, field tables, the registry |

| `070-generate` | the front door |


**Parity was proven byte for byte, not asserted:** 259 lines of emitted registry
identical apart from the line naming which generator wrote it, describe mode
identical, and thirteen malformed sources each refused by both with *the same
message*. The comparison test said in its own header that it couldn't outlive
what it compared against, and went with it — the Lua script stayed one commit
under a deprecated name so it appears in the record once as deliberately put
down.


**Two behaviours changed, both improvements:** failure now reports *which kind*
of failure using the project's own exit codes rather than a bare 1, and an
unterminated block comment finally names its file instead of reporting
`(input):0`.


**The issue's prediction held** — parsing was the small half. The floor
underneath it is the bulk, and it got its own test because a crack there
surfaces as garbled C hundreds of lines away.


One piece worth knowing: a function's name is found by scanning **backwards**
from the last `)` to its match, because a name can sit directly against a star
with nothing between — `char *sneaky` — and no single split expresses that.
The Lua version had already given up and done it by hand for the same reason.


**LuaJIT is off the build path.** Building this engine now needs a C compiler
and nothing else. Regenerating the HTML documentation still needs LuaJIT, which
is deliberately out of scope — project housekeeping, not the road a consumer
walks — and that distinction is now stated in the Makefile header, the
packaging note, and the generator's info file.


## One thing I found along the way


Six generated pages had outlived their sources — two from long-retired files,
four from this session's renames — reachable at their old addresses with stale
text and stale links. The site generator only ever writes and has never removed
anything. I swept them by regenerating the whole set from empty, and the
underlying gap is recorded in the HTML documentation issue.


Clean build from scratch: 17 test binaries plus the CLI conduct suite, all
passing.


--------------------------------------------------------------------------------

### User Request 36

Okay. Next issue?


--------------------------------------------------------------------------------

### Assistant Response 36

Per the roadmap, two candidates. Let me size them.


**309 it is** — small, well-specified, and a hard prerequisite for
runtime-compiled boxes. Let me find every place a wire is checked.


Two call sites. Let me see how the loader reaches the destination slot.


Both sites have what they need. Making the change.


Working as intended — the mapfile test expects the old message. Updating it.


All pass. Now the tests for the capability this adds — I need two same-shape,
different-name structs.


Now the test that proves the capability.


I guessed the API. Let me read the actual interface.


Splice cut a function short. Let me look.


All passing. Now step 5 — recording the interchangeability as a stated
non-guarantee.


Let me fix that block in one go.


Clean. Now the issue paperwork.


**309 is done** — types compared by width, not by name.


## What changed


A wire is legal when both ends count the same number of bytes. That's one
integer against another, at both places a wire gets checked: `042-loader.c` when
a map file draws one, and `052-rewire.c` when a running program does.


**What it buys** — two structs with identical layouts and different names now
connect. Before, a box producing three floats called `triple` couldn't feed a
box taking three floats called `vec3`, even though the bytes are
indistinguishable. The author's options were to rename one, or write a box that
took one and returned the other and did nothing.


**The message carries names and widths both:**


```

box returns int (4 bytes), slot takes double (8 bytes)

```


*"box returns vec4, slot takes stats"* doesn't say why those disagree. Four
against eight does.


## Tests


`tests/072-test-width-wiring.c`, three parts:


- The two structs are byte-identical by the compiler's own account — same
  size, same offsets, a memcpy across them preserves every field. If that ever
  stops being true, the wiring below stops being safe and this is where it
  shows.

- A station producing a `triple` wired into a station taking a `vec3`, run, with
  both stations' completion counts checked — the wire really carried a value.

- Different widths refused through the runtime rewiring surface, with the
  message captured and checked for both names and both widths.


The two structs live in the box source rather than the test file, deliberately:
the thing being proven is that the *engine* stopped caring about the name, so
the shapes have to be real types the generator saw.


## Recorded as a non-guarantee


Same-width, different-layout types now wire without complaint — a struct of
four integers connects to a struct of two integers and a double. Written into
the guarantees page in its own words, including the part that bites hardest: a
wrong wire between two data types gives a wrong number, which is local and
noticeable, while a wrong wire into a port expecting a handle gives a call
through whatever those bytes were — and every pointer is the same width as
every other pointer, and as a `double`, and as a file offset.


Shape comparison would close it honestly, is designed in full in the issue, and
the field tables it needs have been emitted since phase 3 and consulted by
nothing. It lost on costing a walk where width costs a comparison.


## Roadmap position


Front B's next step is **310 — boxes compiled at runtime**, which needed 309
as a hard prerequisite and which builds the quiescence sweep that 216 also
needs. Front A's next is **210b**, now fully unblocked.


--------------------------------------------------------------------------------

### User Request 37

Okay. Can we add 310 and 210b to the todo list and do them next?


--------------------------------------------------------------------------------

### Assistant Response 37

310's last step needs the retire-sweep mechanism from 214, which isn't built.
Let me check what's actually left in 210b first — it may be the quicker one.


**210b is nearly done** — only the two map-file forms remain: the dash for an
unconfigured port and `x64` for a starting depth. Let me do that first, then
310. Reading the `in` line parser.


The `= value` form runs to end of line, so a depth can't follow it. Let me check
the slot kinds before deciding the spelling.


`x64` can't follow `= value`, since that value runs to end of line. Putting the
depth **before** the source gives one rule that works for all three forms.
Implementing that, and I'll flag the change.


Now the loader, which has to apply both.


All tests pass. Now the dump, so the round trip closes.


Round trip still closes. Now the test for the new forms.


The seed sweep refuses a program that can't start — which is 212's to change,
not 210b's. Let me make the test program partly runnable, which is the realistic
case anyway.


All passing. Now the format document and the issue paperwork.


210b is complete — every step built, both questions answered. Filing it.


**210b done.** Now 310 — boxes compiled at runtime. Let me read the parts I
haven't.


Now let me see how lookup works today, since the table has to grow.


Compiles. Now lookup has to walk both.


Now the tests — including the one that pins the accepted cost.


Needs the libs include path too — the registry header pulls in the pool.


`/dev/shm` is mounted noexec — which is exactly why this project has **two**
RAM tiers. Executable code belongs in the `/tmp` tier; the source text belongs
in shared memory. I had both in the wrong place.


A box written at runtime now runs. Step 7 needs the source findable **by box
name**, so a fresh process can reload a dump. Implementing that.


Now the fresh-process test — a dump reloaded by a program that was never told
about the box.


The dump needs a loaded map, not a hand-built one. Let me load from text first.


The loader checks the name itself, before placement. Let me find that check.


All seven checks pass. Steps 1–7 done; step 8 (unloading) is blocked on 214.
Writing the paperwork.


Both done. **210b is complete and filed; 310 is built and in progress**, blocked
on one thing.


## 210b — the port record


Only two things were left, both in the map file, and both landed:


- **`in 2 -`** — a port with no source yet. The dump used to write a comment
  admitting the format had no word for one, which was honest and lost the round
  trip: a half-built program was the one thing a dump couldn't promise to
  reload. It can now.

- **`in 0 x64 $0`** — a starting depth, written only when it isn't the
  default, since the format writes exceptions and `x10` on every line is noise.


**One correction from implementation.** You chose `x64` *after* the source; it
had to move *before* it. A value written on the line runs to end-of-line —
struct values have spaces in them — so nothing can follow one, and a depth
written after a value would silently become part of the value. Before the source
is one rule for all three forms; the alternative was a rule whose exception was
the form people write most. Docs and issue both say the corrected thing.


The round-trip test builds a program that's *partly* wired, because a program
where nothing can start is still turned away by the seed sweep — which is
212's to change, not this issue's. Partly-wired is what a program mid-assembly
actually looks like anyway.


## 310 — boxes compiled at runtime


Built: source in, generator, compiler, `dlopen`, rows added. `src/073-latebox.h`
and `074-latebox.c`, plus the three build facts baked in by the Makefile —
which compiler, where the generator is, where the headers are.


Seven things proven in `tests/075-test-latebox.c`, including **a dump of a grown
program reloading in a fresh process** — the source is filed under each box's
own name when the box is created, and a process that was never told the box
exists compiles it back and says out loud that it did.


**One test asserts a wrong answer on purpose:** a late box whose struct
disagrees in layout at the same width is accepted, and the fields arrive
transposed. That's 309's stated cost, pinned so whoever finds it later finds a
decision rather than an oversight. If shape comparison is ever built, that test
is what breaks.


**Two things worth flagging:**


- **Your two-tier RAM rule earned itself today.** I put both the source text and
  the compiled `.so` in `/dev/shm` and got *"failed to map segment from shared
  object"* — because that tier is mounted noexec. The rule was already written
  down; what it was *for* had to be rediscovered by breaking it. Text goes to
  the read tier, executables to the exec tier.

- **310 is not complete.** Unloading is blocked on 214's retire-sweep-free
  mechanism, so a late box's library is never closed and each compile leaks one.
  That mechanism is now needed by **three** things — 214 itself, 216's station
  removal, and this — and should be built once. That makes 214 look like the
  highest-leverage next issue, ahead of the roadmap's suggestion.


--------------------------------------------------------------------------------

### User Request 38

Great let's do 214 nexty


--------------------------------------------------------------------------------

### Assistant Response 38

Six users to update. Starting with the representation.


Now the helpers, and the six users.


Steps 1–3 done. Now the sweep — the part 310 and 216 both need. Let me see
what the pool exposes.


The pool already has worker indices, and the task-run block is exactly the
window the issue describes. Adding the counters there.


Memory corruption. Let me check where the epoch struct landed and how the worker
index is set.


Found it — I sized the epoch array from the raw parameter, but the pool
decides its own worker count when passed zero. Workers past the first were
writing off the end.


Fixed. Now the sweep itself, which is what makes the scrapyard reclaim rather
than leak.


Now the tests the issue asks for — the sweep observed emptying, and teardown
racing a sweep.


400 rewires, deepest 117 filed, zero left. Let me have a leak checker judge it,
since that's the real test.


Zero leaks, zero errors, 1,761 allocs and 1,761 frees. Let me check the mid-run
rewire test too.


Clean. Now the paperwork — guarantees, info files, and the issue.


**214 done.** The last lock is off the delivery path.


## What changed


A port's destinations were a linked list a rewire could free under a walker's
feet — so every delivery took the station's mutex, counted, copied every
`{station, slot}` pair onto its own stack, and released before visiting any of
them. A lock and a copy proportional to fan-out, **on every value the engine
moved**.


Now they're **one immutable array behind an atomic pointer**. Rewiring builds a
whole new one and swaps the pointer in a single write. A walker reads it once
and walks something nobody will ever modify — no lock, no copy, no half-edited
set.


## The scrapyard


Each worker keeps an epoch on its own cache line, bumped at the start and end of
a **whole task**, so it's odd while inside one and even while not. A replaced
set is filed with a snapshot of every epoch; a later retire sweeps first,
freeing anything where every worker is either even or has moved on.


Nothing waits and nothing spins — an idle worker is asleep and therefore even,
so it passes without moving, which is what would otherwise deadlock a sweep
against a quiet pool.


**One counter for the whole task, not one per window** — exactly as the issue
specified, and this is the part that matters beyond 214: a worker is inside a
*box* earlier in a task than it's inside a *delivery walk*, so the same counter
answers both questions. **310's box unloading and 216's station removal now have
their mechanism.**


## Proven


```

== 076-test-destinations

  a wire came out and the old set was left intact for walkers

  400 rewires under load: deepest 117 filed, 0 left after

  a teardown emptied a scrapyard that still had sets in it

  a rebuild kept wiring order, so the dump still round-trips

```


Under valgrind: **1,761 allocations, 1,761 frees, no leaks, no errors** —
which is what actually judges a scrapyard. The mid-run rewiring test comes out
the same.


## One mistake worth flagging


I sized the per-worker epoch array from the worker count the *caller* asked for.
A pool asked for zero decides its own count — so the array was one slot while
several workers wrote into it. It surfaced as a segfault inside `pthread_join`,
freeing a thread control block, hundreds of lines from anything I'd touched.
Size it by what was settled on, never by what was requested. That's recorded in
the issue.


## What this unblocks


**310's step 8** — unloading a late box's shared object — now has its
mechanism and is no longer blocked. **216's station removal** likewise. Both
were waiting on exactly this, which is why it was worth doing ahead of the
roadmap's order.


--------------------------------------------------------------------------------

### User Request 39

okay let's do 310 step 8 and 216's station removal


--------------------------------------------------------------------------------

### Assistant Response 39

Both need the same quiescence machinery, so let me generalize the scrapyard
first — it should hold anything, not just destination sets.


Generalized and still green. Now station removal.


Let me clean up a drafting artifact in there.


Now `registry_unload_box`, which the test calls for.


An in-flight delivery lands on the removed station. Let me see how delivery
handles a target with nowhere to go.


`t->call` was null — a task built from a station that was removed between the
readiness check and the build. Let me look at that path.


The whole build races removal, not just the call pointer. The station's fields
must stay readable until the scrapyard says nobody is looking.


Correct behaviour — the place isn't free until the sweep runs. Placement must
refuse it until then, and the test must sweep.


The place isn't reclaimed while the pool is busy, so the second removal
correctly refuses. Let me fix the test's loop.


One real bug: items retired **before** the pool existed get a null snapshot and
are never freed. But nobody can hold those — there were no workers.


All green, and clean under valgrind including the `dlopen`/`dlclose`. Now the
paperwork.


Both done and filed. `2b0a32d`.


## 216 — removing a station


A station comes out of a running program and its place is reused, with **no
version tag on any wire** and no cost anywhere on the delivery path. Your
reasoning held exactly: cutting the wires that name it *first* is what makes a
generation counter unnecessary — a wire lives only as a destination record on
some station's output port, so one walk finds all of them, and with none left
there's nothing stale to catch.


**Two things came out of building it that the blueprint hadn't seen.**


**A removed place is not immediately a free place.** A task is built from a
station's slot count, return size, and shim *after* the readiness check released
the mutex — so clearing those at the moment of removal leaves a worker
assembling a task out of a station emptied underneath it. It surfaced as a jump
to address zero: a null shim, called. So the record stays whole and a flag says
"start nothing new here"; the fields go when the sweep says nobody can need
them, and clearing the shim is what finally frees the place. Placing into an
unreclaimed place is refused loudly. That's correct rather than awkward — you
can't reuse something while somebody might still be using it.


**A value already in flight toward a removed station is discarded** — the same
thing this engine already does with a value that has nowhere to go. The
alternative would make every removal a race against whatever was already moving.


## 310 step 8 — unloading


A late box can be unloaded, closing its library. Refused while any station in
the given map places it; the harder half — a worker may be *inside* that code
right now — goes to the scrapyard on the same counter. **It checks the map you
hand it**: a process running several maps could have another placing that box,
and nothing can see that because nothing here is process-wide. Stated rather
than defended.


That capability looked marginal when it was written down — it could only serve
a box compiled and never placed. Stations being removable is what makes it real.


## Verification


```

== 077-test-removal

  three wires named a station; removing it cut all three

  a freed place was reused and received only its own wires

  87 removals and 84 placements under load, scrapyard drained

  an unplaced box unloaded while the pool was busy

```


Valgrind: **2,736 allocations, 2,736 frees, no leaks, no errors** — including
a shared library opened and closed.


## One correctness fix along the way


Items retired *before* the pool existed got a null epoch snapshot and were never
freeable except at teardown. But nobody can hold those — there were no
workers, and the item was already replaced before anyone could reach it. They're
now freed on the first sweep, which also made construction-time wiring stop
accumulating.


## Roadmap


Front A now needs **211** (growing the station table) then **212** (the
construction capstone). Front B's registry family — **311a** onward — is
clear, since 308, 309 and 310 are all done. Six issues closed this session, plus
401 and 210b filed.


--------------------------------------------------------------------------------
