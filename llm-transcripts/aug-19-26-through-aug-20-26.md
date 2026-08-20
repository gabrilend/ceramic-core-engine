# Conversation Summary: 3ccc0eb0-a576-4361-85ec-79ae4bb357a4

Generated on: 2026-08-19 19:11:51

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
