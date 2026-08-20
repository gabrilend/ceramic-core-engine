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
│   ├── 002-stations-and-ports.md   the things that persist
│   ├── 003-datapath-delivery.md    the push path — the core of the engine
│   ├── 004-datapath-statics.md      statics, and writes as events
│   ├── 005-routing.md              plain, comparator, iterator
│   ├── 006-datapath-scheduling.md  the pool, sleeping, termination
│   ├── 007-datapath-build.md       C source to shims and registry
│   ├── 008-map-file-format.md      what a map file says
│   ├── 009-datapath-load.md        map file to running program
│   ├── 010-roadmap.md              the phases
│   ├── 058-guarantees.md          what is always true, and its price
│   ├── implementation-notes/       decisions where more than one answer
│   │   │                           was defensible — what the options
│   │   │                           were, and what each one costs
│   │   ├── README.md               what belongs here, and the rules
│   │   ├── 056-no-pull-path.md    why there is no pull path
│   │   └── 057-packaging.md        handing the engine to someone else
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
**[002 — Stations and ports](002-stations-and-ports.md)** and then
**[003 — Delivery](003-datapath-delivery.md)**. Everything else is
either a variation on that path or the machinery that gets a map into
memory so that path can start.

## By question

| If you want to know | Read |
|---|---|
| What is this and why | [001 — Overview](001-overview.md) |
| What a station is made of, field by field | [002 — Stations and ports](002-stations-and-ports.md) |
| How a value gets from one box to the next | [003 — Delivery](003-datapath-delivery.md) |
| How a value sits still while others stream past it | [004 — Statics and recalculation](004-datapath-statics.md) |
| How a value chooses between several destinations | [005 — Routing](005-routing.md) |
| How threads pick up work, sleep, and stop | [006 — Scheduling](006-datapath-scheduling.md) |
| How C functions become callable by name | [007 — The build path](007-datapath-build.md) |
| How to write a map | [008 — Map file format](008-map-file-format.md) |
| What happens between the file and the first task | [009 — Loading](009-datapath-load.md) |
| What order to build it in | [010 — Roadmap](010-roadmap.md) |
| What you may rely on without measuring | [058 — Guarantees](058-guarantees.md) |
| Why nothing is ever pulled, and what that cost | [056 — Why there is no pull path](implementation-notes/056-no-pull-path.md) |
| What it would take to use this from another project | [057 — Packaging](implementation-notes/057-packaging.md) |
| Where the design had a real choice, and what it cost | [Implementation notes](implementation-notes/README.md) |

## The phases

Clusters of functionality, ordered by dependency rather than by
schedule. Detail in [010 — Roadmap](010-roadmap.md).

| Phase | What it covers |
|---|---|
| 1 | **The pool** — workers, the task queue, sleeping, termination |
| 2 | **Stations and the push path** — the first phase where a graph runs |
| 3 | **The build path** — the generator, shims, the registry |
| 4 | **Configuration** — statics on ports, and writing one as an event |
| 5 | **Routing kinds** — comparators and iterators |
| 6 | **The map file** — parser, loader, validation, seed. The capstone. |
| 7 | **Seeing inside it** — diagnostics, runtime editing, the HTML docs |
| 8 | **The workbench** — drawing a map in a browser instead of typing it |

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
