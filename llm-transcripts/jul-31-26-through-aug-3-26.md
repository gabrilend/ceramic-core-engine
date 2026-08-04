# Conversation Summary: 483ee42e-cbd9-482c-a0ef-89252740e511

Generated on: 2026-08-03 14:19:27

--------------------------------------------------------------------------------

### User Request 1

hi can you add issue files to enable compilation to verilog and other related
HDL languages? The idea being, the user writes in C, and they do so in a way
that is (optionally) compilable to an HDL. If the user does anything that isn't
possible or semantically doesn't make sense, it gives a compiler error. The
result should be both compliant C and, when translated, fully compliant HDL as
well. The idea is, if we can make each box into a cluster of FPGAs (as few as
possible per unit), then we can orchestrate structures of computation,
simulation, or design.


--------------------------------------------------------------------------------

### Assistant Response 1

I'll start by learning the terrain — the issue conventions, the phase
structure, and how the C side of the runtime is currently shaped. Let me look
before writing.


Good — I have the shape of the project now. Before writing, here's what I
found that decides the design:


`★ Insight
─────────────────────────────────────`

- The project already has the exact architectural precedent this needs: `langs/`
  holds per-language plugins implementing one `lang_spec_t` interface, dlopened
  by a registry. An HDL backend family (Verilog / VHDL / SystemVerilog) is the
  same shape rotated — one interface, several emitters — so the phase can
  borrow a proven boundary instead of inventing one.

- The runtime's firing rule ("a box fires when all required input slots hold
  values, and its output pushes into downstream slots") is already a valid/ready
  handshake with FIFOs. The software runtime is, structurally, a simulator of
  the hardware you'd build. That is why this idea is tractable at all.

- The hard part is not translation, it's *agreement*. Issue 324's ruling —
  literal ports are re-read every fire, wired ports are consumed — and the
  nonlinearity routing kind's `double` math are places where C and silicon will
  silently disagree unless one side moves.

`─────────────────────────────────────────────────`


Let me write the parent issue first.


Now the type system — this is the crux of "valid C *and* valid HDL", so I'll
set up the trade-offs and leave the decision itself for you.


Thirteen blueprints written, plus a phase-5 progress page and a line in the
table of contents. Nothing committed — the working tree already has a pile of
other people's modified files staged and unstaged, so a commit here would sweep
them in.


**What the phase says, in one paragraph each:**


- **501, the parent** — the dual-compliance rule and seven ground rules. The
  one that matters most: *when the two worlds cannot agree, the software moves.*

- **502, the hardware dialect** — a table of C constructs with a verdict and a
  hardware reason. Every refusal carries four parts: what, why in hardware
  terms, what to write instead, and the escape hatch.

- **503, fixed-width types** — three candidate schemes; the choice is left to
  you (below).

- **504, the front end** — one hand-written parser whose *coverage is the
  dialect*, so a refusal can never be a parser bug in disguise. `cc -E` does the
  preprocessing.

- **505, the module contract** — valid/ready handshakes and FIFOs, which is
  what the slot store has been simulating since phase 3 without anyone calling
  it that.

- **506, datapath and FSM** — expressions are shapes with no time in them;
  statements have time, so scheduling is where the hard questions live.

- **507, routing cells** — seven cells. Two of them (`weighted`,
  `nonlinearity`) force the pool runner to give up `double` and adopt fixed
  point.

- **508, the host bridge** — a wire crossing the cut becomes a *pair of
  boxes*, so the runtime learns one box kind instead of a concept called
  "remote."

- **509, packing** — your "as few FPGAs per unit" phrasing as a lexicographic
  objective, with a cost model that's calibrated against real synthesis reports
  rather than guessed.

- **510, the intermediate and backends** — an `hdl/` plugin directory shaped
  exactly like `langs/`. VHDL exists mainly to prove the intermediate was
  neutral.

- **511, equivalence testing** — every software run becomes a test suite.
  Declared widths make *exhaustive* testing ordinary.

- **512, the editor lens** — a pure viewer over three files.

- **513, the capstone** — one image kernel, three executions, byte-difference
  count of zero.


`★ Insight
─────────────────────────────────────`

- The strongest argument that this project can host a hardware target is
  structural: `langs/` is already a plugin family behind one interface,
  `slot_alloc` already logs a `cell_capacity` that *is* a FIFO depth, and issue
  324's ruling (wired ports are consumed, literal ports are re-read) already
  says pop-versus-read. The runtime was independently arriving at hardware.

- The most dangerous corner isn't translation — it's that C sizes an
  expression from its *operands' promotions* while Verilog sizes it from the
  *operands*, so `uint16_t c = a + b` on two 8-bit values gives 300 in software
  and 44 in silicon with no warning anywhere. That single sentence is why 503
  blocks the whole phase.

- I made `nonlinearity` push its fixed-point curve back *into* the pool runner
  rather than giving the equivalence test a tolerance. That's the phase's most
  invasive consequence: turning on the hardware target changes what a
  software-only run computes. Flagging it as the thing most worth arguing with.

`─────────────────────────────────────────────────`


There are roughly sixty open questions across the set, recorded in each file.
Five gate everything else, listed at the bottom of the progress page — but one
of them blocks the other four.


● **Learn by Doing**


**Context:** Issue 503 is the floor the whole phase stands on. Every other issue
reads bit widths through whatever it settles. I've written up the problem —
C's integer promotion versus Verilog's operand-width arithmetic — and laid out
three candidate schemes with their real costs: **A** plain typedefs plus
hand-written masks (author error-prone, checker does all the work), **B**
unsigned struct bitfields where C's own modulo-2^N store rule closes the gap for
free at the cost of `.v` on every use, and **C** C23 `_BitInt(N)` which is
semantically perfect and demands gcc 14 / clang 16.


**Your Task:** In `issues/503-fixed-width-and-fixed-point-types.md`, fill in the
section titled "The declaration scheme" — it currently holds only TODO(human).
Write which scheme the project takes and why, in whatever detail you want. A few
lines is enough; the reasoning matters more than the length.


**Guidance:** The trade-off I'd weigh hardest is between *structural* agreement
and *policed* agreement. B and C both make the C compiler enforce the width, so
the checker never has to prove anything about it — a whole class of bug stops
being possible. A leaves the checker responsible, which means a gap in the
checker becomes wrong silicon rather than an error. Against that: C needs a
compiler newer than "whatever `cc` is," which is a real break from how issue 307
deliberately made the C spec depend on nothing. And B's `.v` suffix on every use
is friction that a box author feels on every line they write — worth asking
whether that friction is a cost or an honest signal that this code is going
somewhere unusual.


--------------------------------------------------------------------------------

### User Request 2

can we add issue files to add a "counter" that can be used to count on a fixed
timestep? Like a clock signal, but with configurable duration. it should be able
to return values such as time since and set up little alarm timers for various
expectations. they could be scheduled and cached and defined by a math function
(see number waves, ask open question) which shows certain formula for certain
permutations, which could be useful for measuring recurring schedules
essentially as many as we can.


Okay. Next, I want you to go through the issue files, completed issues, and
currently built source-code and build me a list of contradictions. Then, I want
to go through them together and resolve them one-by-one. As we do, we should
prune the old explanations so they don't confuse anybody moving forward.


--------------------------------------------------------------------------------

### Assistant Response 2

Issue 251 already owns the tick emitter, so the counter belongs under it as
sub-issues rather than as a fresh parent. Let me ground the design in what the
runtime actually has for state and box kinds.


Issue 251 is the tick source; the counter is a different animal, so it goes
underneath as sub-issues. Here's the design decision that shapes all three:


`★ Insight
─────────────────────────────────────`

- A counter doesn't need to *run*. If it stores an origin and a step, then
  `count = floor((now − origin) / step)` is a closed form — so it can be a
  **pull-on-demand source** like a `read` box: never queued, never a task, never
  perturbing the schedule. Issue 244 already established pull-on-demand for data
  boxes, so the precedent exists.

- That also dodges the single-output-per-box rule. A box that wanted to emit
  count *and* elapsed *and* phase would need three outputs, which the schema
  forbids; a puller emits nothing and answers whatever the reader asks for.

- The dispatch already has `mono_us()` and `now_secs()` as file-static helpers,
  duplicated in the pool runner — so a shared monotonic time source is the
  actual first step, and its absence is one of the contradictions I found in
  part two.

`─────────────────────────────────────────────────`


Now part two. Let me hunt contradictions systematically rather than by
impression.


Found them. Evidence-backed, ranked by how much confusion each causes someone
rebuilding from the blueprints.


`★ Insight
─────────────────────────────────────`

- The pattern behind almost every contradiction here is the same one phase-1's
  progress page already diagnosed for itself: **whatever `make test` sweeps
  stayed current; whatever it doesn't touch drifted.** Docs, the `maps/`
  directory, the standalone validator, and `drivers/` all rotted; `tests/maps/`
  did not.

- The second pattern is *aspirational present tense*. Several documents describe
  a decided-but-unbuilt design in the same voice they describe shipped
  behaviour, then add a footnote admitting the gap. A reader who skims the body
  and misses the footnote gets a wrong model of the runtime.

`─────────────────────────────────────────────────`


**Counter work:** three sub-issues under 251 — 251a (the counter as a
pull-on-demand closed form), 251b (alarms, and expectations with `met`/`missed`
branches), 251c (schedules as functions, plus the number-waves question). 251
itself now carries a sub-issue index and a line distinguishing the four roles:
the timer *fires*, the counter *answers*, an expectation *waits*, a schedule
*decides when*.


---


## Contradictions


**1. The C box rule contradicts the C box product.**

`issues/completed/307-c-language-spec.md:172` states a rule marked
non-negotiable — *"No header to include, no macro, no `int
classify(SM_BOX_ARGS)`... SoraMech bends to the language. The language never
bends to SoraMech."* Its own implementation log then records that what shipped
is a fixed SoraMech-shaped signature, and `docs/005-writing-boxes.md:101`
teaches exactly that signature to authors: `int square(const void **inputs,
const int *sizes, int n_inputs, ...)`. The blueprint's most emphatic rule is
violated by the thing the blueprint built. **This one also reaches into new
work** — I cited that rule as ground rule 1 of the HDL parent issue.


**2. Three completed issues ship a file that does not exist.**

`soramech-c.h` appears in 307 (implementation step 5), in 309's directory tree,
and in 223. `langs/c/` contains `Makefile`, `lexer.js`, `spec.c`, `spec.js`,
`spec.info.md`, `spec.so` — no header. Re-completing the completed issues
would not reproduce the tree, which is the one thing they promise.


**3. The runtime doc states the spawn ruling as fact, then footnotes that it
isn't.**

`docs/004-runtime.md` says plainly *"there is no single-spawn invariant and no
multi-spawn marker"*, then blockquotes that the runner still carries both.
Source agrees with the footnote: `propagate_multi_spawn()` at
`src/010-graph-loader.c:2108`, the CAS guard at `src/012-dispatch.c:342`, and
`multi_spawn` still branching slot mode and ring size at
`010-graph-loader.c:2175`.


**4. Two entry-box concepts coexist.**

The loader exposes both `graph_entry_box_id()` (the declared `entry` field) and
`graph_n_entry_boxes()` / `graph_entry_box()` (computed detection). Issue 206
says the field went vestigial as predicted but was never retired — still
mandatory in the schema, still in the runner banner, still commonly pointed at a
read box that cannot be an entry.


**5. Phase 1 is complete and also not.**

`docs/000-table-of-contents.md` says *phase 1 (foundation): complete*.
`issues/phase-1-progress.md` lists 103 reopened and 111 open, under a
"Post-phase maintenance" heading.


**6. The no-path-links rule is violated by most of the corpus.**

`CLAUDE.md` forbids path links between issue files. Issue 419, all four
phase-progress pages, and `docs/004-runtime.md` all use them. Several already
point at paths that moved — the exact failure the rule was written to prevent.
The phase-5 files I added follow the rule, so the corpus is now inconsistent two
ways.


**7. Two cited docs have never existed.**

`docs/004-ipc-and-threading.md` and `docs/005-language-specs.md` are cited as
real by `docs/006-test-coverage-map.md`'s Related Documents, by 307, and by
425/428. That same section correctly annotates the `001-architecture.md` rename
— so it was half-repaired and the other two were missed.


**8. A retired subsystem is still in the tree.**

`drivers/` holds `README`, `bash.sh`, `c.sh`, `lua.sh`. 307 lists the C driver
as retired by the language spec. Nothing in the Makefile, `docs/`, or
`src/*.lua` references the directory.


**9. Build output location contradicts the RAM-tier rule.**

`CLAUDE.md` says compiled builds belong in `/dev/shm/soramech` via
`tmp/shared-memory/`. `build/` sits in the repo root with `libs/`, `src/`,
`tests/` inside it, gitignored under a comment crediting issue 309.


**10. The "data box" is a kind that no longer exists.**

Issues 229 and 244 are written around a `data` box kind. The schema's valid
kinds are `call`, `read`, `write`, `map`; `docs/002-map-model.md` describes the
pull-on-demand behaviour under `read`. Two completed blueprints name a thing you
cannot create.


**11. The clock is implemented twice.**

`now_secs()` and `mono_us()` are file-static in both `src/012-dispatch.c` and
`src/008-pool-runner.c`. Not a textual contradiction — a
single-source-of-truth break that the new counter box would make worse if left
alone.


**12. Two unrelated C-parsing efforts.**

`docs/006-test-coverage-map.md` plans `langs/c/parser.js` for the editor; my new
issue 504 proposes a C front end in C for the HDL path. Neither mentions the
other, and they'd parse the same language for different reasons.


---


Ready to work these one at a time — say a number and I'll pull up the full
evidence and propose the prune. My suggested order is 1 → 3 → 4, since those
three are the ones actively teaching people a wrong model of the runtime.


**And the open question you asked me to raise:** what is a *number wave*? I
wrote four readings into 251c — a parameterised closed form; a
Euclidean-rhythm-style algorithm where permuting (rotating) a pulse distribution
gives a family of related rhythms; a sampled continuous waveform; or an
interference pattern where the wave is what appears when several periodic
sequences align. Reading two fits your phrase *"certain formula for certain
permutations"* almost exactly, and reading four fits *"measuring recurring
schedules"* — and they might be the same idea from opposite ends, one
generating pulses and one detecting where pulses coincide. Is there a specific
source behind the term I should read before that issue gets specified?


--------------------------------------------------------------------------------

### User Request 3

can you write a report on these contradictions for me and either shape it like
an issue file or reference it in an issue file?


--------------------------------------------------------------------------------

### Assistant Response 3

Before writing it — one check I should have run earlier, because it's
mechanical:


No existing audit issue, so this is a new blueprint rather than a reopen.
Writing it as issue 112 in phase 1 — that's where the drift diagnosis already
lives, and the next free number, because:


`★ Insight
─────────────────────────────────────`

- Running `ls | grep -oE '^[0-9]+[a-z]?' | sort | uniq -d` found a
  **fourteenth** contradiction in one line: issue 111 is used twice —
  `111-bash-output-format-and-driver-test-entry` (completed) and
  `111-restore-phase-1-demo-maps-to-current-schema` (open). Every citation of
  "issue 111" is ambiguous.

- That's the argument for shaping this as a blueprint rather than a report:
  about half these findings are detectable by a script, and your standing rule
  is to build the tool rather than the artifact. A report goes stale the day
  it's written; a checker in `make test` cannot.

`─────────────────────────────────────────────────`


Written as `issues/112-record-consistency-sweep-and-checker.md`, and referenced
from the post-phase-maintenance section of `issues/phase-1-progress.md` — that
section already diagnoses this exact drift for phase 1, so the general case
belongs directly under it. No commit; the tree still has other people's unstaged
work in it.


**What shape it took.** An issue file rather than a report, because roughly half
the findings are detectable by a script and your standing rule is to build the
tool instead of the artifact. So it's a blueprint for `scripts/check-record.sh`
wired into `make test`, with the fourteen findings as both the work list and the
checker's first failing run.


**The organizing principle it commits to:** each finding is resolved by deleting
the wrong side, not by annotating the disagreement.
`docs/006-test-coverage-map.md` is the argument for that rule — it correctly
annotates one renamed doc path and then cites two documents that have never
existed, three lines apart in the same list. A note explaining a contradiction
preserves both the error and the correction, and doubles what a reader carries.


**The split it draws:**


- **Mechanical (6)** — duplicate issue numbers, path links between issue
  files, dangling document citations, dangling file citations, orphan
  directories, and progress-status disagreement. These become the checker.

- **Needs a human ruling (8)** — every case where prose and code disagree
  about *meaning* rather than about a path: the C box contract, the spawn rule's
  tense, the two entry-box concepts, the vanished `data` kind, the duplicated
  clock, the two C parsers, and the vision document's exemption.


**One finding grew teeth while writing it.** Finding 1 — the C box rule versus
the C box product — turns out to reach forward into the hardware phase,
because I cited 307's "SoraMech bends to the language" as the precedent for
ground rule 1 of the HDL parent. If that rule is aspirational rather than
binding, the HDL ground rule needs a different justification. I've written that
dependency into both the finding and its ruling.


**Two questions are blocking, and both are yours to answer:**


1. **Is renaming an issue file permitted?** Finding 4 needs it — issue 111 is
   two different issues. The house rule says issue files are immutable and may
   be added to but never deleted; a rename sits between those. I'd move the open
   one rather than the completed one, since the completed one has more inbound
   citations, but that's a house-rule exception I shouldn't grant myself.


2. Still outstanding from last turn: **what is a number wave** — the
   parameterised closed form, the Euclidean-rotation family, the sampled
   waveform, or the interference pattern. Issue 251c can't be specified past its
   scaffolding until that lands.


--------------------------------------------------------------------------------

### User Request 4

cd ..


--------------------------------------------------------------------------------

### User Request 5

<bash-input>cd ..</bash-input>


--------------------------------------------------------------------------------

### User Request 6

<bash-stdout>(Bash completed with no
output)</bash-stdout><bash-stderr></bash-stderr>


--------------------------------------------------------------------------------

### Assistant Response 6

Noted — you're up a level now, at `/mnt/mtwo/programs/sora/`. My tool calls
all use absolute paths into `soramech/`, so nothing on my end shifts.


Still waiting on you for the two blocking answers (issue-file renaming, and what
a number wave is), or say a finding number from 112 and I'll pull up its
evidence and propose the prune.


--------------------------------------------------------------------------------

### User Request 7

number waves are periodic functions that pass through prime numbers. So there's
one for 2 which crosses the Y axis at 2, 4, 6, 8 etc, and one for 3 which
crosses at 3, 6, 9, 12, etc. The idea is to use them to find vacant spots, and
to use those slots like bitflags or something idk I'm not very sure how that
connects. Sure you can rename issue files. Try and move the newest, and always
ALWAYS update the citations. No need for tombstones if there's no references to
the other location.


--------------------------------------------------------------------------------

### Assistant Response 7

Both answered. The number-wave definition is the sieve of Eratosthenes read as
interference, and it connects to scheduling much more tightly than you might
think — let me show you where, then act on the rename ruling.


Now the four citations, and the file's own title:


Now finding 4 in issue 112 becomes a resolved finding with the ruling recorded:


Rename done and all four citations moved. Now the substance — your definition
changes 251c from scaffolding into something specifiable:


`★ Insight
─────────────────────────────────────`

- Your wave family **is** the sieve of Eratosthenes read as interference: the
  vacant spots — integers no earlier wave crosses — are exactly the primes,
  and each vacancy becomes the next wave. The structure generates itself.

- The bitflag connection you weren't sure about is real and it's the Chinese
  Remainder Theorem. Because distinct primes are pairwise coprime, the tuple `(n
  mod p₁, …, n mod p_k)` uniquely identifies `n` within one cycle of length
  `∏pᵢ` — so **k prime waves turn time into a k-dimensional coordinate
  space**, and the bitflag word at tick n is just "which coordinates are zero."

- That's also why *primes* rather than any periods: a wave of period 6 crosses
  exactly where W₂ and W₃ coincide, so it carries no new information. The
  prime waves are a **basis**. Every periodic schedule is a region in the space
  they span.

`─────────────────────────────────────────────────`


Both rulings applied. Here's what moved.


**The rename.** `git mv` from
`111-restore-phase-1-demo-maps-to-current-schema.md` to `113-...`, so both
versions are tracked — the rename is **staged in the index** right now, which
you'll want to know before your next commit. The newer of the pair moved, as you
said. Its title line and status now record the renumbering and why.


**Four citations updated:** issue 103's implementation step 7 and its
related-documents list, issue 233's note about maps sitting outside the test
sweep, and the phase-1 progress table (which also lost its path link, since
that's finding 7 of the same sweep).


**Two apparent citations correctly left alone**, and the second one is a warning
worth keeping:


- Issue 110's *"Issue 111 created for bash output format confusion in
  driver-test map"* points at the original 111, which didn't move.

- Issue 316's *"111 / 113 tests passed (14 binaries…)"* is a test count. A
  naive renumbering tool would have rewritten it — and note that string
  happens to contain both the old and the new number, so it would have been
  mangled twice. I wrote that into finding 4 as a constraint on the checker: **a
  bare number in prose is not a citation.**


The `uniq -d` check now returns nothing. No tombstone at 111; the only surviving
mention of the collision is the past-tense account in issue 112, which is the
ruling's record rather than a marker at the old address.


**Issue 112** now shows finding 4 struck through in the table, the ruling
written up as a general rule for future collisions, status updated to "one
resolved," and the renaming open question removed with the remaining two
renumbered.


---


**Number waves, in 251c.** Your definition replaced the four-guess section
entirely. What I added beyond restating it:


- **The vacancies are the primes**, and each vacancy becomes the next wave —
  so the family generates its own indices. A library seeded with nothing but `2`
  extends itself.

- **Why primes and not arbitrary periods**, which is the part you said you
  weren't sure connected. Three consequences of pairwise coprimality: the
  combined cycle is the *product* (maximal); by CRT every residue combination
  occurs exactly once per cycle, so the schedule space is complete with nothing
  unreachable; and composite periods are redundant — period 6 is just where
  W₂ and W₃ coincide. The prime waves are a basis, and a schedule expressed
  as period-plus-phase is a region in the space they span. The bitflag word
  tests membership in a whole family at once, per tick, with one integer.

- **Where it hurts.** `∏pᵢ` for the first six primes is 30030 — a fine
  table. For the first ten it's 6,469,693,230 — not a table. So the cache is a
  *window* and `n mod pᵢ == 0` stays the definition. A design that
  materialised the full cycle would demo beautifully on three waves and die on
  the seventh.

- **An honest limit:** real maps will have whatever periods the user's work has,
  and when those aren't coprime the clean theory degrades to a general covering
  system — lcm instead of product, some combinations never occurring. Both
  should work; only the prime case is elegant, and the code shouldn't quietly
  assume it.


**The question that replaced yours** is the one I'd most like your read on
eventually: how wide is the bitflag word? A 64-bit word holds 64 schedules
comfortably, but sieving for vacancies needs every wave below the square root,
and the 64th prime is only 311. A fixed word and a growable bitset are two
different data structures — which suggests "schedule signature" and "vacancy
hunt" may be two features sharing a name.


---


Nothing committed. Findings 1, 2, and 3 are still the ones teaching a wrong
model of the runtime — say the word and I'll pull up finding 1's full
evidence, since it's the one that also constrains the hardware phase.


--------------------------------------------------------------------------------
