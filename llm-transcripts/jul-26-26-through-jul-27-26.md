# Conversation Summary: 96b748d7-9435-4c17-8ee7-b48e86b2b4b8

Generated on: 2026-07-26 23:23:54

--------------------------------------------------------------------------------

### User Request 1

hi, can you examine this project and ../soramech/ and get a feel for how we're
supposed to be building the system? I think at
/home/ritz/programming/ai-stuff/soren-ds/ we might find a better soramech
implementation than what's in the ../soramech/ directory.


--------------------------------------------------------------------------------

### Assistant Response 1

I'll take a look at all three directories to understand the system.


I've read through all three. Here's the shape of what's there.


## The three artifacts play different roles


**`../soramech/`** — the working implementation, and the only one with running
code. A C thread-pool runner (`soramech-pool`) built from nine C files, plus a
Lua HTTP file-proxy server, a browser canvas editor, a synchronous Lua
interpreter, and three language bridges (Lua/C/Bash). It works. It's also heavy:
the graph loader is ~100 KB of C in a single file, dispatch ~66 KB, the slot
store ~44 KB. A large share of that mass is exactly what your minimal vision
says to cut — dual-ring slots so each cell can hold either native bytes or
JSON, per-cell tags for reorder, per-port translation specs, per-edge language
classification.


**`soren-ds/`** — the handheld OS. Its `docs/012-soramech-runtime.md` and its
phase-2/phase-3 issue blueprints are a **re-derivation** of the same runtime,
done after learning from the first one. Important caveat: **none of it is
built.** Issues 201–211 (threading core) and 301–311 (runtime) are all still
in the open `issues/` directory; `src/` there is bare-metal driver code —
eMMC, USB, display, probe engine — with no task struct, no slot store, no
worker loop. So the "better implementation" you remembered is a better *design*,
not better code.


**`minimal-soramech/`** — one `vision` file at the project root, nothing else
yet.


## What soren-ds actually does better


Four things, and they're the four that matter:


**Multi-spawn is unconditional.** Soramech proper gates same-box re-entry behind
a single-spawn invariant, then carves an exception for iterator-routing boxes
and everything downstream of them. That exception has to be computed at load
time and threaded through dispatch. Soren-ds deletes the gate: any box can be
firing on three workers at once, and thread safety inside a box function is the
box author's problem. The runtime's guarantee shrinks to two sentences — each
fire owns its input values, each fire owns its return slot — and the whole "is
this box multi-spawn?" bookkeeping evaporates.


**The gathering function is stated as one primitive with a fixed ordering.**
Look at inputs, pop one from each if all are full, reserve a return slot,
*release the atomic*, then build and enqueue the task. Decide under the lock,
act outside it. Contention is bounded by how long it takes to scan and pop, not
by how long it takes to build a task struct.


**One atomic per box, not per slot** — and the doc states why, which is the
part worth keeping: two slot-level deciders looking at the same box can both
conclude "not yet" (box never fires) or both conclude "now" (box fires twice). A
box-level atomic forces exactly one thread to witness the moment the last slot
filled.


**One language means one representation in a cell.** No JSON serialization
across a wire, no dual ring, no per-cell tag. This is where most of soramech's C
bulk goes away.


Read `soren-ds/docs/003-threading-model.md` first — it's four pages and it's
the spine of the whole thing.


## Your vision file asks me a question, so: yes, and there are two of them


The paragraph you asked me to answer — no-input boxes, hooks during the input
check. Both projects have a system mapped out, and they are **different systems
giving different answers**:


- **Soramech proper (built, issue 244):** read boxes never enter the task queue
  at all. At graph load, each read box's bytes are cached on its record. Every
  consumer gets a per-input-port list of its read-box predecessors. When a
  consumer's gather finds a slot empty, it copies the next predecessor's cached
  bytes straight into the input buffer. Multiple read predecessors on one port
  rotate via an atomic counter. A port fed only by a read box counts as *ready*,
  so consumers fire immediately at startup. This is precisely the hook you
  describe: it lives inside the gather-input check and pulls from upstream. It
  makes a no-input box an **inexhaustible source** — a default value, re-read
  on every fire.


- **Soren-ds (designed, issue 308):** `map_run()` submits one initial task per
  entry box named in `meta.json`. Push, once, at startup. A no-input box is a
  **one-shot seed**.


Your paragraph contains both. "Boxes that don't have any inputs should run at
least once" is the seed model; "add a hook to the check-all-the-input-values
which points at an upstream box and runs it to gather" is the pull model. Which
one you pick changes what a read box *means*, so it's worth settling before
anything else.


And a third thing neither one does: your vision wants read to **hang until a
value is present**. Both existing designs cache at load and never block.
Blocking reads would be new machinery.


## Questions back at you, since they change the shape of the code


You say comparator and iterator are the only routing kinds you need. Soren-ds
keeps all seven — randomizer, weighted, distributor, nonlinearity too.
Distributor is the interesting one to argue about: it picks the least-busy
downstream branch by sampling slot fill, which is load balancing expressed as
wiring rather than as scheduler policy. In a multi-threaded-by-default engine,
is that a routing kind or is that the thread pool's job?


Second: soren-ds keeps the compile pipeline, reference-counted artifact
generations, hot-swap, and encapsulated sub-maps. Your vision mentions none of
them. Encapsulation especially — soren-ds argues that without it, any map past
about four boxes deep stops being maintainable. Is a minimal engine allowed to
have sub-maps, or is flattening a nested graph already too much?


Third, the one I'd actually want to nail down: with the gate deleted, what stops
a box from firing on stale ordering? Two fires of the same box land in different
return slots, both push downstream, and the consumer's slots now hold values
whose relative order is whatever the scheduler happened to do. Soramech proper
answers that with tagged cells and downstream reorder-by-tag — machinery
you're trying to delete. Does the minimal engine just say "order is not a
property the runtime preserves," or does something cheaper recover it?


Nothing created yet — I only read. Tell me where you want to start.


--------------------------------------------------------------------------------

### User Request 2

  Multi-spawn is unconditional. Soramech proper gates same-box re-entry behind a
  single-spawn invariant, then carves an exception for

  iterator-routing boxes and everything downstream of them. That exception has
  to be computed at load time and threaded through dispatch.

  Soren-ds deletes the gate: any box can be firing on three workers at once, and
  thread safety inside a box function is the box author's

  problem. The runtime's guarantee shrinks to two sentences — each fire owns
  its input values, each fire owns its return slot — and the whole

  "is this box multi-spawn?" bookkeeping evaporates.



yep, no such thing as multispawn, they're all multispawn. Can you update the
docs and issue files? Even the completed ones, re-opening if the source-code
doesn't seem conformant?


--------------------------------------------------------------------------------

### Assistant Response 2

Let me survey every place the single-spawn invariant is written down or
implemented.


Note before I touch anything: the soramech repo has uncommitted work that isn't
mine (issue 103 moved back out of `completed/`, modifications to 111, 233, and
the phase-1 progress page, plus a batch of transcript exports). I'll do the
edits but won't commit — I'll ask you about that tree first.


I have the full picture. The distinction is load-bearing in four places in the
source, so this is a real re-opening. Let me start with the design docs.


Now re-opening the two issues whose described behavior the source still
implements.


My move of 304/305 breaks inbound links. Let me find every reference.


Now the open issue whose design leans on the guard I just removed from the
design.


Done. The ruling is now written down everywhere the design lives, and the two
issues whose shipped code contradicts it are reopened.


## Docs


- **`docs/004-runtime.md`** — the Parallelism section now states the opposite
  of what it said: no invariant, no marker, every box fires on as many workers
  as have work for it. The runtime's guarantee shrinks to the two sentences you
  named. I recorded *why* the gate went, since the exemption machinery is the
  real argument: gating throttles precisely the hot box, and the iterator
  exemption then cost a load-time walk, a second slot shape, and a rule for
  where the two shapes meet.

- **`docs/002-map-model.md`** — the iterator no longer gets described as a
  spawn category; it's just the routing kind whose branch choice advances per
  fire.

- **`docs/005-writing-boxes.md`** — new section, because deleting the gate
  moves a real obligation onto authors. It's per-language, since the three specs
  hold state differently: **C** shares the whole address space (true data races
  on statics), **Bash** shares nothing but the filesystem, and **Lua** is the
  interesting one — the spec keeps a `lua_State` *per worker*, so the failure
  isn't a race, it's a split brain. A module-level counter counts once per
  worker and reads low, non-deterministically. That trap was undocumented.


## Reopened (source is non-conformant)


- **304 — task dispatch layer.** `dispatch_spawn_if_ready` still branches on
  the marker and runs a per-box compare-exchange that caps a box at one fire per
  run. Steps written for stripping the CAS, dropping the now-dead `spawned[]`
  array, making the re-spawn tail unconditional, and the two tests built on the
  old category.

- **305 — C graph loader.** `propagate_multi_spawn` still does a fixed-point
  BFS from every iterator, and three slot decisions branch on the result —
  cell count, peek/pop mode, tagged flag.


The satisfying part: **324's design ruling already did the hard half of this.**
It established that peek-vs-pop is not loop bookkeeping but the two input
methods, consumed or referenced. The loader kept the box marker as the default
and layered 324's port rule on top as an exception. Delete the marker and the
port rule simply *becomes* the whole rule — literal with no wire feeders is
referenced, everything else is consumed. So this isn't an arbitrary deletion; it
finishes a move you already made. 324 stays in `completed/` with a vocabulary
note — its outcome is conformant, only its "multi-fire boxes" category
language is stale.


## One thing you'll want to look at


**Issue 427's safety argument is now void, and it inverted.** Its `connect`
built-in was documented as safe when called from the spec of the box being wired
— "the common case" — explicitly *because* the CAS guard meant no other
worker could be reading that box's connections. Remove the guard and that case
becomes the racy one: the box's own spec can be running on five workers, each
walking the `connections[]` array being mutated. I rewrote it as a blocker on
the slice rather than a caveat, and noted the options (per-box connections lock,
RCU-style array swap, or confine mutation to quiescence).


Also updated: `325` (passing vocabulary), `phase-3-progress.md` (both rows, plus
a new unchecked box for the deletion), and **soren-ds's
`012-soramech-runtime.md`**, which claimed this was "the one place Soren DS
diverges from soramech proper" — no longer true, so I reframed it rather than
leaving a false statement about a sibling project.


## Not committed, deliberately


The soramech tree was already dirty when I arrived, with work that isn't mine:
issue **103** staged as a rename out of `completed/` (someone else reopening
it), modifications to **111**, **233**, and `phase-1-progress.md`, plus a batch
of transcript exports. My two `git mv`s are staged alongside that pre-existing
staged rename. Tell me how you want to separate them and I'll commit only the
multi-spawn changes.


soren-ds is worse for this — it sits inside the
`/mnt/mtwo/programming/ai-stuff` monorepo, which is dirty across half a dozen
unrelated projects. I only edited the one doc there.


Two things I deliberately left alone, so you can overrule me: **source comments
and `.info.md` files.** `012-dispatch.info.md`, `010-graph-loader.h`,
`001-schema.lua`, and `assets/js/004-inspector.js` all still describe the gate
— accurately, because the code still has it. Making them state the ruling now
would make them lie in the other direction. They're itemized in 304 and 305's
implementation steps so they change when the code does.


Want me to take the source change next? 305 first is the natural order — the
marker has to stop existing before the dispatch branch on it can go.


--------------------------------------------------------------------------------

### User Request 3

Okay what other contradictions can we work on? I want to mostly focus on
documentation this run.


--------------------------------------------------------------------------------

### Assistant Response 3

Good — let me audit the docs against the source and against each other.
Starting with claims I can verify mechanically.


Found a substantial one. Let me confirm its full shape.


Good — the test-coverage doc already avoids hard-coded counts, and the editor
doc covers all seven kinds. Let me fix the unambiguous defects I found.


Now the biggest finding — how a run actually starts.


There's already an issue that owns this concept. Let me read it rather than
create a duplicate.


`docs/001-architecture.md` doesn't exist, yet 22 files reference it —
including a live doc, live source, and two open issues. Let me finish the
entry-box thread first, then show you that.


Issue 103 already caught this in one spot. Let me fix the live references.


Let me check what's left of that broken reference, and sweep for any other
broken doc links.


A systematic authoring bug: issues written in `issues/` link to `completed/X`,
and nobody re-relativizes when they move. Let me verify the targets and fix.


Found and fixed six contradictions. Every markdown link across `docs/`,
`issues/`, and `issues/completed/` now resolves — that was not true when I
started.


## The big one: `entry_box_id` is decorative but mandatory


The runner does **not** use `meta.json`'s `entry_box_id` to pick where a run
starts. `detect_entry_boxes()` derives the set from topology — every
`call`/`write` box whose non-optional ports have no non-read feeders. The
declared field's only consumer anywhere in the C runtime is the word after
`entry` in the startup banner.


Three things make this worse than a dead field:

- `001-schema.lua` **hard-errors** if it's missing, so authors are compelled to
  name a box the runtime ignores.

- Your own quickstart points it at `seed`, a **read** box — the one kind
  `detect_entry_boxes` categorically excludes. So the banner names as "the
  entry" a box that cannot be one.

- No doc explained the real rule.


`docs/004-runtime.md` now has a "Which boxes start the run" section documenting
the actual derivation. And there was already an issue that owns this: **206 —
entry box designation**, closed "won't implement" with the reasoning *"the
`entry_box_id` field becomes vestigial when phase 3 lands."* It called this
correctly and then only half of it happened — the field went vestigial, nobody
retired it. Reopened with the decision it owes: drop it, promote it to an
authoritative override (206 explicitly left that door open), or demote it to
optional. I didn't pick — that's a design call.


## `docs/001-architecture.md` doesn't exist; 22 files cite it


Renamed to `001-overview.md` at some point with no reference sweep. Live ones
fixed: `docs/006-test-coverage-map.md`, `src/001-schema.lua` (retargeted to
`002-map-model.md`, since that's where the format it validates actually lives),
and open issue 247 — which cited `001-architecture.md:651` and `:638-666` for
the event format. I replaced the line numbers with a section name; line-numbered
citations into prose go stale on the next edit anyway.


Issue 103 had already caught this in exactly one spot, which is how these things
usually surface — one person notices locally, nobody generalizes.


## `completed/completed/` — a systematic authoring bug


Issues get written while in `issues/`, so their sibling links say `completed/X`.
When the issue itself moves to `completed/`, nobody re-relativizes, and the link
resolves to `completed/completed/X`. Fixed in 323, 324, and 325. Worth knowing
it recurs on every issue completion — a pre-commit link check would catch it
permanently.


## Three smaller ones


- **Six vs seven routing kinds.** The enum, schema, docs, and the editor's own
  dropdown all ship seven. Two comments in `004-inspector.js` said "Six kinds
  ship" and omitted `nonlinearity` entirely — a comment contradicting its own
  file 1000 lines later. Fixed both, and dropped the stale "multi-spawn" label
  on iterator while I was in there.

- **Push vs pull.** `001-overview.md` said *"The seed box pushed `world` along
  its wire"* — the opposite of the pull-on-demand model `002-map-model.md`
  documents. Rewrote it to explain why a two-box map reports one task, which is
  the observable that makes the model click.

- **The apartment-building metaphor.** The table of contents advertised it as
  living in `notes/vision`. It's in `001-overview.md:32`; the vision has no such
  passage. Fixed, and I also flagged in the TOC that the vision is written in
  the pre-233 dialect.


I also expanded the read-box section in `002-map-model.md` — round-robin
across multiple read predecessors, read-as-default when mixed with real
producers, and load-time file caching were all shipped in 244 and entirely
undocumented.


## Two I did not touch, because they're your call


**`notes/vision` is written in a dead dialect.** It describes `kind: "branch"`
and `"data"` boxes, `outputs[]` tuples, `from_output` wires, `drivers.json`, and
`soramech-runner.lua` — none of which survive. Since your process is "read the
vision, then generate docs from it," a vision in retired vocabulary will keep
seeding wrong docs. But it's your writing and it's the historical seed, so:
annotate it in place, rewrite it to current vocabulary while keeping the intent,
or leave it and rely on the TOC warning I added?


**20 completed issues still name `docs/001-architecture.md` in plain text.** Not
broken links (they're prose references), so nothing flags them. But if completed
issues are the blueprint for rebuilding the project from scratch, a pointer to a
nonexistent doc is a dead end for whoever follows it. Sweep them, or leave
completed issues as historical record?


My recommendation on both: annotate the vision rather than rewrite it (the
intent is still accurate, only the schema rotted), and do sweep the 20 — it's
mechanical and the reconstruction story is the reason those files exist.


Still uncommitted, same reason as before: the tree has 111, 233,
phase-1-progress, and the 103 reopening from someone else's session mixed in
with mine.


--------------------------------------------------------------------------------

### User Request 4

  The big one: entry_box_id is decorative but mandatory


we shouldn't have one single entry box.


  docs/001-architecture.md doesn't exist; 22 files cite it


let's get the architecture pinned down then, and make it.


  Issues get written while in issues/, so their sibling links say completed/X.
  When the issue itself moves to completed/, nobody

  re-relativizes, and the link resolves to completed/completed/X. Fixed in 323,
  324, and 325. Worth knowing it recurs on every issue

  completion — a pre-commit link check would catch it permanently.



Um, we should just not do that then?


--------------------------------------------------------------------------------

### Assistant Response 4

Three decisions taken. Let me start with the architecture doc, since it's the
substantial one — I need accurate internals first.


`008-pool-runner.info.md` still describes the runner as a scaffold that "prints
a usage banner and exits 0" — another stale doc. Let me gather the rest of the
internals.


Several info.md files describe deferred work that has since shipped. Let me
verify the component layout directly rather than trust them.


`015-large-value-heap.h` is included by nothing but itself — the unified
allocator superseded it. Let me confirm against the build.


I have the layout confirmed. Writing the architecture doc.


Now recording your ruling on 206.


Now the link convention. Let me size the problem before picking the rule's
scope.


60 links, but the fix is a go-forward rule. Let me record it in the project's
house rules.


Now converting the links I introduced this session so I'm not perpetuating the
pattern.


Now the sweep of the 20 remaining `001-architecture.md` citations. Let me see
what forms they take.


Let me verify every link still resolves after the sweep and the new doc.


All three done.


## 1. No single entry box — decided and recorded


Issue 206 now carries your ruling: **drop the field.** The derived set is the
only answer, and the manual-override door the original resolution left open
stays shut — an override would just reintroduce the singular-`main` model
you're rejecting.


Sequencing is the one thing worth knowing: the quickstart in `001-overview.md`
teaches `entry_box_id`, and it has to keep teaching it until the schema stops
*demanding* it. Remove the doc first and the quickstart produces a map that
fails validation. So step 1 is the schema, step 5 is the doc.


Also noted: maps that still carry the field keep loading — it becomes ignored,
not an error, so no fixture breaks. And the banner should print the derived set
(`3 entry boxes: seed, tick, config`) with a loud warning when it's empty, since
an empty entry set is legal but always a mistake — nothing can ever fire.


## 2. `docs/007-architecture.md` — written


Numbered 007, not 001, because `001-overview.md` holds that index and
renumbering would break more references than it fixes. It sits in a new TOC
section — the existing 001–006 are the user-facing reading path; this one is
for people changing the runtime.


What it pins down:


- **Three programs, one format** — why the editor, the file server, and the
  pool runner never call each other, and why that constraint is what forces the
  map format to be plain JSON-per-box. A format only one program could read
  would collapse the three into one.

- **The module stack bottom to top**, with the include graph verified rather
  than assumed: `libs/task-pool` and `libs/json` at the base, then the unified
  allocator, slot store, graph loader, spec registry, dispatch, and the
  transcript pair.

- **The path of one value** — ten steps from JSON file to downstream byte.
  Steps 5–9 are the whole execution model; everything else exists to make
  those cheap or observable.

- **The language plugin boundary** — nothing about Lua, C, or Bash is
  special-cased; the three shipped languages use exactly the interface a fourth
  would.


Then I swept all 25 citations of the dead path across 20 files to point at it,
leaving the explanatory annotations intact so the rename stays legible.


## 3. Not doing that anymore


Recorded in the project's `CLAUDE.md` house rules: **never link between issue
files by path.** Write `issue 305 (C graph loader)` — number for lookup, title
so a human knows what it is. An issue's path changes every time it completes or
reopens, so a path link between issues is wrong half the time. Docs cite issues
the same way; doc-to-doc links stay relative, since docs don't move.


Making issue numbers clickable is the HTML mirror's job, which your CLAUDE.md
already specifies. I converted the links I introduced this session so I'm not
perpetuating it. About 60 older ones remain and are correct *today* — they
only break when the citing file itself moves.


Every markdown link across `docs/`, `issues/`, and `issues/completed/` resolves.


## Two things I found while writing the architecture doc


**The `.info.md` layer has rotted, and it's the layer your own instructions say
to read instead of source.** Four confirmed:


- `008-pool-runner.info.md` calls the runner a **scaffold** that "prints a usage
  banner and exits 0." It's a 125 KB binary that runs maps.

- `010-graph-loader.info.md` is the worst — lists topology validation, cycle
  detection, entry-box detection, size classes, and language enumeration as
  deferred. All five shipped. It also says randomizer/weighted/distributor "the
  parser doesn't accept them yet."

- `011-spec-registry.info.md` and `016-unified-allocator.info.md` both list
  integrations as deferred that the include graph shows are wired in.


**`015-large-value-heap.c` has no callers.** It compiles into the binary and has
a passing unit test, but nothing includes its header — the unified allocator
took over variable-size payloads and 015 was never removed. Its own test is what
keeps it alive. Both are recorded under "Known structural debt" in the new doc.


The info.md sweep is the natural next documentation run if you want it — it's
~4 files minimum, maybe more once I check the rest. And `notes/vision` is still
in the dead dialect; I flagged it in the TOC last turn but you haven't said
whether to annotate it or rewrite it.


Still uncommitted — the tree has 111, 233, phase-1-progress, and the 103
reopening from someone else's session tangled with mine.


--------------------------------------------------------------------------------

### User Request 5

  The .info.md layer has rotted, and it's the layer your own instructions say to
  read instead of source.


okay let's fix that.


And

  notes/vision is still in the dead dialect; I flagged it in the TOC last turn
  but you haven't said whether to annotate it or rewrite it.


we shouldn't touch the notes files.


--------------------------------------------------------------------------------

### Assistant Response 5

Understood — `notes/` stays untouched. My TOC entry warns readers about the
dialect without editing the note itself, which is the right split.


Now the info.md layer. Let me triage all of them first.


26 files. Let me verify the specific claims in the ones I haven't read.


Now I have accurate APIs. Let me verify the type surface before rewriting the
worst file.


Now rewriting the worst offender.


Let me verify the remaining C modules in the live path.


Two more, including a stale function signature. Let me verify.


Stale function signatures — the worst kind, since this file exists to be read
instead of the source. Fixing both.


The JSON writer is fully implemented — the info.md calls it a stub.


All three language spec docs are missing the contract surface that issue 325
added. Let me check what each actually declares.


All three language specs are documented as scaffolds with NULL callbacks. Let me
get their real surface.


These three describe the phase-2 Lua runner accurately but don't say it's
superseded. Adding that.


The schema module speaks the current dialect; its info.md describes the retired
one — the reverse of the validator. Let me see the whole file.


Done. I audited all 26 `.info.md` files against source and fixed 16. `notes/`
untouched — the TOC warns readers about the dialect without editing the note.


## The rot was worse than "stale"


Four files documented **fully-built components as scaffolds with NULL
callbacks**:


- All three `langs/*/spec.info.md` — "Currently a scaffold… `init`,
  `teardown`, `compile`, `invoke` — all `NULL` (stub)." The Lua spec is 1000+
  lines with per-worker `lua_State`, table serialisation, and sentinel handling.

- `008-pool-runner.info.md` — "Prints a usage banner and exits 0."


Two had **wrong function signatures**, which is the worst failure mode for a
file that exists to be read *instead of* the source:


- `pool_spawn` takes four arguments; the doc showed three, missing `priority`
  entirely. `pool_init_barrier` returns `int`, documented as `void`. And
  `pool_set_worker_init` / `pool_set_worker_teardown` weren't documented at all
  — while the same file called per-worker init "deferred." It's the hook that
  makes per-worker init work.

- `libs/json` called its writer a stub where "every entrypoint sets `w->err`."
  It's fully implemented with depth tracking, capacity checks, and escaping —
  and it's what serialises every transcript event.


Then a cluster of **"deferred" claims for work that shipped**: the event queue's
Vyukov MPSC ring (29 atomics in the file), both verbosity gates, four of nine
JSONL emitters, the unified allocator's slot-store integration and quiescence
sweep, and the spec registry's `/proc/self/exe` discovery.


## Two documented backwards from each other


`002-validate-map.info.md` and `001-schema.info.md` both described the pre-233
dialect — but for opposite reasons, and that distinction matters for whoever
fixes them:


- The **validator's source is stale** (it really does still branch on `kind ==
  "branch"` and match `from_output`). Its doc was accurate about a broken
  program. I marked it as the retired dialect and pointed at issue 103.

- The **schema module's source is current** (it speaks `from_branch` and routing
  kinds); only its doc rotted. I rewrote it from the actual functions —
  including `check_input_bindings`, which was undocumented despite being the
  graph-level check that decides whether a port is satisfied.


## The orphan now says so


`015-large-value-heap.info.md` opened with "Used by the slot store" — false.
It has no callers; the unified allocator took the job because it can reclaim and
this heap never frees. It now leads with the fact that it isn't wired in, and
why it stayed invisible: a green unit test on an unreachable module.


I also added supersession banners to the three phase-2 Lua runner docs. They
were accurate about what the code does but never said `soramech-pool` is what
actually runs — a reader could pick up `007-runner-main` thinking it was the
current path.


## Smaller things swept along


`012-dispatch.info.md` correctly describes today's spawn guard, so I left the
description and added a forward note that 304 removes it. And I fixed the last
"multi-spawn" vocabulary leak in `001-schema.lua`, closing out step 9 of issue
304.


One judgment call worth flagging: I kept "deferred" sections where they're
*true* — the sentinel function-pointer stub, per-class allocator locks,
size-class piping. The goal was accuracy, not deleting every forward-looking
note.


Still uncommitted, same tangle as before: 111, 233, phase-1-progress, and the
103 reopening from another session are mixed in with mine. That's now three
sessions' worth of doc work sitting unstaged — worth untangling before it
grows.


--------------------------------------------------------------------------------
