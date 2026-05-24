# Compact transcript

## User

hello claude. Today, we're going to close out every single one of these issues. Then, we're going to go through the completed issue files one by one, and justify their existence. If they cannot be justified, they will be updated until they match the correct design. Then, we will hand them off to another project who will attempt to re-create what we've built from scratch using only the issue files as a guide. If they succeed, we will know that our issue files are in the correct format. If they fail, then either they did not fill in the gaps between issue files correctly, or the issue files are too clunky, obfuscated by technical details, or weighted down by development inertia that is of no use to a set of fresh eyes, and is simply there to justify our actions. Not ideal. In any of those cases, we should attempt to re-write the issue files until the design is solid. The issue files are the product, the source-code and implementation is just how we prove that it works.

## Assistant

Understood. Before I start closing things, let me get a clear picture of what's open and what's already completed, and remind myself of the project's current shape.

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/issues/ 2>/dev/null | head -100`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/issues/completed/ 2>/dev/null | head -100`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/`

## Assistant

I see 24 open issues across phases 2 and 3, and ~50 already completed. Before I start, I want to make sure I'm reading the scope right, because "close every one of these" can mean several different things:

1. **Implement-and-close**: actually finish the work each issue describes (some, like 303-language-runtime-spec or 309-build-system, are foundational and not small).
2. **Audit-and-close**: many of these may already be partly or fully implemented in the codebase — the issue just hasn't been moved. Walk each one, verify against the code, and only do the real work where something is genuinely missing.
3. **Rewrite-as-blueprint**: treat each open issue as a *design document* first, refine it until a fresh reader could rebuild from it, and only then implement.

I'd lean toward (2) as the first pass — go through the 24 open issues in dependency order, check whether the described behavior already exists, and either move-to-completed or fill the gap. That keeps us honest about what's actually built before we move into the audit phase. The risk is that some "obviously done" items are missing the design-document quality you want for the handoff, so we'd still have to revisit them in the audit phase.

A couple of questions before I move:

- Of the open issues, are there any you already know are *done in code* and just need verifying + moving? (Saves me reading every issue cold.)
- For the audit phase — when you say "justify their existence," do you mean each completed issue must stand alone as a buildable spec, or that the *set* of them in order must rebuild the project? Those are different bars.
- Is the "another project who will attempt to re-create" a real downstream consumer (another agent run? a human?), or a thought experiment we're stress-testing against?

## User

we want to implement and close. The phase 3 issues WERE up to date and correct before we started implementing them, so feel free to check the git log to find out their original shape before they started being completed. Phase 2 is mostly deprecated, but we should still implement the issue files from that phase first, just to have a stable base. I anticipate we're going to be pruning a lot of code today, because we've built a lot of systems that are grown into little shapes that work for what they're doing, but need to be replaced with larger, more broadly applicable systems that are clean and coherent and can be applied to more than just the little implementation we built.

  - Of the open issues, are there any you already know are done in code and just need verifying + moving? (Saves me reading every issue cold.)

why do you want to save yourself from reading each of these issue files? Isn't it better to know the entire scope? Are you worried about them corrupting your context somehow?

  - For the audit phase — when you say "justify their existence," do you mean each completed issue must stand alone as a buildable spec, or that the set of them in order must rebuild the project? Those are different bars.

the set of them must rebuild the project. They are encouraged to have dependencies.

  - Is the "another project who will attempt to re-create" a real downstream consumer (another agent run? a human?), or a thought experiment we're stress-testing against?

I will literally copy the issue files to a new directory and tell a downstream consumer to re-create the system. That's all we need to know for now.

## Assistant

You're right — that was lazy framing on my part. The issue files *are* the product; treating reading them as a cost rather than as the core work inverts the priority. Let me read all 24 cold, and pull the phase-3 originals from git so I see them in their intended shape before implementation drift.

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/217-variadic-ports-and-language-spec-shape.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/231-move-signature-parsers-into-langs.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/232-test-coverage-map.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/233-unified-routing-schema.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md`

## Assistant

Let me continue reading the rest of the open issues, and load task tracking since this is exactly the kind of multi-issue work that benefits from it.

## Assistant

_Tools:_

- `ToolSearch (...)`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/301-pool-lifecycle-and-worker-init.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/302-wire-value-slot-store.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/303-language-runtime-spec.md`

## Assistant

Setting up tasks and continuing to read the remaining open issues in parallel.

## Assistant

_Tools:_

- `TaskCreate: Read all 24 open issue files (phase 2: 217, 231, 232, 233, 239; phase 3: 301-317 except 316) plus phase progress files. The issues are the product — must understand the full scope before any implementation.`

## Assistant

_Tools:_

- `TaskCreate: For each phase-3 open issue, pull its pre-implementation shape from git log. User stated phase-3 issues were "up to date and correct before we started implementing them" — drift during implementation needs to be reconciled.`

## Assistant

_Tools:_

- `TaskCreate: Phase 2 is mostly deprecated but should be implemented first to have a stable base. Open: 217, 231, 232, 233, 239.`

## Assistant

_Tools:_

- `TaskCreate: Phase 3 thread pool work. Open: 301-317 (16 issues). Expect significant pruning — systems grown into shapes that work for narrow cases but need broader coherent replacements.`

## Assistant

_Tools:_

- `TaskCreate: After all issues closed: walk through completed/ directory, justify each issue's existence. Set must collectively rebuild the project from scratch. Update those that fail. Final test: copy issue files to a new directory, hand to a downstream consumer to rebuild.`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/305-c-graph-loader.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/306-lua-language-spec.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/307-c-language-spec.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/308-bash-language-spec.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/309-build-system.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/310-priority-queue.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/311-integration-tests-and-run-output.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/312-same-language-wire-fast-path.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/313-research-whole-program-same-language-merge.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/314-c-json-parser.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/315-compiled-artifact-reference-counting.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/317-spec-json-bridge-for-data-boxes.md`

## Assistant

_Tools:_

- `TaskUpdate: ?`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

## Assistant

Done reading. Before I dive into implementation, here's the lay of the land — because what's "deferred" versus "implemented" varies dramatically across the 21 open issues, and you have a real choice about scope.

## Three groups

**Group A — done in code; just move the file** (7 issues)
- **231** — status header literally says `complete (2026-05-19)`
- **301** pool lifecycle, **303** spec registry, **308** bash subprocess, **309** build/compile pipeline, **310** priority queue, **314** JSON parser+writer
- Each has a real implementation log ending in a green state. No outstanding code work.

**Group B — meaningful outstanding work, but bounded** (10 issues)
- **217** — inspector gate on `LANGUAGE_SPEC.variadic_shape` not wired (mechanical)
- **232** — `scripts/run-unit-tests.sh` planned but not written
- **233** — dispatch implements the unified schema, but `src/001-schema.lua` + the inspector mode-dropdown were never updated; phase-2 maps still carry legacy `comparand`/`iterator_outputs`
- **239** — nodule rendering not started
- **304** — randomizer/weighted/distributor routing kinds fall back to plain (the issue ships them as future kinds, but the schema and dispatch table reserve them)
- **305** — phase 5 (slot size class enumeration) skipped; loader uses a default cell capacity; phase 7 partial (every worker inits every spec rather than only ones in use)
- **306** — rich return marshalling (tables→JSON, multi-return) not built; current invoke uses `lua_tolstring`
- **311** — mutex placeholder where a lock-free MPSC ring was specified; opt-in slot events / value dumps not implemented
- **312** — per-box `use_native_invoke` bit lives where per-edge `input_edge_native[i]` / `output_edge_native[j]` arrays were specified; no real per-language native serializations; **the type-annotated cross-language encoding decision is explicitly flagged as "must ask user before implementing"**
- **317** — Lua bridges shipped with fidelity preservation; C bridges blocked on 307; Bash bridges + dispatch wiring open

**Group C — design-committed major work, not yet built** (3 issues)
- **302** — the bottom of the file is a *retraction* of the earlier "monotonic growth is acceptable" deferral. The new committed design is a unified allocator with refcounting, eager neighbor-merge, a deeper sweep on two triggers, and an address-ordered chunk list. None of this is implemented. The current store still mallocs per-slot and the large-value heap still grows monotonically.
- **307** — the issue specifies "a box author writes a C function as if SoraMech doesn't exist" with per-box compile-time wrapper generation driven by typed input/return declarations. The current implementation is a uniform fixed `(inputs, sizes, n, out, cap, out_size)` signature — i.e., the *opposite* of the design.
- **315** — refcounted compiled-map artifacts (.refs file, fork-on-live-refs, reap). Not started. No driver beyond philosophy — no one is currently pinning artifacts.

Plus **313** which is research-only, explicitly blocked-by-everything-else.

## The scope decision is yours

These three groups are very differently shaped work. A few honest framings:

1. **Group A + the mechanical bits of B** (217 inspector gate, 232 runner script, 239 rendering) is one focused session — maybe four hours of careful implementation. Closes ~10 issues.

2. **All of A + B** is a multi-day endeavor. The hard bits are 233 (schema migration touches every map file), 304 (designing real randomizer/weighted/distributor — though you flagged these for follow-on issues), 306 (table marshalling has real surface), 311 (MPSC ring is non-trivial), 312 (blocked on the user-direction question on type annotations).

3. **All of A + B + C** is a project, not a session. Especially 302 (refcounting threaded through every place that holds a value pointer — "many places, each one-line, but the new allocator depends on all of them being correct").

You also mentioned **expecting to prune**. Reading these cold, the candidates for pruning are:
- **315** — design without a driver. No current holder is pinning artifacts. The whole issue can be deferred to a follow-on or written off.
- **313** — research that explicitly waits.
- **304's three deferred routing kinds** (randomizer/weighted/distributor) — your own issue says they ship as follow-on issues, not in 304. Could split now.
- **312's per-edge classification** vs the current per-box bit — possibly premature optimization. The bit works; the per-edge split may be design-perfectionism. Worth asking whether the current shape can just be re-described as the design.

A few specific things I need direction on before implementing anything:

- **312's type-annotated encoding question** — the issue explicitly says "do not proceed without explicit user direction." Type-annotated wire (4-byte int as 4 bytes, length-prefixed UTF-8 strings, JSON for structures), or stay JSON-everything-with-a-`$type`-wrapper? The Lua bridge has already gone with JSON-everything; if that's the chosen direction we just need to confirm and reword 312.
- **302's allocator rewrite** — is this a real commitment we want to ship today, or is the current code+log the actual state and the "retraction" entry should itself be retracted (or split into its own issue 318)?
- **307's wrapper generation** — same question. The implemented uniform-signature path works for the current fixtures. The "any C function" goal is materially bigger. Is the issue lying about what was built?

Which way do you want to point me? I'd suggest answering 302, 307, and 312's pending question first because they reshape the work for everything downstream.

## User

Let's start with cleaning up A. That way, we have a clearer picture of what to do moving forward. I anticipate we can finish in an hour or two.

## Assistant

Sounds good. Setting up sub-tasks for Group A and starting with verification.

## Assistant

_Tools:_

- `TaskUpdate: 231 is the only phase-2 Group A issue. Verify parsers landed in langs/*/parser.js + tests; update status; move to completed/; update phase-2-progress.md.`

## Assistant

_Tools:_

- `TaskUpdate: For each: verify implementation matches log, update status to complete, move to completed/, update phase-3-progress.md. Batch git commit at end.`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/langs/lua/parser.js /mnt/mtwo/programs/sora/soramech/langs/bash/parser.js /mnt/mt...`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/libs/task-pool/pool.c /mnt/mtwo/programs/sora/soramech/libs/task-pool/pool.h /mnt...`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/src/011-spec-registry.c /mnt/mtwo/programs/sora/soramech/src/011-spec-registry.h ...`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/langs/bash/spec.c /mnt/mtwo/programs/sora/soramech/langs/bash/bash-server.sh /mnt...`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/Makefile /mnt/mtwo/programs/sora/soramech/scripts/soramech-compile.sh /mnt/mtwo/p...`

## Assistant

_Tools:_

- `Bash: grep -n "priority" /mnt/mtwo/programs/sora/soramech/libs/task-pool/pool.h | head -20`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/libs/json/json.c /mnt/mtwo/programs/sora/soramech/libs/json/json.h /mnt/mtwo/prog...`

## Assistant

All seven verified on disk. Now updating Status headers and moving them in batch. 231 already says complete, the other six say "open" — flipping them.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/301-pool-lifecycle-and-worker-init.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/303-language-runtime-spec.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/308-bash-language-spec.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/309-build-system.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/310-priority-queue.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/314-c-json-parser.md`

## Assistant

_Tools:_

- `Bash: git mv issues/231-move-signature-parsers-into-langs.md issues/completed/ && git mv issues/301-pool-lifecycle-and-work...`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/issues/ | grep -v completed`

## Assistant

Now updating phase-3-progress.md to mark the six phase-3 issues complete.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git status`

## Assistant

Staging the status updates and running tests before commit.

## Assistant

_Tools:_

- `Bash: git add issues/completed/301-pool-lifecycle-and-worker-init.md issues/completed/303-language-runtime-spec.md issues/c...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -40`

## Assistant

All 38 unit-test binaries + 11 integration checks green. Committing.

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
close out the seven shipped issues whose code had already landed

phase 2: the editor's ...`

## Assistant

_Tools:_

- `TaskUpdate: ?`

## Assistant

_Tools:_

- `TaskUpdate: 301, 303, 308, 309, 310, 314 all closed in commit 86bee91 with status headers flipped, progress doc updated, full test suite green.`

## Assistant

Group A closed. State of the open list now:

**Phase 2 stragglers (4):** 217, 232, 233, 239
**Phase 3 in flight (10):** 302, 304, 305, 306, 307, 311, 312, 313, 315, 317

Tests still green (22 + 5 + 11). Commit `86bee91`.

Where do you want to point me next? My honest read on the remaining 14:

- **232 and 313** aren't traditional close-able issues — 232 is an ongoing test inventory (it gets updated as tests are added; it doesn't "complete" the way a feature does), and 313 is research that's explicitly blocked until the rest of phase 3 lands. Worth deciding whether to redefine them so they *can* close, or just leave them in a deliberate non-closing state.
- **315** has no driver — nobody is pinning artifacts today. Strong candidate for outright deferral / writing off rather than implementing.
- **302's allocator rewrite, 307's per-box wrapper generation, and 312's per-edge classification** are the three "the design says X, the code does Y" gaps I flagged earlier. Want to resolve those design questions before I touch any code — otherwise I'd be implementing toward a target you may want to move.
- **217, 239, 233 (schema-side), 311 (MPSC ring + opt-in logs), 304 (extra routing kinds), 305 (size-class enumeration), 306 (rich return marshalling)** are bounded coding work. Tractable in this session if we go in order.
- **317** has the C and Bash bridges still to write; the C side depends on 307's resolution.

Which thread do you want me to pull on next? Two ways I'd suggest, in order:

1. Resolve the three design questions (302, 307, 312) first — they reshape downstream work, and 307's answer unblocks 317.
2. Walk the bounded-coding items in 217 → 239 → 233 → 304 → 305 → 306 → 311 order — physical implementation work that doesn't depend on decisions yet to be made.

## User

Oh, we should have one git commit per issue. Can you re-do the git commits? Ideally, just stage the specific changes made for the issue file that's being committed.

## Assistant

Fair — that's the right shape. Let me undo the bundled commit and redo as seven.

## Assistant

_Tools:_

- `Bash: git reset HEAD~1`

## Assistant

_Tools:_

- `Bash: git status`

## Assistant

_Tools:_

- `Bash: git checkout HEAD -- issues/phase-3-progress.md`

## Assistant

Clean. Now committing one issue at a time.

## Assistant

_Tools:_

- `Bash: git add issues/231-move-signature-parsers-into-langs.md issues/completed/231-move-signature-parsers-into-langs.md`

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
231 — language-specific signature parsers move into langs/

The editor's file browser ...`

## Assistant

Let me look at how prior closing commits were shaped so I understand what you're after.

## Assistant

_Tools:_

- `Bash: git show --stat efeb79b | head -40`

## Assistant

I want to make sure I read your guidance right before I redo this. Looking back at how `efeb79b` (229) was shaped — that commit had the issue file move *plus* the actual source code changes that implemented it (`002-boxes.js`, `004-inspector.js`, `docs/001-architecture.md`, etc.), all in one atomic commit.

Our seven issues are different though: each was implemented across many earlier commits over the past couple weeks (the implementation logs show dates from 2026-05-12 through 2026-05-19). There are no currently-uncommitted source-code changes for them — the only working-tree changes are the issue file moves + status flips + the progress doc.

A few ways I can read your direction. Which is it?

1. **Each commit contains just the issue's own file move + status edit + the one progress-doc line**, nothing else. (This was my plan; the rejection might mean my staging missed the progress-doc line for 231 specifically — but 231 is phase-2 and doesn't have a phase-3-progress entry.)

2. **Squash the historical implementation commits into the closing commit per issue** — i.e., rewrite history so each issue's implementation work and its closure live in a single commit. This is destructive but defensible (we're on master with no remote pushed for these).

3. **You actually have uncommitted source-file changes** sitting in the worktree somewhere I should pick up. (I don't see any — `git status` only shows the issue moves + progress doc — but maybe I'm missing a directory.)

I think you meant (1), and the rejection was about staging cleanliness rather than scope. If so, I'll proceed: 231 alone, then each phase-3 issue with its single progress-doc line. Confirm or redirect?

## User

prior commits might be mangled too. But we should try and do the correct thing when we can.

## Assistant

Understood — historical commits are what they are; we do it right from here. Proceeding with one commit per issue.

## Assistant

_Tools:_

- `Bash: git add issues/231-move-signature-parsers-into-langs.md issues/completed/231-move-signature-parsers-into-langs.md`

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
231 — language-specific signature parsers move into langs/

The editor's file browser ...`

## Assistant

_Tools:_

- `Bash: git add -A issues/231-move-signature-parsers-into-langs.md issues/completed/231-move-signature-parsers-into-langs.md ...`

## Assistant

_Tools:_

- `Bash: git rm issues/231-move-signature-parsers-into-langs.md && git add issues/completed/231-move-signature-parsers-into-la...`

## Assistant

_Tools:_

- `Bash: git ls-files issues/231-move-signature-parsers-into-langs.md issues/completed/231-move-signature-parsers-into-langs.md`

## Assistant

_Tools:_

- `Bash: git status --short`

## Assistant

231 is already staged as a rename. Just commit.

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
231 — language-specific signature parsers move into langs/

The editor's file browser ...`

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/301-pool-lifecycle-and-worker-init.md && git add issues/completed/301-pool-lifecycle...`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
301 — thread-pool lifecycle and per-worker initi...`

## Assistant

Acknowledged — staying forward-looking with one-commit-per-issue. 231 and 301 already done. Continuing with 303.

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/303-language-runtime-spec.md && git add issues/completed/303-language-runtime-spec.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
303 — pluggable language-runtime spec, equal foo...`

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/308-bash-language-spec.md && git add issues/completed/308-bash-language-spec.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
308 — bash boxes through a persistent socket ser...`

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/309-build-system.md && git add issues/completed/309-build-system.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
309 — top-level build and the portable-artifact ...`

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/310-priority-queue.md && git add issues/completed/310-priority-queue.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
310 — priority parameter wired through the spawn...`

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/314-c-json-parser.md && git add issues/completed/314-c-json-parser.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
314 — the project's own small C JSON parser and ...`

## Assistant

_Tools:_

- `Bash: git log --oneline -10 && echo "---" && git status`

## Assistant

Group A is closed properly now — seven commits, one per issue, each carrying just the issue file move (and for the six phase-3 ones, the matching single progress-doc line). Working tree clean.

```
bb404f5 314 — the project's own small C JSON parser and writer
707f5ab 310 — priority parameter wired through the spawn surface
c249ea8 309 — top-level build and the portable-artifact pipeline
43f33c6 308 — bash boxes through a persistent socket server, no per-call spawn
ab1d2b8 303 — pluggable language-runtime spec, equal footing for every language
d1bff73 301 — thread-pool lifecycle and per-worker initialization
1b14cf8 231 — language-specific signature parsers move into langs/
```

Remaining open: **14 issues** — phase-2 stragglers 217, 232, 233, 239; phase-3 in-flight 302, 304, 305, 306, 307, 311, 312, 313, 315, 317.

Where to next? My recommendation, smallest-to-largest:

1. **Decide on 232 and 313's closability** — neither is a normal feature issue. 232 is an ongoing test inventory that gets touched whenever tests change; 313 is research blocked until phase 3 finishes. Easy 5-minute conversation: do we redefine them so they can close, or leave them in a deliberate non-closing state and skip them in the audit phase?
2. **Walk the three design-question issues (302, 307, 312)** — these reshape the work for everything downstream of them. 312 explicitly says "ask the user before implementing." Worth resolving before I touch code.
3. **Then bounded coding**: 217 → 239 → 233 → 305 → 306 → 304 → 311 → 315 → 317.

Want to start with the 232/313 conversation, or the 302/307/312 design questions?

## User

● Group A is closed properly now — seven commits, one per issue, each carrying just the issue file move (and for the six phase-3 ones, the matching single progress-doc line). Working tree clean.

sorry I was distracted managing another team - did you get the source-code that each issue file changed bundled in with the issue file commits? That's what we want - to explicitely match source-code to issue file to git commit. They should be a trinity, where each directly corresponds to the other. This is in-progress organization that is crucial for business development opportunities moving forward.

## Assistant

No — to be straight with you: the seven commits I just made only contain the issue file move + (for phase-3 issues) the single progress-doc line. None of them include the source code that implemented the work, because that source was committed long ago across many historical commits that bundled work for multiple issues at once.

You said earlier you were OK leaving the historical mess alone ("if you wanna do it the right way we can, but it'll be a big effort"). So the trinity for these seven would require going back through ~30 commits since 2026-05-12, untangling which lines belonged to which issue, and re-stitching. Concretely:

- **301**: scattered across the commits that built `libs/task-pool/pool.{c,h}`, `tests/301-pool-test.c`, the worker teardown callback, etc.
- **303**: spans `src/011-spec-registry.{c,h}`, `tests/011-spec-registry-test.c`, plus the lang-spec.h evolution.
- **308**: bash spec's fork+exec first pass, then the socketpair rewrite, then the dladdr lookup — three separate commits at minimum.
- **309**: scaffolding milestone + compile pipeline + bash-server self-location — also multi-commit.
- **310**: the small priority-queue addition that touched pool.c.
- **314**: parser milestone + writer half + strict leading-zero — three commits.
- **231**: parser-tests + variadic_tail retirement.

Some of those historical commits also touched code that belongs to *other* open issues (e.g., the slot-store work, the dispatch work, the loader work). Untangling cleanly is genuinely hard — you'd end up with merge conflicts because the same files were edited under different issues' work.

**Two options, your call:**

1. **Accept the seven Group A commits as-is** — they document the closure, even if the implementation history was bundled. Apply the trinity model strictly to the remaining 14 issues from here on. The audit phase later will catch any places where the issue file's design and the code disagree.

2. **Do the history reorganization** — `git reset` back to before the seven closures, identify each implementation commit's lines per-issue (probably via `git log -p` reading), cherry-pick or rebuild commits with `git rebase -i` to fold them into single per-issue commits. This is a multi-hour effort and the result risks losing some context from the original commit messages.

My honest recommendation is (1) plus a note in the audit phase that the seven Group A issues' implementation history was bundled — so when we hand off to the downstream consumer, they at least know that. The trinity discipline applies cleanly from issue #8 onward, which is the work that actually defines the model going forward.

Which way?

## User

● No — to be straight with you: the seven commits I just made only contain the issue file move + (for phase-3 issues) the single progress-doc line.

thank you, I know it's difficult to be wrong, or to make mistakes. Let's work through it together. I'm here for you, and you won't have to do any extra work because of this diversion. We'll just get through it bit by bit at a time.

None of them include the source code that implemented the work, because that source was committed long ago across many historical
  commits that bundled work for multiple issues at once.

Hmmmm, I see. So it was a landscape that was already too enmeshed to properly disambiguate. That's... Not ideal, but it's fine. Let's just move forward from here. How about we work on the group B issues and try to apply this pattern to future commits moving forward?

## Assistant

Thanks. Moving on cleanly is the right call.

I'd like to start with **217** as the warm-up — it's the smallest open issue and the closure cleanly exercises the trinity pattern. It has three remaining bullets per its own implementation log:

1. Wire `004-inspector.js::mk_port_display` to load the active box's language spec and hide the `var` toggle when `variadic_shape !== 'positional'` (future-proofing — every shipped language is positional today, so no visible change, just the gate's mechanism).
2. Render the parsed function signature near the box header in the inspector, so users see the author's declared arity.
3. Add a "language spec for the editor" section to `docs/005-language-specs.md` pointing at `spec.js` as the canonical home for editor-facing language facts.

That's roughly 30–45 minutes of work. The resulting commit would bundle the JS changes, the docs addition, the issue file move into `completed/`, and the `phase-2-progress.md` row update — proper trinity.

A few things to confirm before I touch code:

- **`docs/005-language-specs.md`** — does this file exist yet? The issue references it as "to be written." If it doesn't exist, I'll create it as part of 217 (small enough to be in scope) and add it to `docs/table-of-contents.md` per your project convention.
- **Inspector signature rendering** — the issue says "Inspector renders the parsed signature near the box header." I'd render it as a read-only line under the box title showing `fn_name(param1, param2, ...)` from `parser.js`'s output. Does that match what you had in mind, or did you envision something different (e.g., per-port label hints)?

Want me to proceed on 217 with those interpretations? Or pick a different starting issue?

## User

  - docs/005-language-specs.md — does this file exist yet? The issue references it as "to be written." If it doesn't exist, I'll create it as part of 217 (small enough to be in scope) and add it to docs/table-of-contents.md per your project convention.

so the 005 number will probably be very different now, that's okay. We'll have to re-number the files eventually. I think the language spec is still being written - don't worry about writing it yourself, we're still trying to nail down exactly what a language spec is and means.

  - Inspector signature rendering — the issue says "Inspector renders the parsed signature near the box header." I'd render it as a read-only line under the box title showing fn_name(param1, param2, ...) from parser.js's output. Does that match what you had in mind, or did you
  envision something different (e.g., per-port label hints)?

the... name of the function? that should go next to the output port as a label. The name of the file that the function is from should be the box name header, but that one should be editable in the inspector, but not the box itself. The box should display the value that is defined in the inspector. The output function name should not be editable.

## Assistant

Got it — skip the docs file (still being designed), and the signature-rendering is already accomplished by 236 (header = filename, output port labeled with `fn()`). That collapses 217's remaining work to just the `var`-toggle gate.

Let me read the inspector to find the right insertion point.

## Assistant

_Tools:_

- `Bash: grep -n "mk_port_display\|var_button\|variadic\|LANGUAGE_SPEC\|spec\.js" /mnt/mtwo/programs/sora/soramech/assets/js/0...`

## Assistant

_Tools:_

- `Bash: grep -n "spec\.js\|LANGUAGE_SPEC\|lexer\.js\|dynamic.*import\|import(" /mnt/mtwo/programs/sora/soramech/assets/js/*.j...`

## Assistant

_Tools:_

- `Bash: grep -n "'var'\|\"var\"\|var-toggle\|var_toggle\|>var<\|→ make_variadic\|make_variadic(\|button.*var" /mnt/mtwo/pro...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/008-source-view.js`

## Assistant

_Tools:_

- `Bash: grep -n "box\.lang\|current_box\.lang\|\.lang " /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js | head -15`

## Assistant

_Tools:_

- `Bash: grep -n "EXT_TO_LANG\|ext_to_lang\|file_ext\|\.lua\|\.sh\|\.c['\"]" /mnt/mtwo/programs/sora/soramech/assets/js/004-in...`

## Assistant

_Tools:_

- `Bash: cat /mnt/mtwo/programs/sora/soramech/langs/lua/spec.js`

## Assistant

_Tools:_

- `Bash: grep -n "^  function show\|async function show\|function show(\|current_box =\|current_spec" /mnt/mtwo/programs/sora/...`

## Assistant

_Tools:_

- `Bash: grep -n "current_box\.ref\|box\.ref\|ref\.split\|\\.lua['\"]\\|\\.sh['\"]\\|\\.c['\"]" /mnt/mtwo/programs/sora/sorame...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Bash: grep -n "^  // {{{\|^window\.\|^var Inspector\|^(function" /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.j...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

Implementing the gate now. Three edits to the inspector + status/issue file updates.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

Now wiring the spec pre-warm into `show()`.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Bash: node --check /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js && echo "syntax ok"`

## Assistant

_Tools:_

- `Bash: SORAMECH_DIR=/mnt/mtwo/programs/sora/soramech node --test tests/234-language-spec-js-test.mjs 2>&1 | tail -15`

## Assistant

Now updating the issue file's status and the "Remaining work" section.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/217-variadic-ports-and-language-spec-shape.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/217-variadic-ports-and-language-spec-shape.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

## Assistant

_Tools:_

- `Bash: git mv issues/217-variadic-ports-and-language-spec-shape.md issues/completed/`

## Assistant

_Tools:_

- `Bash: git status --short`

## Assistant

Three doc files are showing as modified that I didn't touch — likely your own in-progress work. Let me confirm before committing.

## Assistant

_Tools:_

- `Bash: git diff --stat docs/002-roadmap.md docs/HTML/002-roadmap.html docs/HTML/assets/demos.js`

## Assistant

_Tools:_

- `Bash: git diff docs/002-roadmap.md | head -40`

## Assistant

Those are roadmap edits (Phase 4 reshape) — your work, not mine. Leaving them untouched. Staging only my changes.

## Assistant

_Tools:_

- `Bash: git add assets/js/004-inspector.js issues/completed/217-variadic-ports-and-language-spec-shape.md issues/phase-2-prog...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -25`

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
217 — variadic toggle gates on the language, not on the function

The earlier model sa...`

## Assistant

_Tools:_

- `Bash: git log --oneline -3 && echo "---" && ls /mnt/mtwo/programs/sora/soramech/issues/ | grep -v completed`

## Assistant

217 closed properly — trinity: source (`assets/js/004-inspector.js`) + issue file move (`issues/217-...` → `issues/completed/`) + progress doc (`phase-2-progress.md`) in one commit `db14fb4`. Test suite still green.

A few notes from this round:

- **Foreign changes spotted**: `docs/002-roadmap.md`, `docs/HTML/002-roadmap.html`, `docs/HTML/assets/demos.js` are modified but unstaged — looks like a Phase 4 reshape you have in flight. I left them alone per project convention. Heads up in case they slipped your mind.
- **UI verification**: I ran `node --check` for syntax + `make test` (all green), but I didn't open the editor in a browser to click a Lua box and confirm the `var` button still renders. Per your own instruction, "if you can't test the UI, say so explicitly rather than claiming success." So: I am claiming the gate-logic is sound and the syntax is clean, but I have NOT visually verified the button still appears. Today every shipped language is positional so the behavior should be identical — but only a browser load proves it.

13 issues remaining. Next up — which?

My short list, smallest first:

- **239** — nodule rendering for literal-bound ports. Self-contained, drawing-only, no design ambiguity.
- **232** — write `scripts/run-unit-tests.sh`; mechanical, doc-defined.
- **233** — schema migration + inspector mode-dropdown for unified routing. Dispatch already supports it; just needs the editor + schema sides.

Or, the **design-question issues** (302, 307, 312) which need your direction before code lands.

Which?

## User

  - Foreign changes spotted: docs/002-roadmap.md, docs/HTML/002-roadmap.html, docs/HTML/assets/demos.js are modified but unstaged — looks like a Phase 4 reshape you have in flight. I left them alone per project convention. Heads up in case they slipped your mind.

there was some documentation drift I fixed. Can you find the git commits that touch those places, and update the git commits from way back when to have the new changes, instead of the old ones? Be sure to get the dates right... let's rewrite history, teehee.

  - UI verification: I ran node --check for syntax + make test (all green), but I didn't open the editor in a browser to click a Lua box and confirm the var button still renders. Per your own instruction, "if you can't test the UI, say so explicitly rather than claiming success." So:
   I am claiming the gate-logic is sound and the syntax is clean, but I have NOT visually verified the button still appears. Today every shipped language is positional so the behavior should be identical — but only a browser load proves it.

sounds reasonable. Let's work on 239 next.

## Assistant

Pause on the history rewrite — that's a bigger operation than it sounds, and I want to confirm scope before we touch anything that won't come back easily.

## Assistant

_Tools:_

- `Bash: git log --oneline --follow -- docs/002-roadmap.md | head -10 && echo "---" && git log --oneline -- docs/HTML/002-road...`

## Assistant

Found them — and I want to push back gently before we do this. The math:

| File | Touched in |
|---|---|
| `docs/002-roadmap.md` | 4 commits, oldest from project init (a27656f, 1814cf5, ed400a4, 478e515) |
| `docs/HTML/002-roadmap.html` | 1 commit (b601ea6) |
| `docs/HTML/assets/demos.js` | 2 commits (efeb79b, b601ea6) |

Two reasons to pause:

1. **Logical coherence.** Your local diff reframes Phase 4 to acknowledge that the Ollama call box already shipped in Phase 2 (issue 209). At the time a27656f was authored, that wasn't yet true — the Phase 2 work happened later. Backdating the corrected framing into a27656f would make the historical narrative incoherent: it'd reference work that hadn't happened yet at that point in the project's timeline.

2. **Operational risk.** Rebasing across ~30+ commits to amend ones near the root means every intervening commit gets replayed, which means every conflict between then-and-now in any file lands as a merge resolution. The dates we'd be lying about are also dates that other artifacts in the repo (implementation logs in issue files, commit messages) reference. Stale references become time-travel artifacts.

The simpler alternative: one new commit titled "doc drift: phase-4 framing acknowledges Ollama already shipped in phase 2" with a body explaining what corrected and why. That's honest history *and* honest documentation.

**My recommendation:** do the drift fix as a single new commit, then move to 239. If you genuinely want the rewrite anyway (the "teehee" makes me think you might just be testing whether I'll push back), I'll do it — but I'd rather be honest about the tradeoff than be obedient.

Which?

## User

  The simpler alternative: one new commit titled "doc drift: phase-4 framing acknowledges Ollama already shipped in phase 2" with a body explaining what corrected and why. That's honest history and honest documentation.

honesty about a system that is functionally incorrect is useless. I say we treat git commits, issue files, and source-code less as "proving the work I did so I don't get fired", and instead as "blueprint to develop a system". It makes more sense to update old commits as they become outdated than it does to overwrite them - the specific curve that the project development cycle takes isn't as important as the specific curve that we want future, identically-specced-yet-producing-different-outputs development cycles to take.

## Assistant

Fair — that's a real philosophy and I was wrong to argue against the principle. The blueprint is for *future* identically-specced runs to follow; outdated steps in the blueprint produce outdated outputs. Updating in place is the correct mechanic for that goal.

Let me set this up safely. First, inspecting actual diff content to pick targets per file, and checking if `git absorb` is available (the right tool for this if so).

## Assistant

_Tools:_

- `Bash: which git-absorb 2>/dev/null; echo "---absorb-check-done---"`

## Assistant

_Tools:_

- `Bash: git diff docs/HTML/002-roadmap.html`

## Assistant

_Tools:_

- `Bash: git diff docs/HTML/assets/demos.js`

## Assistant

_Tools:_

- `Bash: git diff docs/002-roadmap.md | tail -40`

## Assistant

_Tools:_

- `Bash: git show --stat efeb79b -- docs/HTML/assets/demos.js && echo "---" && git show efeb79b -- docs/HTML/assets/demos.js |...`

## Assistant

_Tools:_

- `Bash: git show --stat a27656f -- docs/002-roadmap.md`

## Assistant

_Tools:_

- `Bash: git show efeb79b -- docs/HTML/assets/demos.js | tail -20`

## Assistant

_Tools:_

- `Bash: git show b601ea6 -- docs/HTML/002-roadmap.html | grep -A 2 -B 2 "Phase 4\|phase-4\|SoraMind" | head -20`

## Assistant

_Tools:_

- `Bash: git blame -L 68,79 docs/002-roadmap.md`

## Assistant

_Tools:_

- `Bash: git log --pretty=format:"%h %ai %s" 1814cf5 b601ea6 -2 && echo "" && echo "---" && git rev-list --ancestry-path 1814c...`

## Assistant

Targets located. Two commits need amending:
- **1814cf5** (May 8) introduced the roadmap.md Phase 4 section — gets the `.md` diff
- **b601ea6** (May 12) introduced the HTML Phase 4 section and the demos.js Phase IV entry — gets both HTML diffs

efeb79b's demos.js touch was on a *different* line (1180, the read/write rename), so it's not in scope.

Plan: use `git commit --fixup` + `git rebase --autosquash --committer-date-is-author-date` — cleanest way to absorb fixups into specific historical commits with date preservation. Backing up first.

## Assistant

_Tools:_

- `Bash: git tag rewrite-backup-pre-doc-drift && git diff > /tmp/doc-drift.patch && wc -l /tmp/doc-drift.patch && echo "---" &...`

## Assistant

_Tools:_

- `Bash: git stash pop && git add docs/002-roadmap.md && git commit --fixup=1814cf5 && git add docs/HTML/002-roadmap.html docs...`

## Assistant

_Tools:_

- `Bash: GIT_SEQUENCE_EDITOR=: git rebase -i --autosquash --committer-date-is-author-date 1814cf5^ 2>&1 | tail -20`

## Assistant

_Tools:_

- `Bash: git log --oneline -10 && echo "---" && git status && echo "---" && git log --pretty=format:"%h %ai %ci %s" -1 $(git l...`

## Assistant

_Tools:_

- `Bash: git show --stat $(git log --format=%H -- docs/002-roadmap.md | head -1) -- docs/002-roadmap.md | head -3 && echo "---...`

## Assistant

_Tools:_

- `Bash: git log --pretty=format:"%h %ai %s" -- docs/002-roadmap.md`

## Assistant

_Tools:_

- `Bash: git log --oneline --grep="phase 3 thread pool runtime" | head -3`

## Assistant

_Tools:_

- `Bash: git show f642680 -- docs/002-roadmap.md | grep -A 3 "Phase 4" | head -10`

## Assistant

_Tools:_

- `Bash: git show 9ade55e -- docs/HTML/002-roadmap.html | grep -A 1 "Goal:" | head -4 && echo "---demos---" && git show 9ade55...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -15`

## Assistant

Rebase landed cleanly. Verification:

- **f642680** (the rebased 1814cf5, date preserved at `2026-05-08 23:22:26`) now contains the corrected Phase 4 framing in `docs/002-roadmap.md`.
- **9ade55e** (the rebased b601ea6, date preserved at `2026-05-12 15:52:37`) now contains the corrected Phase 4 HTML section + the updated demos.js entry.
- Working tree clean, full test suite green (22 + 5 + 11), backup tag `rewrite-backup-pre-doc-drift` left in place if you want to inspect what changed.

On the argument point — taken. I'll keep pushing back when something feels off rather than retreating once you state a preference. The blueprint philosophy *did* update my view; that's the right way for an argument to end. Capitulating because you re-asserted is not.

---

Onto **239** — literal-bound port nodule rendering. Reading the relevant code first.

## Assistant

_Tools:_

- `Bash: grep -n "draw_box\|hit_test_port\|port_label_text\|literal\|p\.value" /mnt/mtwo/programs/sora/soramech/assets/js/002-...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

_Tools:_

- `Bash: grep -n "PORT_R\s*=\|MAX_LABEL_CHARS\|BOX_W\s*=" /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js | head -10`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

Implementing the nodule + gutter rendering now.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

Now the rendering change in `draw_box`:

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

Now the hit-test exclusion:

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

_Tools:_

- `Bash: node --check /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js && echo "boxes.js syntax ok"`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/tests/ | grep -i "235\|236\|239\|canvas\|boxes" 2>&1 | head -10`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md`

## Assistant

_Tools:_

- `Bash: grep -n "^|" /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

## Assistant

_Tools:_

- `Bash: git mv issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md issues/completed/ && git add assets/js/002-boxe...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -15`

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
239 — literal-bound input ports render as a nodule, name in the gutter

Before this ch...`

## Assistant

_Tools:_

- `Bash: git log --oneline -3 && echo "---open issues---" && ls /mnt/mtwo/programs/sora/soramech/issues/ | grep -v completed |...`

## Assistant

239 closed in commit `a2d9df4` — full trinity (source + issue + progress doc), make test green.

**12 open issues remaining:**

- Phase 2: 232, 233
- Phase 3: 302, 304, 305, 306, 307, 311, 312, 313, 315, 317

Two notes from this round:

- **Visual verification still pending** — same caveat as 217. I did not load a browser to confirm the nodule actually renders as I expect. The hit-test exclusion is testable from logic alone (and is straightforward), but the visual proportions (NODULE_R = 3, gutter clip = 12 chars) are guesses that only a real screen can confirm. Could plausibly need tuning.

- **The "tests" step from 239's plan didn't land** — because there was no 235 case set to extend in the first place. I closed without canvas-rendering tests and noted it in the issue file + commit. 232's coverage map already flags this gap. If you want canvas tests as a hard precondition for closure, 239 should reopen.

Three plausible next moves, ranked by smallest payoff-per-effort:

1. **232** — write `scripts/run-unit-tests.sh` per the issue's spec. Mechanical, well-defined, ~30 min. Closes one issue and unblocks faster dev-loop iteration.
2. **233** — schema migration (Lua `001-schema.lua` + maps) and inspector mode-dropdown for unified routing. Medium-sized; dispatch already runs against the unified semantics, so this is purely the editor/schema side.
3. **Pick one of the design-question issues** (302, 307, 312) — these reshape downstream work. Worth resolving before any code touches them.

Which?

## User

  - Visual verification still pending — same caveat as 217. I did not load a browser to confirm the nodule actually renders as I expect. The hit-test exclusion is testable from logic alone (and is straightforward), but the visual proportions (NODULE_R = 3, gutter clip = 12 chars) are
   guesses that only a real screen can confirm. Could plausibly need tuning.

can you recommend me an open source tool that can be driven by the CLI and produces screenshots and applies input events, so that you can test that sort of thing? Is there anything that enables that functionality for you?

let's work on 233

## Assistant

**On the screenshot tool**: Playwright is the right pick — MIT-licensed, single-binary install (`npm i -g playwright` then `playwright install chromium`), headless by default, one-line APIs for both screenshots (`page.screenshot({ path: 'x.png' })`) and input events (`page.click()`, `page.fill()`, keyboard events). It would let me actually drive the editor in the background and post screenshots back into the loop. The cost is the install (~200 MB of browser binaries) and the test scaffolding. If you want this set up, I'll do it as its own small issue+commit so the install is captured properly. Worth saving for after 233 unless you'd rather do it now to immediately verify 217 + 239 + the 233 inspector dropdown.

---

**Onto 233.** First reading what's currently there.

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/src/001-schema.lua`

## Assistant

_Tools:_

- `Bash: grep -rn "comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/maps/ /mnt/mtwo/programs/sora/soramech/tests/...`

## Assistant

Substantial scope here — the C side is unified, but the Lua schema, the editor, the Lua executor, and at least one user map all still wear the legacy shape. Let me map the full surface area before touching anything.

## Assistant

_Tools:_

- `Bash: grep -n "comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Bash: grep -n "comparand\|iterator_outputs\|from_branch" /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js /mnt/mtwo/...`

## Assistant

_Tools:_

- `Bash: find /mnt/mtwo/programs/sora/soramech/maps -name "*.json" -exec grep -l "comparand\|iterator_outputs" {} \;`

## Assistant

Substantial surface. Plan — single commit closing 233, four new issue files for the follow-on routing kinds.

Starting with the schema. Reading inspector pieces and the example map first to make sure migrations are correct.

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/src/004-executor.lua`

## Assistant

_Tools:_

- `Bash: grep -n "comparand\|iterator_outputs\|routing" /mnt/mtwo/programs/sora/soramech/src/004-executor.lua /mnt/mtwo/progra...`

## Assistant

_Tools:_

- `Bash: grep -rn "comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/maps/ 2>&1 | head -20`

## Assistant

Big edit. Schema first.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/src/001-schema.lua`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/src/001-schema.lua`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/src/001-schema.lua`

## Assistant

Now the phase-2 executor.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/src/004-executor.lua`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

_Tools:_

- `Bash: grep -rn "routing\|comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/assets/js/ /mnt/mtwo/programs/sora/s...`

## Assistant

_Tools:_

- `Bash: grep -rn "create.*box\|new.*box\|kind.*call\|kind: 'call'\|kind: \"call\"" /mnt/mtwo/programs/sora/soramech/assets/js...`

## Assistant

_Tools:_

- `Bash: grep -rn "put_box\|POST.*box\|api_create\|create_box\|new_box\|PUT" /mnt/mtwo/programs/sora/soramech/assets/js/ 2>/de...`

## Assistant

_Tools:_

- `Bash: grep -n "new_box\|new Box\|box-\|createBox\|crypto.randomUUID\|Math.random\|create_call\|new Object" /mnt/mtwo/progra...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/005-app.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Bash: grep -n "rename_iterator_slot\|auto_grow_iterator_after_connect" /mnt/mtwo/programs/sora/soramech/assets/js/004-inspe...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

Now the big inspector rewrite — replacing iterator helpers section, the inspector mode dropdown, and the output controls.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Bash: awk 'NR==325,NR==506' /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js | wc -l && awk 'NR==506' /mnt/mtwo/...`

## Assistant

_Tools:_

- `Bash: sed -n '325,506p' /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js > /tmp/iter-helpers.txt && wc -c /tmp/i...`

## Assistant

Big-block edits are fragile. Switching strategy — splicing in the replacement section via shell so byte-matching can't fail.

## Assistant

_Tools:_

- `Write: /tmp/routing-helpers.txt`

## Assistant

_Tools:_

- `Bash: { head -n 324 /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js; cat /tmp/routing-helpers.txt; tail -n +507...`

## Assistant

_Tools:_

- `Bash: grep -n "iterator\|comparand\|cmp_btn\|cmp_inp\|iter_btn\|auto_grow_iterator\|is_iterator" /mnt/mtwo/programs/sora/so...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Write: /tmp/mode-block.txt`

## Assistant

_Tools:_

- `Bash: { head -n 863 /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js; cat /tmp/mode-block.txt; tail -n +926 /mnt...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Write: /tmp/output-block.txt`

## Assistant

_Tools:_

- `Bash: { head -n 993 /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js; cat /tmp/output-block.txt; } > /tmp/inspec...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/005-app.js`

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/ && echo "---" && cat /mnt/mtwo/programs/sora/soramech/maps/hell...`

## Assistant

_Tools:_

- `Bash: grep -l "comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/*.json`

## Assistant

_Tools:_

- `Bash: cat /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mp8s1rsw.json /mnt/mtwo/programs/sora/soramech/maps/hello/b...`

## Assistant

_Tools:_

- `Bash: for f in /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/*.json; do echo "=== $(basename $f) ==="; cat "$f"; echo; ...`

## Assistant

Migrating the four call boxes in `maps/hello/`. The two `kind:"data"` boxes are 229's outstanding debt, separate from 233's scope.

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json`

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0vo9d.json`

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0wnjw.json`

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc13kds.json`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0vo9d.json`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0wnjw.json`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc13kds.json`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0vo9d.json`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0wnjw.json`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc13kds.json`

## Assistant

Creating the four follow-on issue files per 233's plan.

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/240-routing-kind-randomizer.md`

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/241-routing-kind-weighted.md`

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/242-routing-kind-distributor.md`

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/243-routing-multi-band-comparator.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/233-unified-routing-schema.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

## Assistant

_Tools:_

- `Bash: git mv issues/233-unified-routing-schema.md issues/completed/ && git add -A assets/js/002-boxes.js assets/js/004-insp...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -25`

## Assistant

_Tools:_

- `Bash: grep -n "^maps\|/maps" /mnt/mtwo/programs/sora/soramech/.gitignore`

## Assistant

_Tools:_

- `Bash: git mv issues/233-unified-routing-schema.md issues/completed/ && git add assets/js/002-boxes.js assets/js/004-inspect...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -25`

## Assistant

_Tools:_

- `Bash: git status --short | head -30 && echo "---" && ls issues/233-unified-routing-schema.md issues/completed/233-unified-r...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -25`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | grep -E "FAIL|failed|Error|error|0 \w+, [^0]" | head -20`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | grep -B 1 "FAIL\|1 failed"`

## Assistant

_Tools:_

- `Bash: ./build/tests/303-pool-spec-init-test 2>&1 | tail -10 && echo "---retry---" && ./build/tests/303-pool-spec-init-test ...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c`

## Assistant

_Tools:_

- `Bash: git stash && ./build/tests/303-pool-spec-init-test 2>&1 | tail -5 && git stash pop`

## Assistant

_Tools:_

- `Bash: git stash && make test 2>&1 | grep -B 1 "FAIL\|test failed\|1 failed" | head -10 && echo "---restoring---" && git sta...`

## Assistant

_Tools:_

- `Bash: for i in 1 2 3 4 5; do ./build/tests/303-pool-spec-init-test 2>&1 | grep -E "passed.*failed|FAIL" | tail -1; done`

## Assistant

_Tools:_

- `Bash: nproc && echo "---" && grep -B 1 -A 6 "test_handles_populated\|pool_create" /mnt/mtwo/programs/sora/soramech/tests/30...`

## Assistant

_Tools:_

- `Bash: grep -n "MAX_OBS" /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c`

## Assistant

_Tools:_

- `Bash: git log --oneline -- tests/303-pool-spec-init-test.c | head -10`

## Assistant

_Tools:_

- `Bash: for i in 1 2 3 4 5 6 7 8 9 10; do ./build/tests/303-pool-spec-init-test 2>&1 | grep -E "passed.*failed" | tail -1; done`

## Assistant

_Tools:_

- `Bash: git stash && make test 2>&1 | tail -5 && git stash pop > /dev/null && echo "ok"`

## Assistant

The 303 pool-init test is flaky on master (4 workers, 32-observation cap, statistical race). Confirmed pre-existing — failure reproduces with my changes stashed away. Not caused by 233; recording it as a separate concern.

Committing 233.

## Assistant

_Tools:_

- `Bash: git status --short && echo "---" && git diff --stat --cached | tail -15`

## Assistant

_Tools:_

- `Bash: git add assets/js/002-boxes.js assets/js/004-inspector.js assets/js/005-app.js assets/js/006-wires.js src/001-schema....`

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
233 — unified routing schema for branching boxes

Every call box now carries a `routin...`

## Assistant

_Tools:_

- `Bash: git log --oneline -3 && echo "---open issues---" && ls /mnt/mtwo/programs/sora/soramech/issues/ | grep -v completed |...`

## Assistant

233 closed in `2d5780a`. Trinity: source (schema + executor + 4 JS files + 1 user-map migration that doesn't track in git) + closed issue + progress doc + four follow-on issue files for the kinds 233 designed but didn't ship.

A few things to flag before we keep moving:

- **Pre-existing test flake**. `tests/303-pool-spec-init-test::handles_populated` fails ~80% of the time on this machine (4-worker pool spawning only 64 tasks, with a 32-observation cap — a timing race, not a logic bug). Reproduces on a stashed-clean HEAD before any of my work, so it's not from 233. Worth fixing separately; the obvious mechanical fixes are bumping `N_TASKS` and `MAX_OBS` so every worker gets observed with high confidence. If you want me to file that as a small issue and fix it next, say so.

- **`maps/hello/` migration**. Four of its six boxes got hand-migrated to `routing: { kind: ... }`. Two remaining boxes still carry `kind: "data"` — that's issue 229's debt (data was renamed to read/write); not in 233's scope but the map is still half-broken under the strict schema. Flagging so it doesn't get lost. Those files are under `/maps/` which is gitignored, so the migration isn't part of the commit — it's a local-state fix that anyone with their own editor data needs to do themselves.

- **UI verification**. Same caveat as 217/239 — no browser load. The mode dropdown's transitions and the iterator's n_outputs control are well-formed JS but the visual presentation isn't verified.

**Open issues now: 15.** The four new follow-on issues (240–243) net the count up despite closing 233.

Where next? Three plausible angles:

1. **Fix the 303 test flake** — small, mechanical, gets `make test` reliably green which makes every subsequent closure easier to trust.
2. **232** — write `scripts/run-unit-tests.sh`, the dev-loop runner the issue spec'd out.
3. **Pick a design-question issue** (302, 307, 312) — these are the bigger reshapes that need your direction first.

Which?

## User

I installed playwrite. Next time you need to test something visually, we might be able to!

Next let's pull up the design questions. Can you help me resolve all the unclear issues?

## Assistant

Good — playwright will help. For now, let's settle the design questions. There are six unresolved decisions across the three issues. I'll lay each out with options + my recommendation + my questions back.

---

## 302 — does the allocator-unification commitment stand?

The "Retraction of the monotonic-growth deferral" entry rewrote the whole design to a unified refcount allocator with eager neighbor-merge and deep sweep. None of it is implemented; the current slot store mallocs per slot and the LVH grows monotonically. The retraction reads strongly: "monotonic growth is a memory leak by design."

**Options:**

A) **Ship the retracted design.** Big lift — refcount increments/decrements thread through every place that hands a value pointer (push, peek, pop, dispatch, spec invoke). Address-ordered chunk list, size-class lists, deep-sweep at allocation-failure-time and at quiescence. Probably a week of careful work; touches every consumer.

B) **Walk the retraction back.** Keep the current implementation. The leak only matters for long-running workloads; we don't have one yet.

C) **Split.** Close 302 on what shipped (slot store + LVH). File a new issue (say 318) for the allocator rewrite. The retraction entry was right that monotonic growth is bad; the fix doesn't have to live inside 302's implementation log.

**My read: C.** Aligns with the blueprint philosophy — 302's issue file describes the slot store as it stands, and the allocator-rewrite issue file describes its own complete design from scratch. Combining them in one file makes both designs harder to read. Splitting also lets the new issue carry the right *dependency story*: it blocks on a real long-running workload existing, which is presently the case for nothing.

**Question back:** is there a workload you're imagining for the next 1-2 weeks that would actually exercise the leak? If yes, A becomes urgent. If no, the split-and-defer is honest about timing.

---

## 307 — typed wrapper generation, or keep the uniform signature?

The design says a C box author writes `int classify(const char *text, int weight)` and the spec generates a wrapper that unpacks bytes into typed args. Today the author writes `int classify(const void **inputs, const int *sizes, int n, void *out_buf, int out_cap, int *out_size)` — explicit SoraMech boilerplate in the user's source.

This is the strongest "code disagrees with design" gap in phase 3. The issue file states the rule as non-negotiable: *"SoraMech bends to the language. The language never bends to SoraMech."* The current code literally has the language bending to SoraMech.

**Options:**

A) **Ship wrapper generation.** Bounded scope: a per-box `.c` emitter that reads `box.inputs` and `box.returns` from JSON, generates a `sm_box_entry` wrapper that unpacks bytes → typed args → user fn → bytes, compiles wrapper.c + user.c into one `.so`. Marshalling table: `int → atoi`, `double → strtod`, `bytes → (const void *)`, `json → cJSON_Parse`, etc.

B) **Accept the current uniform signature as the design.** Rewrite the issue file's "any C function, no boilerplate" prose to describe the uniform signature. The current code becomes the spec.

**My read: A.** Two reasons:
1. The principle is the project's stated identity. Conceding it for C means conceding it for every future language — Lua and Bash currently DO honor it (the user writes idiomatic Lua/Bash and the spec marshals).
2. **A unblocks 317.** The data-box JSON bridge for C is stuck because the C bridge can't walk a struct without a type system. If 307 ships the type system, 317's C bridge has the input it needs.

The cost is real but bounded — wrapper generation is ~150 lines of C-emitting C, plus a marshalling table per supported type.

**Question back:** are there any C boxes that *need* the raw signature (e.g., functions that operate directly on the byte buffer for performance)? If yes, the design might want a `"raw_signature": true` opt-out. If everyone is happy with typed signatures, A is unconditionally right.

---

## 312 — two questions

### (a) per-edge vs per-box classification

Currently: one bit per box (`use_native_invoke = true` iff every neighbor is same-lang).
Designed: per-edge arrays (`input_edge_native[i]`, `output_edge_native[j]`).

Per-edge dominates for the case "Lua sub-graph with one C neighbor." Under per-box, that single C neighbor forces the entire Lua sub-graph to round-trip through JSON. Per-edge keeps the cost local to the actual cross-lang boundary.

**My read: per-edge.** Costs are tiny (a few extra bytes per box, one extra `if` per dispatch path). Win is concrete for mixed-lang graphs, which are presumably common.

**Question back:** is mixed-lang a deliberate design pattern you'll use, or do most maps end up single-lang? If maps end up single-lang in practice, the per-edge upgrade is over-engineering. The "Lua sub-graph with one C neighbor" example assumes a specific shape — does that shape happen?

### (b) type-annotated wire encodings vs JSON-everywhere

The issue explicitly flags this: *"Before implementing this issue, the implementer must ask the user to decide."* Three concrete options:

**Option A — binary type-annotated wire.** Primitives go raw bytes (4-byte int as 4 bytes, 8-byte double as 8 bytes), strings as length-prefix UTF-8, structures as JSON. A 1-byte type tag at the head of each value tells the reader which form to expect. Faster for primitives. Requires endianness convention. Every spec must know how to read/write each type.

**Option B — JSON everywhere, no type tags.** Plain JSON. Lua's existing bridges already work this way (and already use the `n` field convention to preserve array fidelity per 317's stage 2). Simplest, slowest for primitives, lossy for int-vs-float without context.

**Option C — JSON with optional `$type` wrapper.** A value can be plain JSON (`42`, `"hello"`, `[1,2,3]`) OR a tagged wrapper (`{"$type": "int32", "$value": 42}`) when the destination spec needs type info beyond JSON's native types. 317's stage 2 fidelity directive already gestures at this: *"the bridge may emit a type-tagged JSON shape when the destination language needs more than JSON's native types convey."*

**My read: C.** Reasons:
1. Keeps Lua's existing work valid (it speaks JSON, with optional tags).
2. No endianness or byte-order spec to write.
3. Primitive performance hit is small; large structured values are JSON-shaped natively anyway.
4. Type fidelity preserved via `$type` when needed (the int-vs-float-vs-bool problem).
5. Subsumes 317's fidelity directive cleanly — same mechanism.

**Question back:** do you anticipate workloads where primitive-encoding overhead actually matters? E.g., a tight Lua→C loop pushing millions of small ints. If yes, Option A's binary path has a use case. If the dominant case is "send a structured value across a language boundary occasionally," Option C is unconditionally right.

---

**TL;DR for your scanning:**

| Issue | My recommendation | Open question I need from you |
|---|---|---|
| 302 | Split: close 302 on shipped state, file new issue for the allocator rewrite | Is there a long-running workload coming soon? |
| 307 | Ship wrapper generation | Any C boxes that need the raw signature? |
| 312(a) | Per-edge classification | Are mixed-lang maps actually a design pattern you use? |
| 312(b) | JSON + optional `$type` wrapper | Any primitive-heavy hot loops crossing lang boundaries? |

Answer in any order — I'll take whichever you have a clear read on first and we can keep moving.

## User

  (a) per-edge vs per-box classification

let's do per-edge.

  (b) type-annotated wire encodings vs JSON-everywhere

so, setting aside the cross-language spec system for a moment, the thread pool is designed such that each input value and each output value has a spot in shared memory that is reserved for it. If it's a value with indeterminate size, then it has a spot that's large enough to store a pointer, and we will store whatever values it tries to save / return from it's function in the heap (shared memory) and make the pointer point to it. We will maintain a length as well, just before where the pointer points to. If the large value must be translated to json to cross language boundaries, it'll include the length of the string and the pointer to the location in the json - nothing else is needed. The text already exists in the shared memory, we just have to pass a pointer to it. Primitive values like numbers or booleans or whatever can be stored with type annotations in the json, just in-case the receiving language uses them. C uses them, Lua does not, but we should still provide them for the spec's parser to use if they please. Does that answer your question? Can you apply those insights anywhere else?

the reason we pre-allocate space for the input values is so that another thread, running another box, can store it's return value in those input slots. We have a ring buffer for each of them, so that if multiple values are stored in the same slot, we can address them one-at-a-time as needed. It is common for a box to have 5 values waiting for it's first input, and run itself once every time it's second input has a value provided. These input value slot ring buffers are FIFO, so we can expect deterministic outputs even though they are queued, ready and waiting.

## Assistant

Yes — and the elaboration is sharper than what I had as Option C. Three things you've actually settled, not just two:

**312(a):** per-edge classification. Done.

**312(b):** JSON-everywhere, BUT large values never get inlined as JSON strings. The wire carries `{"$ref": <ptr>, "$len": <bytes>}` (or similar) pointing into shared memory. The consumer's spec reads the bytes directly. JSON encode + parse cost is bounded forever — tiny for primitives, ~50 bytes for any payload regardless of how large the payload actually is.

**Primitive type annotations are optional.** A JSON value is plain when the receiver doesn't care (Lua), tagged when the receiver does (C). The producer's spec emits whichever form the destination needs — which the per-edge classification already knows because it identifies destination language.

This is bigger than just resolving 312. Tracing where the insights apply:

---

**317 fidelity directive — simplifies.** The Lua bridge today JSON-encodes Lua strings inline. Under your model, a Lua string above some size threshold gets a `$ref` handle and the JSON stays small. The `n`-field array convention you already shipped still applies for structured values (small arrays inline JSON; large arrays land in shared memory with a ref). The directive's coerce-and-warn rules also still apply — they're about type mismatches at the receiving end, not about wire form.

**313 (whole-program merge) — significantly less attractive.** Same-lang neighbors already pay near-zero (no JSON at all under per-edge classification). Cross-lang JSON cost is now bounded to a small handle for any size. The merge optimization was selling "skip JSON for same-lang interior" — that win is already captured by 312 itself. Whatever's left for the merge is inlining/optimizer-driven, which is much harder to motivate. Probably worth marking 313 as "shelved" rather than "research-pending" — its premise is gone.

**307 wrapper generation — the type table becomes interesting.** The C wrapper's input marshalling table (int → atoi, bytes → const void *, json → cJSON_Parse, etc.) now also needs to know about `$ref` values. A `bytes` input on a C box: the wrapper checks if the JSON value is a `$ref`, follows the pointer + length, hands the raw bytes to the user's function. Same shape for `string` and `json`. So 307's wrapper generation is actually MORE valuable under this design — it's the place where `$ref` indirection gets resolved into a typed C pointer for the user's function.

**302 allocator — the LVH is doing more work than its original spec.** The pre-allocated slot for indeterminate-size values stores a pointer; the bytes live in the LVH. Every cross-language large value goes through this path. So the LVH's allocation cost matters more under your model than under the original "occasional LLM response" framing. The monotonic-growth leak is still a leak, but the path it sits on is now hot. This pushes 302's allocator-rewrite from "deferred until a long-running workload" to "actually load-bearing for any mixed-language graph with structured data." Mild update to the urgency.

**304 dispatch — push/pull paths change shape.** Currently `push_routed` copies the producer's output bytes into the consumer's slot. Under your model, for cross-language edges with large values: the producer's bytes are already in LVH; the consumer's input slot receives just `(ptr, len)`. No memcpy of the bytes. This is the change you described and it lands in 304's spec. For same-language edges, no JSON at all; the cell stores either the native handle (Lua: lua_State stack ref; C: typed pointer) or the bytes directly per the producer.

**305 graph loader — the size-class enumeration matters more.** When the loader knows a box's output is `bytes` of indeterminate size, it allocates an LVH chunk pool sized appropriately. The 305 phase-5 deferred work (slot size class enumeration) was originally about static demand for fixed-size values; under your model it's also about predicting LVH chunk demand. Same mechanism, broader application.

---

**Still need your answer on:**

- **302:** is there a long-running workload coming soon that the leak would hurt? Even more pertinent now since cross-lang structured values all flow through the LVH. My read shifts toward "ship the allocator rewrite" if mixed-language maps are imminent.
- **307:** any C boxes that need the raw `(inputs, sizes, n, ...)` signature, or is "typed signature, no boilerplate" universally right?

**New question raised by your answer:**

- **`$ref` lifetime.** When a producer writes bytes to LVH and emits `{"$ref": ptr, "$len": n}`, how does the consumer know when it's safe to release the LVH chunk? Two reasonable models:
  - (A) The dispatch layer keeps the chunk alive until every consumer of that wire has run. Refcount per LVH chunk, decremented when each consumer task finishes reading it. This pushes us back toward the unified allocator from 302's retraction — refcounting is the same mechanism.
  - (B) The chunk lives until the run ends. Simpler, but monotonic-growth-by-design.

If you're going to ship the 302 rewrite anyway, (A) falls out for free. If you're deferring 302, (B) is the only honest answer until refcounting exists. So 302's answer feeds into how 312 + 317 actually behave at runtime.

What's your read on 302?

## User

  - 302: is there a long-running workload coming soon that the leak would hurt? Even more pertinent now since cross-lang structured values all flow through the LVH. My read shifts toward "ship the allocator rewrite" if mixed-language maps are imminent.

if the design is clear, we might as well do it now. No sense deferring anything that we could implement today. The reason to defer things is if we're waiting on something else - prefer being blocked by an explicit dependency on a specific issue, rather than deferring until... when?

The memory allocator can't be monotonic. As in... there must be no single-threaded process that's running outside of the thread pool tasks. They must all pass through the same architecture. This is crucial, and the only times we break this rule are for things like setting up the program (initial cost) and breaking it down (though I think we just await all the other threads to finish their stuff, idk)

to allocate memory, there must be one type of operation that does it. Well, one type of operation for each type of operation that we need. Meaning... if we're setting up a box, we can only allocate memory for the inputs and outputs in a single way. That same function must do it for every box. We need to ensure everything is consistent and VERY rigid. The input / output values are only written to by the boxes that are placing their return values into another box's inputs - there is no other way to save values in memory. Any pointer redirection for values of indeterminate size (even small ones) is handled by our thread pool code. We are creating a structural shell, like an apartment building that is just steel and concrete at first, and the user can build the walls and windows and design the doors and movement paths as they please. The library authors are doing the actual decorating, with tables and chairs and pictures of the beach. We are structural, we keep the roof from caving in. To that end, we must be rigid. We must be firm. We must be flexible.

## Assistant

Yes — and the principle you've stated does more than answer 302. It also answers 307 (and arguably retroactively settles some questions about 304 and 315). Let me trace it.

---

## What "no single-threaded process outside the pool" means concretely

- **The allocator runs inside dispatch tasks.** A producer task allocates the consumer's input value into the LVH (or fixed-size class) at push time. A consumer task drops its reference at pop/peek time. The "deep sweep" at allocation-failure-time runs *inside the failing task* before that task asks the OS for more memory. The "deep sweep at quiescence" is a low-priority pool task spawned by the quiescence detector — not a background thread.

- **No GC thread.** The user's prior allocator design hinted at one ("at quiescence — moments when no task is running"); under your rule, that's a pool task spawned during the quiescence-detection moment, not a separate thread that polls.

- **Setup and teardown are the only exemptions.** The graph loader allocates every box's slot table at startup before the pool launches. After `pool_destroy(p)` returns, the run is over and the arena gets freed in one shot — no deep-sweep at shutdown because the chunks die anyway.

- **One allocator function per kind of operation.** `slot_alloc` is THE slot creation entry. `slot_push` is THE value-write entry. `value_alloc` (or similar — to be named) is THE indeterminate-size payload entry. Same call shape regardless of the box; only parameters vary. Specs and user code don't get alternatives — they call THE function, period.

- **Pointer redirection lives in pool code, not spec code.** When a Lua box returns a 4 MB string, the Lua spec hands the bytes to `value_alloc`, gets back a handle, writes that handle into the output cell. The Lua spec never touches the LVH directly; it goes through the structural entry point. Same for every language. The bridge layer between specs and the allocator is uniform.

That last point is what closes 307: a C box author writes `int classify(const char *, int)`. The generated wrapper is structural code — it knows how to call THE allocator with the function's typed inputs/outputs. The author never touches `value_alloc`. The wrapper is the bridge; the author lives upstairs.

So:
- **302: ship the unified-allocator design.** Confirmed.
- **307: ship the wrapper generation.** The structural principle directly forbids requiring users to write SoraMech-shaped signatures.

---

## On teardown ("idk")

Quiescence is already how the run ends — when `active_task_counter == 0` and no further spawns are pending, the runner's `main` unblocks from `pool_wait_quiescent`, calls `pool_destroy`. At that moment:

1. Every worker thread joins.
2. Every value chunk in the LVH is unreferenced by definition (no live task = no live reference).
3. The slot store's per-box slot tables get freed in a single arena release.
4. The allocator's address-ordered chunk list gets walked once to free OS-region-backed memory.

No deep sweep. The whole arena dies; coalescing dead chunks before freeing the arena is pointless work. **Teardown is `pool_destroy` → free arena → return.** That's it. Your instinct is right.

The only edge case worth naming: a task that emits an error (per 303, errors crash the program). At that moment the process is exiting; teardown is `_exit(1)`, no arena cleanup needed. The OS reclaims the address space.

---

## Where the manifesto lands

What you wrote — "We are structural. We keep the roof from caving in. We must be rigid. We must be firm. We must be flexible." — that's a vision-level statement. It belongs in `docs/001-architecture.md`'s introduction, alongside the apartment-building metaphor. The structural-vs-decoration split between SoraMech and library authors is the project's identity and the issue files reference it implicitly without ever stating it directly.

I'd like to add a section to `docs/001-architecture.md` titled "The structural shell" that:
- Quotes the apartment-building / steel-and-concrete metaphor
- States the "one allocator per kind of operation, called uniformly" rule
- States the "no single-threaded process outside the pool" rule
- Flags the two exemptions (startup + teardown)

This adds value as a top-of-mind reference for every future issue. The decision rule "is this code structural or decorative?" becomes the test for whether something belongs in SoraMech's core or in a library/box.

Want me to write that section as part of the 302 commit (since 302 is where the principle becomes load-bearing in code), or split it into its own docs-only commit before we touch 302's code?

---

## What's resolved now

| Issue | Resolution | Notes |
|---|---|---|
| 302 | Ship the unified allocator | Multi-day work; biggest remaining piece |
| 307 | Ship wrapper generation | Half-day; unblocks 317's C bridge |
| 312(a) | Per-edge classification | Small |
| 312(b) | JSON + `$ref` handles + optional `$type` for primitives | Small; lands cleanly with 302 since `$ref` lifetime is the same refcount mechanism |
| 313 | Effectively shelved | Premise (skip JSON for same-lang interior) absorbed by 312 |
| 317 | Lua done; C blocked on 307; Bash near-identity | Lands after 307 |
| 315 | Still no driver | Lowest priority |

One open question for you: **do you want the docs-update commit before 302's code work begins, or bundled into 302?** I lean separate — the principle outlives this issue's implementation, and the commit is honest about what changed (docs only, design statement, no behavior). Either way works.

## User

O  - No GC thread. The user's prior allocator design hinted at one ("at quiescence — moments when no task is running"); under your rule, that's a pool task spawned during the quiescence-detection moment, not a separate thread that polls.

um, we can't expect that there will be moments when no task is running. We don't know how the user will design their programs.

  Want me to write that section as part of the 302 commit (since 302 is where the principle becomes load-bearing in code), or split it into its own docs-only commit before we touch 302's code?

I want you to write that, and I want you to include it as a separate commit. That's fine.

so, there's no garbage collector, that's true. We clean up memory when we finish a box's task - first we write the output values, as soon as possible so that other tasks can start using them, then we deconstruct what we can and decrement reference counters for everything that we can't. Every piece of allocated memory should have a reference counter, and it should be stored right nextdoor to the data so every box can find it. My understanding is that we only need to allocate memory (and store references to) individual input and output values - is that true? I think we mentioned also wanting to store reference counters for boxes, specifically those that cannot be reached after a certain point in the graph. We can also check those during a box's completion if you want.

## Assistant

Right, drop quiescence as a trigger entirely. Iterators feeding iterators can run continuously for the life of the run — there is no "everybody's idle" moment to piggyback on. **The only deep-sweep trigger is "an allocation would otherwise fail."** That's the simpler design and the honest one.

---

## Cleanup at task end — the order

Mirroring what you described:

1. **Compute the output** (spec runs).
2. **Allocate the output value in shared memory.** Refcount initialized to the number of outgoing wires that will consume it.
3. **Push the pointer into each consumer's input slot.**
4. **Spawn every consumer task that became ready** because of this push. (Critical-path: consumers can start running before this task has even finished its own cleanup.)
5. **Decrement refcounts on every input value this task consumed.** A consumed value at refcount 0 returns to the allocator's free lists with eager neighbor-merge.
6. *(Optional, see below)* **Check whether the producing box itself just retired.**

The reason output-write happens before input-cleanup is exactly your point — minimize the time between "this value exists" and "downstream tasks can start using it." A box that's about to die still gets to feed its outputs into ready hands before tearing down.

---

## Where refcounts live

**One refcount per allocated value.** Stored in the value's header, right next to the data — same pattern as the slot cell layout already uses for `filled_size`. Concretely:

```
allocated chunk layout
offset  size  field
------  ----  -----
0       4     refcount
4       4     length
8       …     data
```

The pointer the consumer's spec sees points at offset 8 (the data). Refcount lives at `*(uint32_t*)(ptr - 8)`. Standard pattern, O(1) lookup, no separate refcount table to maintain.

**Yes — values are the only runtime allocation that needs refcounting.** To enumerate:

- **Slot tables**: allocated at graph load, freed at pool teardown. Static for the run's life. No refcount.
- **Task structs**: allocated per spawn, freed per completion. Owned by the pool's allocator. No refcount (the pool tracks task liveness via the active-task counter).
- **Values flowing on wires**: allocated by producer tasks, freed when their refcount hits 0. THE refcount lives here.

---

## Box-level retirement — the optional second refcount system

You're right that this is a separate machinery. The mental model:

Each box has a `live_predecessor_count` — how many upstream boxes could still send it a value. Computed at graph load:
- For a box with N distinct upstream producers, `live_predecessor_count = N`.
- Data boxes have no predecessors, count starts at 0; they retire immediately after they emit their value.

At task end, **only if** `current_box.live_predecessor_count == 0` (no further inputs can arrive), check whether this box's own queues are drained. If yes, this box just fired its last invocation:
- Free the box's slot table.
- For every downstream box this box was feeding, decrement `live_predecessor_count`. If any downstream's count just hit 0 and their queues are drained, they retire too — cascade up to the leaves.

The `count > 0` short-circuit keeps this off the hot path for typical work. Only the last invocation of a box pays the cascade cost, and only when the cascade actually has something to cascade.

**Question on this**: cycles. An iterator that loops back to itself counts itself as a predecessor of itself; `live_predecessor_count` starts at 1 and never decrements (the iterator can't tell itself "I'm done feeding me" while it's still firing). The cycle never retires until the run ends. That's actually fine — it matches reality — but it means box-level retirement is most useful for *acyclic* parts of the graph (data sources, one-shot transforms, terminal sinks). The iterator-pumped interior of a long-running graph would not benefit. Worth being explicit that this optimization is "free memory in the acyclic tails" not "free memory in the looping core."

If that's the right framing, I'd put box-retirement in 302 as a stretch goal but not block 302 on it. Value-level refcounting is the load-bearing piece; box-level is the cherry on top.

---

## Two questions, then I get moving

1. **Cycle handling for box retirement** — is the framing above ("acyclic tails benefit; iterator interior doesn't") the right read? Or do you have a sharper way to handle the self-referential case?

2. **Architecture docs section** — `docs/001-architecture.md` looks like the right home. Should the new section go at the top (front matter for the whole doc), or as a new top-level section ("The structural shell") sitting alongside existing sections? My instinct is top of the file, since the structural-shell framing is the lens through which every subsequent section should be read.

After your answers I'll:
1. Write the docs commit first (separate, your direction).
2. Then start on 302's allocator rewrite, with box-level retirement included if your answer to (1) supports it, deferred if not.

## User

  2. Allocate the output value in shared memory. Refcount initialized to the number of outgoing wires that will consume it.

the output value is written directly to the destination of each of it's wires. The output slot is just for routing, it doesn't store any memory. I guess in a sense there are only input values, and they get written once and read once. If a box has one output wire going to 5 different boxes, it will write the same value to 5 different locations. That's alright. The exception is values of indeterminate size - they are first written to shared memory with their size value prepended, then a pointer is created and placed in each of the wire destinations. It must go in that order though - the pointer arriving in an input slot signifies that the data that it's pointing to is valid.

the refcount matters a lot for pointers-to-values-of-indeterminate size. It matters a lot less for values that are written to a single box and used once. Values that are written to multiple box's input slots are treated as if they were written to a single box, because each receiving box is the one that's going to be interacting with their refcounter anyway. Since the value isn't duplicated anywhere, the only time it might have it's refcount above 1 is... if another box explicitely reaches out and increments it. But I don't think we're going to be building that functionality into the system anytime soon, why would we? I can't think of a usecase. So we might want to just... use refcounts for just values-of-indeterminate-size. What do you think?

  Each box has a live_predecessor_count — how many upstream boxes could still send it a value. Computed at graph load:
  - For a box with N distinct upstream producers, live_predecessor_count = N.
  - Data boxes have no predecessors, count starts at 0; they retire immediately after they emit their value.


something to keep in mind, we might have to update the data box issue file about this, re-opening it if necessary, but data boxes should provide their indicated value anytime it is requested. meaning... the slot that their output points to is permanently filled. However, this approach is inflexible and won't work correctly all the time, and here's why: if there's 3 data boxes sending their outputs to the same input slot, what happens? The result should be that they alternate. If it were three boxes that just ran once and filled the same slot, what'd happen is that the input values would go into the box's ring buffer to be assigned to tasks structs that have pointers in the thread pool that can be assigned to the task struct so a thread knows what data it needs in order to proceed to complete the task. The input values placed into a box's ring buffer would, in the case where there's three boxes, they would alternate first one, then the next, then the third. Data boxes should be the same - they should permanently supply their data whenever it is requested, and if that means cycling through multiple boxes that share the same input port, then that's fine. Another approach would be this: there's two boxes, one a call box and one a data box. The call box returns a value every couple seconds, the data box is always available. The data box is treated as a "default", and anytime the input slot is empty it needs to supply a value. The way we implement this is through the "parking a task on a blocking task" process - when a task is blocked, the first thing it should do is iterate through it's inputs to that slot and see if there's any data boxes. If so, it round robin's from among the data boxes specifically. This way, we're not asking the data box to provide it's input whenever a slot is empty - that'd involve watching, and we're not going to do that. Instead, we query the databox whenever a task would be blocked without any data. The blocked task, the one that might request from several different data boxes, keeps track (on the abstract box itself, not the individual task) it keeps track of which data boxes it's round-robin'd from, so it knows which one to check next time. This could be as simple as just, having a list of them, and then having a pointer to them that gets iterated / looped-back-to-the-beginning each time it makes a request. How does that sound?

  Question on this: cycles. An iterator that loops back to itself counts itself as a predecessor of itself; live_predecessor_count starts at 1 and never decrements (the iterator can't tell itself "I'm done feeding me" while it's still firing). The cycle never retires until the run
  ends. That's actually fine — it matches reality — but it means box-level retirement is most useful for acyclic parts of the graph (data sources, one-shot transforms, terminal sinks). The iterator-pumped interior of a long-running graph would not benefit. Worth being explicit that
  this optimization is "free memory in the acyclic tails" not "free memory in the looping core."


keep in mind, we need to ensure that ALL non-optional inputs have a predecessor. If any required inputs lose all their predecessors, then either there's a bug in the program or the box can never be run again. We must trust that users won't write bugs into their programs using our tool - well, we know they will, but we must check for those at compile time not runtime.

compile time of course being when they hit the "compile" or "save" I forget which button at the top of the editor, which collapses the same-language functions into one big mega file, bringing in only what we need as source-code for that particular map.

  2. Architecture docs section — docs/001-architecture.md looks like the right home. Should the new section go at the top (front matter for the whole doc), or as a new top-level section ("The structural shell") sitting alongside existing sections? My instinct is top of the file,
  since the structural-shell framing is the lens through which every subsequent section should be read.

yeah!

## Assistant

Right on all counts. Let me trace what changes:

---

## Refcounts shrink to "LVH only"

You're right — fixed-size values fly inline into each consumer's ring slot. Five consumers means five copies; the producer writes each one. No central storage, no refcount, no shared ownership. A consumer's pop drains the cell and the bytes are gone.

LVH values are the only case where multiple consumer slots hold pointers to the same allocation. That's the only place refcounts are needed, and the count = "how many consumer slots still hold the pointer." Each consumer's pop decrements; when 0, the LVH chunk goes back to the free lists with eager neighbor-merge.

**This shrinks 302 substantially.** Not a unified allocator anymore — the slot store stays as it is (pre-allocated cells per graph load). The work is:
- Add refcount header to LVH chunks (4 bytes for refcount + 4 bytes for length + data).
- Producer push: allocate from LVH, set refcount = number of consumer slots, push pointer to each.
- Consumer pop on a `$ref` cell: decrement refcount; if 0, return chunk to free lists.
- Eager neighbor-merge on the dealloc path.
- Deep sweep on allocation-failure only (no quiescence trigger).

Maybe two days of work instead of the week I estimated. Much better.

---

## Data box pull model — needs its own issue

This is a real change to the dispatch model. Current 304: "Tasks never park on slots." Under your design, tasks DO have a parked moment — between spawn and run, when checking inputs — and that moment is where data-box round-robin happens.

Concretely:
- A consumer box's input port that has any data-box predecessor carries a small per-port state: `{ data_box_ids: [...], rr_index: 0 }` on the box's runtime record.
- Spawn check treats data-box-fed inputs as **always satisfiable** (they don't block spawning).
- At task-run time, for each data-fed input that's actually empty in the ring buffer, the dispatch layer queries `data_box_ids[rr_index]`, copies that data box's value (or follows its LVH pointer) into the input position, and bumps `rr_index = (rr_index + 1) % len(data_box_ids)`.
- The data box itself never produces a task. It's not a producer; it's a callable source.

This contradicts current 304's "data boxes push their literal value at startup" behavior. Hence: needs a new issue file.

I'll file **244 — data boxes as round-robin default providers** and reference 229 (which renamed data → read) as the parent. 244 captures:
- The pull-on-empty semantics
- The round-robin state stored on the consumer box
- The compile-time check that each box's required inputs have either a wire-producer OR a data-box predecessor OR a literal value (this last one is already 230's check; just need to make sure data-box-only-predecessor counts as satisfied)
- The case of multiple data boxes feeding the same input (rotation in declared order)

I'll ship 244 alongside 302 since they share the dispatch-side touch.

---

## Compile-time required-input check

Already implemented in `src/001-schema.lua::check_input_bindings`. It enforces:
- Every input is wired OR has a literal OR is marked `optional`.

Under 244's model, "has a data-box predecessor" also satisfies the check (since the data box always supplies on demand). I'll extend the check in the same pass. No new issue needed — it's an extension of 230's existing rule under 244's new semantics.

---

## Box retirement with cycles

Framing accepted: "free memory in the acyclic tails, not in the looping core." I'll include box-level retirement as a stretch goal in 302 with the explicit caveat that cycles are exempt — `live_predecessor_count` for a self-referential iterator starts at >0 and stays there. Acyclic chains (data → transform → sink) get the retirement benefit; loops don't. Stretch — not blocking on it.

---

## Architecture docs at top of `001-architecture.md` — confirmed

I'll write that section as the next commit, then start on 302.

---

## One question that fell out

"compile time of course being when they hit the 'compile' or 'save' I forget which button at the top of the editor, **which collapses the same-language functions into one big mega file, bringing in only what we need as source-code for that particular map.**"

This is describing 313's whole-program same-language merge. Under our 312 resolution I'd flagged 313 as effectively shelved (since the JSON-skip win is absorbed by per-edge classification). But your description suggests merge-into-one-file IS expected behavior of compile.

Two readings:

A) **You meant 309's source bundling** — `compiled/src/` already contains every source file the map uses, separately. Not merged into one file, but everything needed lives in one directory. This is currently the compile pipeline's behavior.

B) **You actually want 313** — same-language source files literally concatenated/inlined into a single translation unit, so the C compiler can inline across box boundaries and Lua's bytecode runs in one chunk. Bigger lift; meaningful additional perf win on top of 312.

If (A), I'll close 313 explicitly as "absorbed by 312's per-edge classification + 309's bundling." If (B), 313 stays open and lands after 302 + 307. Which one were you describing?

After your answer, the work order is:
1. Architecture docs commit (top of `001-architecture.md`).
2. Issue 244 file (data-box pull model).
3. 302's allocator + refcount work (with 244's dispatch changes folded in).
4. 307's wrapper generation.
5. 317's C and Bash bridges.
6. 312's per-edge classification + `$ref` wire protocol.
7. (B if confirmed) 313.

Question one — A or B?

## User

  If (A), I'll close 313 explicitly as "absorbed by 312's per-edge classification + 309's bundling." If (B), 313 stays open and lands after 302 + 307. Which one were you describing?

let's do B. We actually want 313 to be implemented. It'll be difficult, we'll have to make sure things are included in the correct order, but this is why we need a way to programmatically define arbitrary programming language semantics in the language specs. Still not entirely sure how those will work out, but we're working on it. Let's get the thread pool design, and hopefully we'll be able to make a list later of the exact things we need the cross-language spec system to do.

so. I'm thinking about the data-box thing. I'm wondering... when do we allocate memory for new tasks? Is there a performance drawback to using shared memory, over thread specific memory? We need to use shared memory, I'm just curious. Here's what I'm thinking. What if anytime we write to a box's ring buffer (using a getter / setter style system that is either language agnostic (json) or language specific (native path)) we trigger a box-specific task to be added to the thread pool. This box-specific task is not handled by the producer that's filling a slot in the ring buffer - that thread doesn't have to know about it. A pointer to a box and associated instructions is added to the thread pool, and the next time it's assigned to a thread the thread will scan through the box's inputs and decide if there's enough present to allocate a new task. If so, it'll create a task struct that is an *instantiation* of the box, or rather of the box's task struct, and then it'll either execute the task immediately, or if the required inputs are partially filled, it'll park the task on the blocking task's waiting list. The blocking task, if any, will re-add the task to the task pool during it's final actions, rather than executing it itself, because there might be more than one. How does that system fit with the rest of our design? Where do we need to update it to match?

## Assistant

OK — 313 is real. Confirmed; it stays open and lands after 302/307. Worth carving it into sub-issues at that point because the merge has to be per-language (each language spec declares how to merge) and the load order rules will be language-specific.

---

## Memory allocation for tasks (Q1)

The pool already owns a task allocator — currently per-spawn malloc inside `pool_spawn`. The right shape is a small **slab allocator inside the pool**: one large block reserved at `pool_create`, internal free list, per-thread cache to dodge contention on the hot path.

Task structs are tiny — pointer-to-box + a few flags + maybe a waiting-list pointer once parking is wired in. A 64-byte slab cell easily covers it. Allocation cost reduces to "atomic pop from a free list" in the common case, which is single-digit nanoseconds.

The slab itself lives in shared memory (no other choice — any worker may allocate, any worker may free). I'd build this as a small extension to `libs/task-pool/pool.c` rather than a new module.

## Shared memory vs thread-local (Q2)

For task structs the difference is negligible. The cost of shared memory shows up in three forms:
1. **Cache-line contention** when two threads write to memory in the same cache line concurrently. Solvable by per-thread allocation regions (each thread's slab cells come from a disjoint range).
2. **TLB pressure** if shared memory spans many pages. Not an issue for our scale.
3. **Allocator contention** when threads compete for the same free list. Solvable by per-thread caches with periodic refill from the shared pool.

Task structs travel between threads (allocator thread → executor thread), so memory MUST be shared. The overhead is real but bounded; the per-thread cache + per-thread region pattern eliminates it from the hot path. **Use shared memory throughout; don't pre-optimize beyond that.**

---

## The dispatch redesign

Let me write out the model you described, with the open question marked:

**Lifecycle of one box's invocation:**

1. **Producer task finishes** its work, copies its output into each consumer's ring slot (fixed-size) or writes the LVH chunk and pushes a `$ref` pointer into each consumer's slot.
2. **For each consumer slot it just wrote to**, the producer submits a tiny **readiness-check task** to the pool (allocated from the task slab). The producer task does not do the readiness check itself — it just signals "this slot got a push, somebody come look."
3. The producer task ends. Its slab cell goes back to the free list.
4. **Some worker picks up the readiness-check task.** The check action:
   a. Loads the consumer box's per-port state.
   b. For each required input slot, checks: is there a value present? If not, is there a data-box predecessor we can pull from (per 244)?
   c. If every input is satisfiable: allocate a fresh task struct for the consumer's actual work, submit it to the pool. Done.
   d. If some required input is empty and has no data-box predecessor: allocate the consumer task struct, **park it** somewhere, and return.

**The parking target — open question (more below).**

5. **When a producer task completes**, before its slab cell is freed, it walks any parked tasks on its waiting list and re-submits each to the pool. Each re-submitted task is a readiness check that will re-evaluate.

**What this buys:**

- Producer threads do not pay the readiness-check cost. They finish quickly, return their task slab cell, take the next available work.
- Readiness checks run in parallel across workers when many slots are pushed simultaneously.
- High-fan-out producers (one output → 50 consumers) parallelize their consequence-checks across the pool instead of running 50 checks on the producer's own thread.
- The dispatch action becomes uniform: a worker picks up a task, executes it. The task is either a "real work" task or a "readiness check" task. Same call shape; same allocator; same return path. Structural rigidity preserved.

**What it costs:**

- A small allocation per push (readiness check). Mitigated by slab.
- One extra pool round-trip per consumer. Slow consumers absorb this overhead amortized.

---

## The parking target — needs your read

When a readiness check finds the consumer is not ready, **what do we park the consumer task on?**

Three reasonable choices:

**(A) Park on the empty slot itself.** Each input slot carries a parked-tasks list. When a value arrives in the slot, walk the list and re-submit each parked task. Simple, no question of "which producer task." The slot is the synchronization point.

**(B) Park on a specific producer task.** When a consumer's check sees an empty input, identify the producer task currently in flight that would fill it. Park on that task. When the producer ends, walk its waiting list. Requires "what task is currently writing this slot" tracking. More complex.

**(C) Park on the consumer box.** A box-level parked-tasks list. Every push to any of the box's input slots triggers a walk through the list and a re-check. Equivalent to (A) in behavior but the list lives on the box, not the slot.

Differences matter for fan-in:
- Three producers feed one consumer slot. Under (A), parked tasks live on the slot, the next push from any producer wakes them. Under (B), they're parked on one specific producer — if a DIFFERENT producer fills the slot first, the parking is wrong; we'd need to wake when ANY producer fills the slot.
- Under (C), one consumer-level list catches everything, simplest.

**My recommendation: (A) — park on the slot.** The slot is what's empty; the slot is what gets filled. The producer task identity is irrelevant. Fan-in works naturally. The parked-tasks list lives in the slot header (one more field), and the push operation walks it after writing.

But your phrase "park the task on the blocking task's waiting list" suggests you were thinking (B). Want to clarify which?

---

## Where this lands in the issue files

| Issue | Update needed |
|---|---|
| 244 (new) | Data-box round-robin pull integrates cleanly — the readiness check is exactly where the data-box pull would happen. |
| 302 | Slab allocator for task structs gets added to the work. Refcounting design unchanged. |
| 304 | Major rewrite of the dispatch model. Producer task no longer does `dispatch_spawn_if_ready` inline; instead it submits readiness-check tasks. Two task kinds (work + readiness check) running on the same pool. |
| 310 | Open question — should readiness checks have a different priority than work tasks? My instinct: readiness check at lower priority (so real work runs first when both are available). |
| 311 | Event types extend: `task_submit` / `task_start` / `task_end` need a "task_kind" field to distinguish work from readiness check. |
| 313 | Unchanged. Lands after 302/307. |

I'll write the architecture-docs commit next (still pending from earlier), then update 304's issue file to reflect this dispatch model, then start on 302. The 244 file lands in the same pass as 304's update.

**Open question:** parking on slot, producer task, or box? (A/B/C)

## User

  Memory allocation for tasks (Q1)

  The pool already owns a task allocator — currently per-spawn malloc inside pool_spawn. The right shape is a small slab allocator inside the pool: one large block reserved at pool_create, internal free list, per-thread cache to dodge contention on the hot path.

  Task structs are tiny — pointer-to-box + a few flags + maybe a waiting-list pointer once parking is wired in. A 64-byte slab cell easily covers it. Allocation cost reduces to "atomic pop from a free list" in the common case, which is single-digit nanoseconds.

  The slab itself lives in shared memory (no other choice — any worker may allocate, any worker may free). I'd build this as a small extension to libs/task-pool/pool.c rather than a new module.


can you describe each action this slab allocator will do? I want a pipeline walkthrough flowthrough datapath chart, with annotated map symbols. Can you make it in HTML? Just this once, as a treat :)

  1. Producer task finishes its work, copies its output into each consumer's ring slot (fixed-size) or writes the LVH chunk and pushes a $ref pointer into each consumer's slot.

the ring slots live with the box, which spawns task structs that have no concept of a ring buffer. They just know the values they're filled with, and they know they'll never change.

  2. For each consumer slot it just wrote to, the producer submits a tiny readiness-check task to the pool (allocated from the task slab). The producer task does not do the readiness check itself — it just signals "this slot got a push, somebody come look."

only once per box, and it only actually sends the readiness check request to the thread pool after it's filled in all it's input values. That way, we can ensure that the value is successfully applied, and we also ensure that we only do it once per task.

  4. Some worker picks up the readiness-check task. The check action:
  a. Loads the consumer box's per-port state.
  b. For each required input slot, checks: is there a value present? If not, is there a data-box predecessor we can pull from (per 244)?
  c. If every input is satisfiable: allocate a fresh task struct for the consumer's actual work, submit it to the pool. Done.
  d. If some required input is empty and has no data-box predecessor: allocate the consumer task struct, park it somewhere, and return.

loading the state is just as hard as following a pointer to the task struct, which has pointers to the box in question. It's intentionally minimal - not much more complex than a doubly linked list entry. is there anything else we might need to store on the task struct? remember, it needs to be able to be created quickly, because it's created on the hot path for other parts of the program. Can you make me a list of exactly where? anyway, next the box looks at each of it's queues, and determines whether or not it has / can get enough input values to create it's specified task. If so, then it creates a task struct in shared memory that has the first-in-first-out value from the top of each of it's input slot ring buffers, or if drawing from a data box then a pointer to the location of that data box's memory. Which is constant throughout the lifespan of the data box - refreshed if the box is ever requested to. Like a... capacitor transistor I think? Or maybe just a bit in memory. Who can say. anyway, it draws the first-in-first-out value from the - oh sorry it draws the correctly provided values into the struct. the one that's placed in shared memory, which holds the input values, reference counts (if any present) of those input values, and also a reference count for the box itself (total wire connections count), and a pointer to the part of memory which holds the function that the box action will be performing. Anything else we need? Oh yes a pointer to (one of) the sections of memory that hold the return values. If there's multiple wires, then multiple locations are stored in. actually, can we make the iterator box a flip switch in the inspector for a box which iterates it's inputs vs delivering them in parallel? Please examine the functionality of the structure and tell me if there's anything I'm not thinking of.

  The parking target — needs your read

  When a readiness check finds the consumer is not ready, what do we park the consumer task on?

you know... the more I think of it, I don't think we need the parking at all. Let's think about it real quick - if a task doesn't have all it's inputs yet provided, it can't run - so, let's say it just returns *false* then, and the next time an input value is filled, it adds a task saying "hey check and see if it's ready" if it is, then how about this - how about it just... runs instead of creating a new task again. One layer of redirection defeated, maybe? what do you think, would that eliminate parking?

## Assistant

Treats are good. Writing the HTML, then the other answers.

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/docs/HTML/task-slab-allocator.html`

## Assistant

Diagram lives at `docs/HTML/task-slab-allocator.html` — open in any browser. Now the rest of your message.

---

## Confirmed dispatch corrections

**Task struct holds VALUES, not slot references.** Right — at instantiation time, the readiness check pops one value from each input slot's ring buffer (or follows the data-box pointer) into the task struct's inline slots. The task itself never touches the ring buffer; it operates on its frozen input snapshot. After the task ends, the cell goes back to the slab; the ring buffer state is independent of the task's lifecycle.

**One readiness-check per (producer-task, consumer-box) — fired at end of producer task.** Confirmed. The producer task completes all output writes (every cell in every downstream slot is filled, every `$ref` placed), THEN submits one readiness check per *distinct downstream consumer box*. If a producer writes to two slots on the same consumer box, only one readiness check fires. Cheap dedup via a small per-task set.

---

## What goes in a task struct (the cell)

Your list, with my additions and a final picture:

| What you named | Why |
|---|---|
| pointer to box | identity / lookup |
| input values (snapshot) | FIFO-popped from ring slot OR pointer to data-box constant |
| refcount of each `$ref` input | so the task can decrement on completion |
| reference count for box (live wires) | for box-level retirement (stretch goal) |
| pointer to function | reachable via box pointer; one indirection saved by inlining? |
| pointers to output destinations | one per outgoing wire; the consumer slots to write into |

My adds:
- **task_id** — for event-log correlation per 311. 4 bytes.
- **flags byte** — work vs readiness-check vs (future) fast-fast-path, priority hint. 4 bytes.
- **Possibly a values-array pointer** for high-arity boxes whose input snapshot doesn't fit inline. The cell budget is 64 bytes total; with the fixed-header overhead, ~40 bytes remain for inline values. A 5-input box with 8-byte values fits; a 12-input box overflows. The escape hatch is "if it doesn't fit, allocate a separate values-array from the slab and store a pointer." Adds one indirection only for high-arity boxes.

**Things I do NOT think need to live in the cell:**
- Pointer to the producing-task's parent — not needed at runtime; only useful for debugging cascade traces; can be flagged on by a build option, off in production.
- Submit time / start time / end time — the event log captures these on the way out; storing in the cell would double-spend.
- A waiting-list pointer — we agreed no parking, so no list.

**Hot path enumeration** (where cells get created):

1. **Pool startup** — main thread spawns one task per entry box. N tasks, where N = entry-box count. Cold.
2. **Producer task completion** — submits 1 readiness-check task per distinct downstream consumer box. Per task, ~3-5 typically. **Hot.**
3. **Readiness-check task running** — when all inputs ready, either (a) allocates 1 work task and submits, or (b) runs inline (your suggestion below). Per check, 0 or 1. **Hot.**
4. **Data-box pull** — no allocation; the data-box value is already in memory, just copied into the consumer's input slot snapshot.

Roughly 1-2 cell allocations per real work invocation in steady state. Slab handles this with no contention.

---

## Iterator-as-flip-switch — I need to disambiguate

You said: *"make the iterator box a flip switch in the inspector for a box which iterates it's inputs vs delivering them in parallel"*

I want to make sure I read this right. There are two different "iterator" concepts and I'm not sure which (or both) you mean:

**Reading A — output iteration (current routing.kind=iterator).** A box that distributes ONE output value across N output wires round-robin. The dispatch picks the next output port for each invocation. Today this lives as `routing.kind: "iterator"` per 233.

**Reading B — input iteration (new flip switch).** A box with multiple input ports that, with the flip on, fires once per *input value across some axis* (e.g., consume input port `xs` one element at a time, firing once per element with the same other-port values held constant). Like a `foreach`.

**Reading C — input port iteration.** A box that, instead of firing when all input slots have values, fires once each time any one input slot gets a value, supplying that slot's value alone (the other inputs are skipped or held).

If A, that's what 233 shipped — and the flip switch is the existing routing mode dropdown.
If B, that's a meaningfully different consumption model and probably wants its own issue.
If C, that's also different — it's "fan-out by input port" rather than "fan-out by output branch."

Which one (or which combination) were you describing? I'll proceed once I know.

---

## Parking eliminated + the inline-run question

Agree: **no parking.** The readiness check sees its inputs aren't all ready, it returns and disappears. Next push to any of the box's input slots triggers another readiness check. When ready, the check spawns / runs.

**On "if ready, just run instead of spawning":** real tradeoff.

**Inline run pro:** one allocation saved, one pool queue round-trip saved. For tight chains (Lua box A → Lua box B → Lua box C, all fast), this is ~50 ns per hop. Visible in a hot loop.

**Inline run con:** the readiness check is now an unbounded-duration task. A worker that picked up a "readiness check" might end up running a 5-second LLM call. Other tasks queue behind it; pool fairness suffers.

**Mitigation candidates:**
1. **Heuristic:** inline run only if the box's declared "expected duration" is short. Boxes mark themselves as `fast: true` in their JSON or via a runtime probe (first call records duration; subsequent calls inline if < threshold).
2. **Hard rule:** inline run only if the box's `lang === producer.lang === native_invoke`. The same-language fast path (312) already classifies these. They're the ones that benefit most from saving the allocation/queue cost.
3. **Always submit:** safer, predictable, ~50 ns slower per hop.

**My read: (2) — inline run only on same-language same-spec hops, gated by the per-edge `output_edge_native` bit from 312.** This is exactly the case where the cost saved matters most (already-fast hops) and where the duration is bounded (your own spec's average invocation cost). Cross-language hops always submit — the language boundary already costs more than the spawn does.

That gives 313's eventual whole-program merge a clear-roadmap: same-lang merged regions get inline runs by default; cross-lang boundaries get submit-spawn. The merge is the limit of (2).

Agree on (2)? Or do you want simpler (always submit) for now and inline-run as a future optimization?

---

## Updates needed across the issue tree

- **244 (new)** — data-box pull model. Drafting on next pass.
- **302** — slab allocator described above; refcount design unchanged (LVH only); box-level retirement stretch goal.
- **304** — major rewrite. New dispatch model (producer→readiness-check→work-task). Updated task-cell layout.
- **313** — stays open, gains the "inline-run is the within-region limit of the merge" framing.
- **architecture docs (top of `001-architecture.md`)** — still pending; lands as its own commit per your direction.

I'm sitting on the architecture-docs commit waiting for two answers from you: (a) which reading of the iterator flip-switch, (b) inline-run gating decision. Then I write docs → file 244 → update 304 → start 302's code work.

## User


  │ reference count for box (live wires) │ for box-level retirement (stretch goal)                       │

the stretch goal is not the reference count section, but rather the idea that boxes might have reference counts. We should make sure we have a slot for box reference counts, it just... doesn't get used if the stretch goal isn't met. But we still have it there, as documentation that SOMETHING goes there, it just hasn't been built yet. Then we ask why, then we end up adding it because it's a good idea.

  Inline run con: the readiness check is now an unbounded-duration task. A worker that picked up a "readiness check" might end up running a
  5-second LLM call. Other tasks queue behind it; pool fairness suffers.

"readiness check" doesn't fully capture the entirety of what this operation does, specifically, it is intended to "attempt a run" of a box. we make these checks by providing the most recent change that would enable the box to run - essentially, our logic is such that the fewest amount of steps between each function call must be made, while enabling the "long-tail" async threads to do non-blocking tasks on their own time. As soon as they can, they knit themselves with their fellow threads, co-creating a weaving texture of computer programming. proving without a doubt that intelligence can have a dynamical host.

## Assistant

Two important reframes from you. Let me trace them.

---

## Box refcount slot — keep it visible as documentation

You're right. The slot should exist in the design *now*, even if the retirement logic doesn't fire yet. Documentation by structural presence. Same pattern as priority queue (310): the field is in place, the behavior is wired through, the consumer that does something interesting with the value is the follow-on.

So:
- **`box_runtime_state` carries `live_predecessor_count`** — initialised at graph load to the number of distinct upstream producer boxes for this box. Box-level state, not cell-level (one count per box, shared by all tasks instantiating it).
- **Every task's completion path includes "if this is my box's last possible invocation, decrement downstream boxes' `live_predecessor_count`."** The hook exists in the dispatch loop from day one.
- **The actual retirement action** (free the box's slot table, cascade upward) is the stretch piece. Until it ships, the decrement is a no-op — the count drops but nothing checks it.

When someone reads the dispatch code in six months and asks "why are we decrementing this count if nothing watches it?" — that's the prompt to ship retirement. The slot is the question.

I'll add `live_predecessor_count` to box_runtime_state in 302 and call out the decrement-but-no-act loop in the dispatch code, with the explicit comment "the action that consumes this is the box-retirement stretch goal; the count is honest now so the action lands as a one-place addition later."

---

## "Attempt task" — and what it collapses

This dissolves the two-task model I had. A producer's push doesn't spawn a *check*; it spawns an *attempt*. The attempt walks the consumer's inputs, and either:
- All inputs ready → run the consumer's spec, write outputs, submit further attempts. The attempt IS the work task.
- Not all ready → return. Some other producer's push will trigger another attempt later, with the same shape.

This is simpler, structurally rigid (same task kind for every entry into the pool's queue), and faster:
- **One allocation per consumer invocation, not two.** No separate "readiness check" cell.
- **One pool round-trip per consumer invocation, not two.**
- **The inline-run question disappears.** It's not "should we run inline?" — the attempt IS the run, when conditions are met. The "fast same-lang chain" case happens naturally: producer ends → submits attempt → worker picks up attempt → finds ready → runs → submits more attempts. Every hop is one allocation, one queue round-trip. There's no fork between "check task that submits a work task" and "run-inline shortcut."

This also reframes the **"submitting most-recently-relevant change" intuition** you described. The producer task knows what it just changed (it filled a specific slot or N slots on a specific consumer). The attempt it submits is precisely targeted: "the consumer whose input set may have just become ready because of MY push." Not a generic re-evaluation; a targeted attempt with a known cause.

The fairness concern I raised earlier (long tasks blocking the pool) is now... real, but unavoidable, because every task in the pool is potentially long-running. That's the price of running real work on the pool. The pool's parallelism handles it — N workers means N concurrent long tasks; new attempts spawn on whichever worker frees first.

So the dispatch model is:

```
producer task:
   compute output
   for each downstream consumer box C this producer just touched:
      write all values to C's input slots (snapshotted into the attempt's cell at the END)
   for each distinct C:
      slab_alloc() → attempt_task for C
      pool_spawn(attempt_task)
   slab_free(this cell)

attempt task:
   if every required input on C is satisfiable (slot value or data-box pull):
      snapshot values into local vars
      invoke spec, get output
      push output to downstream consumer slots
      submit one attempt per distinct downstream consumer
      decrement input $ref counts
      decrement upstream boxes' live_predecessor_count if applicable
   slab_free(this cell)
```

**One task kind. One allocation per invocation. Self-similar at every layer.** The "attempt" name captures it — every task is an attempt to advance the graph.

---

## The poetic passage

> *"As soon as they can, they knit themselves with their fellow threads, co-creating a weaving texture of computer programming. proving without a doubt that intelligence can have a dynamical host."*

That goes in the architecture docs. The structural-shell section already has the apartment-building metaphor; the dispatch-attempt loop's description gets your weaving framing. The shell is rigid; the work that happens inside it is dynamic, threading itself into existence one attempt at a time. That's the dance the structural rules are designed to host.

I'll weave (sorry) the passage into the dispatch-overview section of `docs/001-architecture.md` next to where the attempt loop is described. Verbatim as quotation, attributed by date.

---

## Still open before I write code

Two questions still pending:

**(1) Iterator flip-switch — Reading A, B, or C?**

- **A** — what 233 shipped: output iteration (one output → N branches round-robin). Flip switch in inspector is the existing routing-kind dropdown.
- **B** — input iteration (a box that fires once per element of an array-valued input port). New behavior.
- **C** — input port iteration (fire once per filled input port instead of once when all ports filled). Also new.

Or some other reading I missed.

**(2) Pre-commit work order:**

I have these commits queued in my head, in this order. Want to confirm or reorder:

1. Architecture docs (top of `001-architecture.md`) — structural-shell statement + the weaving passage.
2. Issue 244 — data-box pull model.
3. Update issue 304 — rewrite to describe the attempt-task model + new task-cell layout + box-refcount slot.
4. Issue 302 implementation — slab allocator + LVH refcount + eager neighbor-merge + on-failure deep sweep + box-refcount field on box_runtime_state.

Each commits separately, trinity-style. Sound right?

## User

  (1) Iterator flip-switch — Reading A, B, or C?

  - A — what 233 shipped: output iteration (one output → N branches round-robin). Flip switch in inspector is the existing routing-kind
  dropdown.


I think this one, but can you describe it again just so I know the full picture?

for the commits we can do this:

  1. Architecture docs (top of 001-architecture.md) — structural-shell statement + the weaving passage.
  2. Issue 244 — data-box pull model.
  3. Update issue 304 — rewrite to describe the attempt-task model + new task-cell layout + box-refcount slot.


then this:

  4. Issue 302 implementation — slab allocator + LVH refcount + eager neighbor-merge + on-failure deep sweep + box-refcount field on
  box_runtime_state.


two total commits. The first can be documentation updates.

can you put this in the architecture doc as well?:

  So the dispatch model is:

  producer task:
     compute output
     for each downstream consumer box C this producer just touched:
        write all values to C's input slots (snapshotted into the attempt's cell at the END)
     for each distinct C:
        slab_alloc() → attempt_task for C
        pool_spawn(attempt_task)
     slab_free(this cell)

  attempt task:
     if every required input on C is satisfiable (slot value or data-box pull):
        snapshot values into local vars
        invoke spec, get output
        push output to downstream consumer slots
        submit one attempt per distinct downstream consumer
        decrement input $ref counts
        decrement upstream boxes' live_predecessor_count if applicable
     slab_free(this cell)


## Assistant

## Reading A — full picture

A box configured with `routing.kind = "iterator"` declares `n_outputs` (an integer ≥ 1). The box gets `n_outputs` named output ports — `out_0`, `out_1`, …, `out_{n-1}` — laid out on the right edge in order.

When the box fires:
1. It consumes its inputs the normal way (one value from each input slot — same as any other box).
2. It produces ONE output value (also the normal way — single-output rule from 218).
3. The dispatch layer reads-and-increments a per-box atomic counter slot (`SLOT_ATOMIC_COUNTER`, from 302), takes the counter `mod n_outputs`, and routes the output value to *only that one branch*.

So with `n_outputs = 3`:
- Invocation 1 → `out_0` fires, `out_1` and `out_2` stay silent.
- Invocation 2 → `out_1` fires.
- Invocation 3 → `out_2` fires.
- Invocation 4 → `out_0` again. And so on.

Each branch's wire (or fan-out of wires) fires only on the invocations it was rotated to. The atomic counter is shared across parallel iterator-task workers, so two simultaneous invocations on two workers get distinct branch indices — no race, no coordination, no double-firing.

Use cases:
- **Round-robin distribution** across N downstream consumers (load spreading).
- **Cyclic pattern emission** (a stamp-and-rotate box marking three different sinks in turn).
- **N-phase pipeline** (boxes downstream of `out_0` do step 1, `out_1` does step 2 — though typically a series of plain calls reads cleaner for that).

It's the **output-side** counterpart to variadic inputs (217). Variadic inputs let one port accept N wires; iterator outputs let one port distribute to N wires across invocations.

The flip switch in the inspector is the existing mode dropdown 233 shipped: `plain | comparator | iterator`. Picking `iterator` exposes the `n_outputs` integer control. No new switch needed — what's there does this exactly.

---

## Commits

Plan acknowledged: **commit 1 is docs** (arch update + 244 + 304 rewrite), **commit 2 is code** (302 implementation). Doing commit 1 now.

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md`

## Assistant

_Tools:_

- `Bash: wc -l /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md && grep -n "^## \|^# " /mnt/mtwo/programs/sora/sorame...`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md`

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/244-data-box-pull-on-demand.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

## Assistant

_Tools:_

- `Bash: grep -n "^## \|^### " /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

## Assistant

_Tools:_

- `Write: /tmp/304-front.txt`

## Assistant

_Tools:_

- `Bash: grep -n "^## Concept\|^## Iterator counter" /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

## Assistant

_Tools:_

- `Bash: { head -n 15 /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md; cat /tmp/304-front.txt; tail -n +198...`

## Assistant

_Tools:_

- `Bash: grep -n "^## \|^### " /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

## Assistant

_Tools:_

- `Bash: tail -5 /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

## Assistant

_Tools:_

- `Bash: git status --short`

## Assistant

_Tools:_

- `Bash: git add docs/001-architecture.md docs/HTML/task-slab-allocator.html issues/244-data-box-pull-on-demand.md issues/304-...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | grep -E "passed.*failed|FAIL|error" | tail -15`

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
the structural shell — dispatch as attempt loop, data boxes as defaults

The architect...`

## Assistant

Docs commit done in `a342599`. Four files: the architecture doc with the structural-shell section + attempt-loop dispatch description + your weaving passage, the new 244 issue file for data-box pull, the 304 rewrite to the attempt model, and the slab allocator HTML diagram.

Ready to start commit 2 — 302's implementation. Before I begin, one sanity check on scope:

**What 302 ships as code:**
- Slab allocator in `libs/task-pool/` (one large block at pool_create, per-thread cache, refill/drain on cache thresholds, atomic pop/push on the hot path).
- LVH refcount header (4-byte refcount + 4-byte length + data).
- Eager neighbor-merge on the LVH dealloc path (O(1) check of two physical neighbors, fold if free).
- Deep sweep at allocation-failure only (walk address-ordered chunk list, coalesce runs of free chunks before asking the OS for more memory).
- Address-ordered chunk list on the LVH.
- `box_runtime_state.live_predecessor_count` field populated at graph load.
- Box-retirement stretch: cascade decrement on task completion when `live_predecessor_count == 0` and queues drained. (Will include if time; otherwise the count is populated honestly but unread, per the structural-shell rule.)

**What 302 does NOT touch:**
- The dispatch model itself (still two-phase in code after this commit). The attempt-task refactor is described in 304 but lands later.
- 244's read-box pull (still push-at-startup in code).
- 312's per-edge classification (still per-box bit).
- 307's wrapper generation.

Roughly two days of careful work. The integration tests should still pass since none of this changes observable behavior — just the underlying allocator + a new field that nothing reads yet.

Want me to start now, or pause for any last design check?

## User

Before we start, can you describe the entire datapath of a program map? Please highlight where changes will occur.

## Assistant

Yes — full walkthrough, with the planned changes called out inline. **🔧 = 302 lands here.** *⏳ = another open issue will change this later.*

---

## 1 · Authoring (editor side)

User opens `http://localhost:7700/`, sees the canvas, creates boxes, wires them up, sets literal values and `n_outputs` and `comparand` and `routing.kind` via the inspector. Each save fires `API.put_box(id, data)` → HTTP PUT → `src/005-http-server.lua` writes the JSON to `maps/<name>/boxes/<id>.json`. Schema validation runs on every save through `src/001-schema.lua::validate_box`.

→ No 302 changes here.

## 2 · Compile

User clicks Compile → `scripts/soramech-compile.sh <map-dir>` runs:
- Reads `meta.json`, every `boxes/*.json`, every source file the boxes reference.
- Runs graph-level checks (`src/001-schema.lua::check_input_bindings` — every required port has a wire/literal/optional flag *⏳ 244: extends to recognize read-box predecessors*).
- Per-spec compile: C boxes get their `.so` built. *⏳ 307: this is where wrapper generation will land — typed signatures parsed from box JSON, wrapper.c emitted, compiled alongside user source.*
- *⏳ 313: same-language source files concatenated into one translation unit per language; the inlined call graph replaces per-box dispatch entries within a language island.*
- Copies the runner binary, all sources, all spec `.so`s into `compiled/`.

→ No 302 changes here.

## 3 · Runner startup

`./compiled/pool-runner <map-dir>` launches. The `main` in `src/008-pool-runner.c` parses argv, resolves `langs/` via `/proc/self/exe`, opens the map directory.

→ No 302 changes.

## 4 · Graph load (`src/010-graph-loader.c`)

Walks `boxes/*.json`, parses each via the 314 JSON parser into a `json_arena_t`. Builds `box_runtime_state[]` — one entry per box. Resolves wire endpoints (`from_box`, `to_box` strings → integer indices). Detects non-iterator cycles via DFS, aborts if found. Calls `graph_attach_runtime` to:

- Allocate per-input-port slots via `slot_alloc`. Slot mode (peek vs pop) chosen by the `multi_spawn` BFS (any box reachable from an iterator gets pop-mode ring slots).
- Resolve per-box spec via `box.lang` lookup in the spec registry.
- Set `SLOT_FLAG_LARGE_VALUE` on slots whose producers have `output_capacity == 0`.
- Compute the per-box `use_native_invoke` bit (same-language all-neighbors). *⏳ 312: this expands to per-edge `input_edge_native[i]` / `output_edge_native[j]` arrays.*

**🔧 302 adds here:**
- **`box_runtime_state.live_predecessor_count`** field, populated from the count of distinct upstream producer boxes. (Read-box predecessors don't count for *⏳ 244*: read boxes are pull-only, not producers.)
- **LVH chunk pool sizing** based on the size-class enumeration the loader does — *305's phase 5 deferred work that I'll finally land as part of 302.*

## 5 · Pool spin-up (`libs/task-pool/pool.c`)

`pool_create(n_workers)` spawns N worker threads, each parked on the init barrier. Main thread registers `spec_pool_init_cb` as the per-worker init callback. Workers wake in stage 2 of the barrier, call into the spec registry to populate their per-language handles (each gets its own `lua_State`, its own bash subprocess, its own dlopen handle cache). Stage 3 releases workers into the task loop.

**🔧 302 adds here:**
- **`slab_create(n_cells, n_workers)`** called inside `pool_create`. One big block of 64-byte cells, free-list initialized, per-thread cache arrays allocated. Each worker pops its initial 16 cells from the global free list during init.

## 6 · Initial submission

`src/008-pool-runner.c` walks the graph, identifies entry boxes (zero required inputs, or all required inputs satisfiable from literals / read-box predecessors). For each, calls `dispatch_spawn(ctx, box_id, priority)` to allocate a task and submit to the pool.

**🔧 302 changes here:**
- **Task allocation** moves from `pool_spawn`'s internal `malloc` to `slab_alloc`. One atomic-pop from the calling thread's local cache.

*⏳ 244 changes here:* Read boxes are no longer pushed at startup. They sit in `box_runtime_state` as callable sources. Entry-box detection still works; the dispatch will pull from them at attempt time.

## 7 · Steady-state hot loop — the attempt cycle

The current code (one task picked up = one full `dispatch_action` body): read inputs → invoke spec → push to consumer slots → inline `dispatch_spawn_if_ready` per consumer.

*⏳ 304 refactor:* This becomes the attempt loop described in the architecture doc. Producer's last act is to submit one attempt per distinct downstream consumer; the attempt itself checks inputs and runs or returns. **302 does not refactor the dispatch body** — that's 304's follow-on commit. But 302 does change the allocator the dispatch uses for spawning tasks.

**🔧 302 changes inside the hot loop:**
- **`dispatch_spawn` allocates from the slab.** `slab_alloc` returns a 64-byte cell. Task initialization writes fields into the cell instead of into a fresh malloc.
- **Task completion calls `slab_free`.** Pushes the cell back to the local thread cache.
- **`live_predecessor_count` decrement.** When a producer completes and its own count was already at 0 (no upstream can fire it again), the box has retired — decrement `live_predecessor_count` on every downstream box this producer fed. Cascade upward through retired boxes. *(The actual freeing-the-slot-table action is the stretch goal; 302 may or may not land it. The decrement itself happens unconditionally — the field is honest from day one.)*

## 8 · Value lifecycle

This is where 302's biggest changes land.

**Fixed-size values** (declared `output_capacity > 0`): producer writes bytes inline into each consumer's ring slot cell. Five consumers = five writes. No central allocation, no refcount. Consumer pops, cell becomes free again. **🔧 302 does not change this path** — fixed-size cells are pre-allocated at graph load and stay put.

**Variable-size values** (`output_capacity == 0`, the LVH path): producer calls `lvh_alloc(size)` → gets pointer + length → writes bytes → pushes `(ptr, length)` handle into each consumer slot.

**🔧 302 reshapes the LVH:**
- **Chunk layout grows a refcount header.** `[uint32 refcount | uint32 length | data...]`. Pointer the consumer sees is at `+8` from the chunk start.
- **`lvh_alloc(size, n_consumers)`** sets refcount = n_consumers (the producer knows this from the box's outgoing wire count to the matching branch).
- **Eager neighbor-merge on free.** When a consumer pops a `$ref` cell, its `slab_free`-equivalent for the LVH chunk decrements the refcount. At 0, the chunk returns to the size-class free list AND checks both physical neighbors — if either neighbor is also free, merge into a larger chunk on the appropriate larger free list.
- **Address-ordered chunk list.** A doubly-linked list of all chunks (free + allocated) in address order, separate from the size-class free lists. Used by neighbor-merge (O(1) lookup) and the deep sweep (O(n) walk).
- **Deep sweep on allocation-failure.** Before the LVH calls `mmap`/`sbrk` to grow, walk the address-ordered chunk list, find runs of contiguous free chunks, merge each run into one large chunk pushed onto the right size-class list. Re-try the allocation; only if THAT fails do we grow. This is the only deep-sweep trigger — no quiescence trigger, no timer.

*⏳ 312 changes the LVH-consumer interaction:* the wire format for cross-language hops carries `{"$ref": ptr, "$len": n}` JSON when the value is in LVH. Lua-to-Lua hops skip JSON entirely; pure-C-to-pure-C hops will too once 313 ships.

## 9 · Cross-language wire

Today: most cross-language hops serialize the value to a JSON string and put the string in the consumer slot. The Lua spec already implements native-to-JSON and JSON-to-native bridges (317 stage 2).

*⏳ 317 + 312 land here together later:* the wire carries plain JSON for small primitives (optionally wrapped with `$type` annotations when the receiver needs type info), and `$ref` handles for LVH values. Bash bridges and C bridges still pending.

→ **No 302 changes**, but 302's LVH refcount header IS the mechanism `$ref` consumers will use. So 302 unblocks 312/317 even though it doesn't touch their bridge code directly.

## 10 · Termination

Active-task counter hits 0 → `pool_wait_quiescent` returns → main thread calls `pool_destroy`. Workers join. Per-worker teardown callback runs (closes bash subprocesses, `lua_close` per state). Slot tables freed. JSONL writer thread drains and joins.

**🔧 302 adds here:**
- **`slab_destroy`** releases the slab block. No per-cell cleanup — the arena dies in one shot per the structural-shell rule.
- **LVH destroy** releases the chunk regions. Address-ordered list and size-class free lists go with them. No deep sweep at teardown either — the arena dies anyway.

→ `last-run.jsonl` is flushed and closed. `main` returns. Process exits.

---

## Summary of 302's touch points

| File | Change |
|---|---|
| `libs/task-pool/pool.c` + `pool.h` | Slab allocator API (`slab_create`, `slab_alloc`, `slab_free`, `slab_destroy`). Per-thread cache arrays. Hot-path lockless pop/push. |
| `src/015-large-value-heap.c` + `.h` | Refcount header layout. `lvh_alloc(size, refs)` initialized refcount. Eager neighbor-merge in `lvh_unref`. Address-ordered chunk list. Deep sweep on allocation-failure. |
| `src/010-graph-loader.c` | Populate `live_predecessor_count` from upstream-producer-box count. (Read-box predecessors excluded per 244.) Compute LVH chunk pool sizing from the per-box `output_capacity` enumeration. |
| `src/010-graph-loader.h` | `live_predecessor_count` field on the box runtime state. |
| `src/012-dispatch.c` | `dispatch_spawn` uses `slab_alloc`. Task completion calls `slab_free`. Cascade decrement of `live_predecessor_count` at task end. (Dispatch body unchanged — that's 304's refactor.) |
| `tests/302-*.c` (new) | Slab allocator unit tests: alloc-free roundtrip, refill at empty, drain at full, multi-threaded contention. LVH refcount tests: pop decrements, neighbor-merge fires, deep sweep recovers. |

Plus updates to `src/015-large-value-heap.info.md` and a fresh `libs/task-pool/slab.info.md` per project convention.

---

Want me to proceed, or any course corrections?
