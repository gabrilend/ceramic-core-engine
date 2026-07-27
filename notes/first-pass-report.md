# First-pass report — lessons learned while building

This document is the deliverable of the first pass over minimal-soramech.
The build proceeded issue by issue, in order, and every pain point,
contradiction between documents, and gap the design left unstated is
recorded here as it was met. The second pass reads this before touching
anything.

Entries are append-only, grouped by the phase that surfaced them. Each
entry says what was expected, what was found, and what the second pass
should do differently.

## Found before building anything

**The launcher's hard-coded root pointed at a machine that isn't this
one.** The `run-demo` script carried a project root of
`/home/ritz/programs/sora/minimal-soramech`; the project actually lives at
`/mnt/mtwo/programming/ai-playground/minimal-soramech`. The script's
override-by-argument saved it from being broken, but a hard-coded default
that is wrong is a fallback in disguise. Lesson: the hard-coded `${DIR}`
convention needs a stated rule for what happens when a project moves —
probably a check that refuses loudly when the default does not exist,
rather than quietly relying on the caller to pass the right thing.

**The vision file asks for its own edit, and the table of contents seals
it.** The vision's final paragraph says "please inform the user then
delete this paragraph when we figure it out"; document 000 says the
vision is sealed and never edited. Both instructions cannot be followed.
First pass resolution: the paragraph's question (input-less boxes) was
answered by the seed sweep and gatherers, the user is informed here, and
the paragraph stays — the seal wins because history outranks tidiness.
The second pass should decide the precedence rule explicitly instead of
leaving two documents in quiet disagreement.

**The docs and the global project conventions disagree about where
indexes come from.** The docs are numbered 000–010 but no
file-index-counter existed to record it. One was created holding 010
before any source file was named. Lesson: the counter must be created
the moment the first indexed file is, or two files will eventually claim
one number.

## Phase 1 — the pool

(appended as built)

## Phase 2 — stations and the push path

(appended as built)

## Phase 3 — the build path

(appended as built)

## Phase 4 — the pull path and configuration

(appended as built)

## Phase 5 — routing kinds

(appended as built)

## Phase 6 — the map file

(appended as built)

## Phase 7 — seeing inside it

(appended as built)

## Cross-cutting lessons

(appended at the end of the pass)
