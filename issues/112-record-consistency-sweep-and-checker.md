# 112 — Record consistency: the contradiction sweep and the checker that prevents the next one

## Status

open · phase 1 · survey completed 2026-08-02; fourteen findings
below, **one resolved** (finding 4, the duplicate issue number,
2026-08-03). Six are mechanically detectable and belong to a
checker; eight need a human ruling on which side is wrong. Two
open questions remain — the renaming question was answered on
2026-08-03 and its answer is recorded in finding 4.

## Current behavior

The project makes a specific promise about its own records:
*the project should be able to be completely recreated from
scratch by re-completing each of the issue files in the
completed directory.* The blueprints are the product, and the
source tree is what falls out of following them.

That promise does not currently hold. A survey on 2026-08-02
found fourteen places where two parts of the record disagree —
a completed issue against the code it produced, a document
against the runtime it describes, a house rule against the
corpus that is supposed to obey it, and in one case an issue
number against another issue with the same number.

None of these are hidden. Several are annotated in place with a
note explaining the discrepancy, which is the failure mode
worth naming first: **a note explaining a contradiction is
itself a form of drift.** It preserves both the wrong statement
and the correction, doubles what a reader has to hold, and
converts a fixable error into a permanent feature of the
document. `docs/006-test-coverage-map.md` demonstrates this
exactly — it correctly annotates one renamed document path and
then cites two documents that have never existed, in the same
list, three lines apart.

## Intended behavior

Every finding below is resolved by **deleting the side that is
wrong**, not by annotating the disagreement. Where the code is
right, the prose changes and the old prose goes away. Where the
prose is right, the code changes. Where both were reasonable at
different times, the older one is removed and its reasoning is
carried forward into the surviving text as an ordinary
sentence, not as a footnote about what things used to be.

The exception is a genuine ruling that reversed — those already
have a place to live, which is the issue file that made the
ruling, in its own body rather than in an append-log.

Then: the classes of finding that a script can detect become a
script, wired into `make test`, so the next one fails a build
instead of waiting two months for a survey.

## The findings

Ranked by how badly each one misleads someone building from the
record. The first three actively teach a wrong model of the
runtime; the middle group breaks lookup and navigation; the
last group is dead weight.

| # | Finding | Kind | Detectable |
|---|---------|------|------------|
| 1 | the C box rule contradicts the C box product | rule vs code | no |
| 2 | the runtime doc states the spawn ruling as fact, then footnotes that it isn't | doc vs code | no |
| 3 | two entry-box concepts coexist in the loader | code vs code | no |
| 4 | ~~issue number 111 is used twice~~ **resolved 2026-08-03** | record vs record | **yes** |
| 5 | three completed issues ship a header that does not exist | issue vs tree | **yes** |
| 6 | phase 1 is complete in one place and not in another | record vs record | **yes** |
| 7 | the no-path-links rule is violated by most of the corpus | rule vs record | **yes** |
| 8 | two cited documents have never existed | doc vs tree | **yes** |
| 9 | a retired subsystem is still in the tree | issue vs tree | partially |
| 10 | build output location contradicts the RAM-tier rule | rule vs tree | partially |
| 11 | the "data box" is a kind that no longer exists | issue vs code | no |
| 12 | the clock is implemented twice | code vs code | no |
| 13 | two unrelated C-parsing efforts, neither aware of the other | plan vs plan | no |
| 14 | the vision document is in a dialect the project no longer speaks | doc vs code | no |

### 1 — The C box rule contradicts the C box product

`issues/completed/307-c-language-spec.md:172` states, and marks
non-negotiable: *"No header to include, no macro, no
`int classify(SM_BOX_ARGS)`. Just a function with a typed
signature... SoraMech bends to the language. The language never
bends to SoraMech. This is non-negotiable across every spec."*

The same file's implementation log records what shipped: a
fixed signature, `int fn(const void **inputs, const int *sizes,
int n, void *out_buf, int out_capacity, int *out_size)`, with
the typed-wrapper generator that would have honoured the rule
deferred to a future-enhancements section. `docs/005-writing-
boxes.md:101` teaches that fixed signature to box authors as
the contract.

So the project's most emphatic stated principle is contradicted
by the code written to implement it, in the same file, and the
user-facing documentation sides with the code.

This one reaches forward: the HDL parent issue (501) cites that
rule as the precedent for its own ground rule 1, which says a
hardware box's source may contain no SoraMech-specific syntax.
If the rule is not real, that ground rule needs a different
justification or a different shape.

**The ruling needed:** is the rule aspirational or binding? If
binding, the typed-wrapper generator stops being a future
enhancement and becomes required work, and the fixed signature
becomes a documented interim. If aspirational, the
non-negotiable language comes out of 307 and the honest
statement — *SoraMech asks C boxes for one fixed signature* —
replaces it everywhere, including in 501.

### 2 — The runtime doc states the spawn ruling as fact, then footnotes that it isn't

`docs/004-runtime.md` says in its own voice: *"Same-box
concurrent fires are not gated. There is no single-spawn
invariant and no multi-spawn marker."* A blockquote below it
concedes that `soramech-pool` still carries the CAS spawn guard
and the iterator-seeded marker walk.

The source agrees with the blockquote, not the body:
`propagate_multi_spawn()` at `src/010-graph-loader.c:2108`, its
call site at 2159, the marker still selecting ring size and
slot mode at 2175–2181, and the CAS guard at
`src/012-dispatch.c:342`.

Two issues are reopened to remove them (304, task dispatch
layer; 305, C graph loader), so the direction is settled. What
is wrong is the document's tense: it describes a decision as
though it were a behaviour.

**The ruling needed:** none, really — this is a prune. The
body moves to the future tense or the section moves behind the
work, the blockquote disappears, and the doc stops asserting
something a reader can disprove by running the binary.

### 3 — Two entry-box concepts coexist in the loader

`src/010-graph-loader.h` exposes `graph_entry_box_id()` — the
`entry` field declared in the map's meta — and also
`graph_n_entry_boxes()` / `graph_entry_box()`, the computed
detection that finds boxes whose required inputs are all fed by
read boxes.

Issue 206 (entry box designation), reopened 2026-07-26, records
that the declared field went vestigial exactly as predicted and
was never retired: still mandatory in the schema, still printed
in the runner banner, and still commonly pointed at a read box,
which cannot be an entry at all.

So the schema requires a field, the banner reports it, and the
runtime decides the real answer somewhere else.

**The ruling needed:** retire the field, or give it a job. The
plausible job is an override — declared entry wins, computed
detection fills in when absent — which is a real feature and
should be a decision rather than a leftover.

### 4 — Issue number 111 was used twice · resolved 2026-08-03

`issues/completed/111-bash-output-format-and-driver-test-
entry.md` and what was then `issues/111-restore-phase-1-demo-
maps-to-current-schema.md` were different issues with the same
number. Every citation of "issue 111" was ambiguous, and the
house convention of citing by number-plus-title papered over it
without fixing it.

Found by one line of shell, which is the argument for the
checker:

```sh
ls *.md completed/*.md | sed 's|completed/||' \
  | grep -oE '^[0-9]+[a-z]?' | sort | uniq -d
```

**The ruling, given 2026-08-03:** renaming an issue file is
permitted. Move the newer of the colliding pair, always update
every citation, and leave no tombstone at the old number when
no reference to it survives.

**What was done.** The demo-map restoration issue — the newer
of the two — became 113. Four citations moved with it: issue
103's implementation step and its related-documents list, issue
233's note about maps outside the test sweep, and the phase-1
progress table. Two apparent citations were checked and
correctly left alone: issue 110's *"Issue 111 created for bash
output format confusion"* points at the original 111, which did
not move; and issue 316's *"111 / 113 tests passed"* is a test
count, not a reference. That second one is a warning for the
checker — a bare number in prose is not a citation, and a
naive renumbering tool would have corrupted it.

**The general rule this establishes**, for the checker and for
future collisions: the newer file moves, citations are updated
in the same commit as the rename, and the old number is simply
free again. The record does not carry a marker saying a
renumbering happened, because a marker at the old number would
be a reference to it, which would defeat the point of having
none.

### 5 — Three completed issues ship a header that does not exist

`langs/c/soramech-c.h` appears in 307's implementation sequence
(step 5), in 309's build-system directory tree, and in 223's
file listing. `langs/c/` contains `Makefile`, `lexer.js`,
`spec.c`, `spec.js`, `spec.info.md`, and the built `spec.so`.
There is no header.

This is the promise in the Current-behavior section failing
directly: following the completed issues does not reproduce the
tree.

**The ruling needed:** write the header, or strike it from all
three. It is entangled with finding 1 — the header's stated
contents were the `SM_BOX_ARGS` macro, which the no-boilerplate
rule forbids.

### 6 — Phase 1 is complete in one place and not in another

`docs/000-table-of-contents.md` lists *phase 1 (foundation):
complete*. `issues/phase-1-progress.md` carries a "Post-phase
maintenance" section listing 103 reopened and the phase-1 demo
maps open, with the note that the maps and the standalone
validator both rotted because nothing in the test suite reaches
them.

**The ruling needed:** none; the progress page is right and the
table of contents is stale. Worth deciding whether the table of
contents should carry a phase status at all, given it will
always be the copy nobody updates.

### 7 — The no-path-links rule is violated by most of the corpus

`CLAUDE.md` is explicit: never link between issue files by
path, because an issue's path changes every time it completes
or reopens. Write `issue 305 (C graph loader)` instead.

Violations: issue 419's sub-issue list, all four phase-progress
pages' issue tables, and `docs/004-runtime.md`'s conformance
blockquote, which links `../issues/304-...` and
`../issues/305-...` — paths that were correct only because both
issues happen to be reopened right now, and that were wrong for
the months they sat in `completed/`.

**The ruling needed:** none; the rule is the rule. The work is
a mechanical sweep, and then the checker keeps it swept. Note
that the phase-5 files added 2026-07-31 follow the rule, so the
corpus is currently inconsistent with itself in both
directions.

### 8 — Two cited documents have never existed

`docs/004-ipc-and-threading.md` is cited by
`docs/006-test-coverage-map.md`, by 307's Concept section, and
by 425 and 428 as a document to update.
`docs/005-language-specs.md` is cited by 307 twice, once as the
home for the box-author thread-safety rule.

Neither exists. `docs/` holds 000 through 007. The thread-
safety rule that 307 promised to put in `005-language-specs.md`
lives in `docs/005-writing-boxes.md`, which is a different
document that happens to share a number.

**The ruling needed:** none; retarget the citations. The
interesting question is whether the numbering collision between
the promised `005-language-specs.md` and the actual
`005-writing-boxes.md` caused the confusion, which would be an
argument about how document numbers get assigned.

### 9 — A retired subsystem is still in the tree

`drivers/` holds `README`, `bash.sh`, `c.sh`, `lua.sh`. Issue
307 lists `drivers/c.sh` as *"phase 2 C driver, retired by this
spec."* Nothing in the `Makefile`, in `docs/`, or in
`src/*.lua` references the directory.

**The ruling needed:** delete, or state why it stays. The house
rule about deprecated files — mark with a `-done` suffix, keep
for one commit so it appears in the record, then remove — was
written for exactly this and was not applied.

### 10 — Build output location contradicts the RAM-tier rule

`CLAUDE.md` states that ephemeral output goes through the
two-tier RAM scheme, with `tmp/shared-memory/` → `/dev/shm/
soramech` for logs, builds, and artifacts. `build/` sits in the
repository root holding `libs/`, `src/`, and `tests/`
subdirectories, gitignored under a comment crediting issue 309.

Both are real and they disagree about where a build lives.

**The ruling needed:** either `build/` moves under the RAM tier
and the root entry becomes a symlink, or the rule is amended to
say that compiled objects are exempt. There is a real argument
for the exemption — a build that vanishes on reboot forces a
full rebuild — and it should be written down rather than
implied by the layout.

### 11 — The "data box" is a kind that no longer exists

Issues 229 (data box as language-agnostic file I/O) and 244
(data box pull on demand) are written around a `data` box kind.
`src/001-schema.lua`'s valid kinds are `call`, `read`, `write`,
`map`. `src/010-graph-loader.h`'s enum has `BOX_CALL`,
`BOX_READ`, `BOX_WRITE`, `BOX_MAP`. `docs/002-map-model.md`
describes the pull-on-demand behaviour under `read`.

Two completed blueprints describe a kind you cannot create.

**The ruling needed:** whether the rename is recorded in the
two issues' bodies (updating the current-behavior sections,
which the house rules already permit) or whether a short note
in `docs/002-map-model.md` suffices. The risk of leaving it is
that "data box" is still spoken language around the project
while being unbuildable.

### 12 — The clock is implemented twice

`now_secs()` and `mono_us()` are file-static in both
`src/012-dispatch.c:137,144` and `src/008-pool-runner.c`, which
calls `clock_gettime` at 123, 195, and 379.

Not a textual contradiction — a single-source-of-truth break.
It is listed here because the counter box (251a) makes a third
consumer, and a third copy is where this stops being tidy and
starts being a bug surface: the transcript's timestamps and a
box's sense of elapsed time would come from separately
maintained code.

**The ruling needed:** none; one time module, three callers.
This is the cheapest finding to fix and the one with a
dependent waiting on it.

### 13 — Two unrelated C-parsing efforts

`docs/006-test-coverage-map.md` plans `langs/c/parser.js` —
*"write it and its tests together when a C parser commits to a
shape"* — for the editor's signature parsing.
`langs/c/` currently holds `lexer.js` and `spec.js` but no
parser. Issue 504 (the C front end and dialect checker),
written 2026-07-31, proposes a hand-written C front end in C
for the hardware path.

Both parse C. Neither mentions the other. They have genuinely
different requirements — one runs in a browser against a
signature line, the other runs at compile time against a whole
translation unit — but that difference should be stated
somewhere, or one of them should be dropped.

**The ruling needed:** are these one effort or two? If two,
each issue says so and says why.

### 14 — The vision document is in a dialect the project no longer speaks

`docs/000-table-of-contents.md` flags this honestly:
`notes/vision` describes `branch` and `data` box kinds,
`outputs[]` tuples, `from_output` wires, and a Lua runner —
none of which the project still has.

It is listed last because the annotation is arguably correct
here in a way it is not elsewhere: a vision document is a
historical statement of intent, and rewriting it to match the
implementation would destroy the thing it exists to preserve.

**The ruling needed:** confirm that the vision is deliberately
exempt from the prune-don't-annotate rule, and say so in the
document itself rather than only in the table of contents, so
the exemption travels with the file.

## The checker

Findings 4 through 8, and half of 9 and 10, are mechanical.
They should be a script — `scripts/check-record.sh` — run by
`make test`, failing the build with the offending file and line
rather than a summary.

What it can check today:

| Check | Method |
|-------|--------|
| duplicate issue numbers | the `uniq -d` line above |
| path links between issue files | grep `](` in `issues/**.md`, excluding links into `docs/` |
| dangling document citations | extract `docs/NNN-*.md` mentions from every markdown file; verify each exists |
| dangling file citations | extract backticked paths from "Relevant files" sections; verify each exists or is marked as proposed |
| orphan directories | a top-level directory referenced by nothing in `Makefile`, `docs/`, `src/`, or `scripts/` |
| progress-page status against the table of contents | only if phase status becomes structured rather than prose |

The fourth check needs a convention, because a "Relevant files"
list legitimately names files that do not exist yet — that is
what a blueprint does. The distinction between "this file will
exist when the issue is done" and "this file was claimed and
never written" is exactly the distinction finding 5 turns on,
and the checker cannot make it without help. A marker on
not-yet-existing paths is the obvious answer and it is one more
thing for an author to remember.

What it cannot check: findings 1, 2, 3, 11, 12, 13, 14 — every
case where prose and code disagree about meaning rather than
about a path. Those need a person, which is why this issue
exists as a list rather than only as a script.

## Open questions

1. **What marks a "Relevant files" entry as not-yet-existing?**
   Needed before the checker's fourth check can run without
   drowning in false positives.
2. **Should a completed issue whose design was later reversed
   be edited in place?** The house rule says update the body
   and the current-behavior section rather than appending a
   log. Findings 1 and 11 are both cases where a completed
   issue's *design* section, not just its status, describes
   something the project walked away from — and editing the
   design section of a completed blueprint changes what
   re-completing it would build.

## Suggested implementation steps

1. **The checker first**, covering duplicate numbers, path
   links, and dangling document citations. Three checks, all
   unambiguous, all currently failing — so the first run
   produces a work list rather than a green tick.
2. **Fix what the checker finds**: findings 4, 6, 7, 8. These
   are mechanical once the rulings in 4 are made.
3. **Finding 12**, the shared time module, because 251a is
   waiting on it and it is small.
4. **Findings 2 and 3**, the two places the documentation and
   the schema teach a wrong model of the runtime. Both are
   entangled with reopened issues (304, 305, 206) and should
   land as part of that work rather than separately.
5. **Finding 1**, which needs a real decision about what the
   C box contract is, and which changes 307, the box-author
   guide, and the hardware phase's ground rules together.
6. **Findings 5, 9, 10, 11, 13, 14**, each a small prune once
   its ruling is made.
7. **Extend the checker** with the file-citation and
   orphan-directory checks, once conventions 2 and 3 exist.

## Relevant files

- `scripts/` — where the checker lands; `make test` is the
  caller
- `docs/004-runtime.md` — finding 2
- `docs/006-test-coverage-map.md` — finding 8, and the
  half-repaired citation list that motivates the
  prune-don't-annotate rule
- `docs/000-table-of-contents.md` — findings 6 and 14
- `src/010-graph-loader.{c,h}` — findings 2, 3, 11
- `src/012-dispatch.c`, `src/008-pool-runner.c` — findings 2
  and 12
- `langs/c/` — findings 1 and 5
- `drivers/` — finding 9
- issue 103 (runner graph loader and validator) and the
  phase-1 demo-map restoration issue — the two prior instances
  of this same drift, both diagnosed with the same cause
- issue 206 (entry box designation) — finding 3
- issue 304 (task dispatch layer) and issue 305 (C graph
  loader) — finding 2's code half
- issue 307 (C language spec) — findings 1 and 5
- issue 504 (C front end and dialect checker) — finding 13
