# 000 — Table of contents

Minimal SoraMech. An engine for building programs out of small C
functions wired together, where the wiring decides what runs and when.

## The tree

```
minimal-soramech/
├── vision                          the original note. sealed — not edited.
├── docs/
│   ├── 000-table-of-contents.md    this file
│   ├── 001-overview.md             what the machine is, in one page
│   ├── 002-stations-and-slots.md   the things that persist
│   ├── 003-datapath-delivery.md    the push path — the core of the engine
│   ├── 004-datapath-gather.md      the pull path
│   ├── 005-routing.md              plain, comparator, iterator
│   ├── 006-datapath-scheduling.md  the pool, sleeping, termination
│   ├── 007-datapath-build.md       C source to shims and registry
│   ├── 008-map-file-format.md      what a map file says
│   ├── 009-datapath-load.md        map file to running program
│   ├── 010-roadmap.md              the phases
│   └── HTML/                       the generated site: every document,
│                                   issue, and interface, cross-linked
│                                   (make html; start at index.html)
├── notes/                          thinking that predates the docs
│   └── first-pass-report.md        lessons, contradictions, and gaps
│                                   from building it all once
├── src/                            the engine
├── libs/                           the pool, and anything reusable
├── issues/                         one file per unit of work
│   └── completed/
│       └── demos/                  one runnable demo per finished phase
├── tests/
├── scripts/
├── strategems/                     data-flow patterns that keep proving useful
├── input/                          what goes into the box
├── output/                         what comes back out
├── desire/                         what should be better
├── faith/                          expectation of boons and blessings
└── tmp/ -> /tmp/minimal-soramech   RAM. tmp/shared-memory -> /dev/shm/
```

## Reading order

The documents are numbered to be read straight through, and they build
on each other.

If you only read one, read **[001 — Overview](001-overview.md)**.

If you want to understand how it actually works, read
**[002 — Stations and slots](002-stations-and-slots.md)** and then
**[003 — Delivery](003-datapath-delivery.md)**. Everything else is
either a variation on that path or the machinery that gets a map into
memory so that path can start.

## By question

| If you want to know | Read |
|---|---|
| What is this and why | [001 — Overview](001-overview.md) |
| What a station is made of, field by field | [002 — Stations and slots](002-stations-and-slots.md) |
| How a value gets from one box to the next | [003 — Delivery](003-datapath-delivery.md) |
| How a value can be fresh at the moment it is used | [004 — Gathering](004-datapath-gather.md) |
| How a value chooses between several destinations | [005 — Routing](005-routing.md) |
| How threads pick up work, sleep, and stop | [006 — Scheduling](006-datapath-scheduling.md) |
| How C functions become callable by name | [007 — The build path](007-datapath-build.md) |
| How to write a map | [008 — Map file format](008-map-file-format.md) |
| What happens between the file and the first task | [009 — Loading](009-datapath-load.md) |
| What order to build it in | [010 — Roadmap](010-roadmap.md) |

## The phases

Clusters of functionality, ordered by dependency rather than by
schedule. Detail in [010 — Roadmap](010-roadmap.md).

| Phase | What it covers |
|---|---|
| 1 | **The pool** — workers, the task queue, sleeping, termination |
| 2 | **Stations and the push path** — the first phase where a graph runs |
| 3 | **The build path** — the generator, shims, the registry |
| 4 | **The pull path and configuration** — gatherers, statics |
| 5 | **Routing kinds** — comparators and iterators |
| 6 | **The map file** — parser, loader, validation, seed. The capstone. |
| 7 | **Seeing inside it** — diagnostics, runtime editing, the HTML docs |

## Notes

**`vision`**, in the project root, is the original design note, written
before any of this existed. It sits at the root rather than in `notes/`
because moving it would mean touching it, and it is sealed. It is sealed. Several things in it are now wrong — it
still describes hooks for input-less boxes, blocking reads and writes,
a dedicated thread that fills inputs, and a comparator whose threshold
comes from inside the box. All of those were resolved differently and
the resolutions are in the documents above. The note is kept unedited
anyway, including a sentence that breaks off mid-word, because it is a
record of the moment the design got unstuck.

Where the note and these documents disagree, these documents are
correct.
