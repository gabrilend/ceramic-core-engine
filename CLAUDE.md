# Minimal SoraMech — project notes

An engine for building programs out of small C functions wired
together, where the wiring decides what runs and when. There is no
main loop and no scheduler anyone writes: you describe a shape, and
the shape executes itself across every core on the machine.

It is a distillation of the larger SoraMech project — which lives on
the `original` branch of this same repository. That one bridges
between languages; this one deliberately cannot, and dropping the
bridge is what buys focus on the runtime underneath it.

## The three nouns

- **A box** is a plain C function. Takes its arguments by value,
  returns one value, and is not permitted to remember anything
  between calls.
- **A station** is one placement of a box in a map. It owns the
  buffers holding values waiting to be fed to that box, the mutex
  guarding them, and the list of places its output goes. The same
  box can appear at several stations, each with its own buffers and
  wiring.
- **A task** is one invocation — a copy of each input value plus a
  pointer to the code that will run.

Decreasing lifetime: the box is compiled into the binary, the
station lives as long as the program, the task lives for one call.

## The one rule

> A station runs when, and only when, every one of its input ports
> holds a value.

Nothing polls and nothing scans for ready work. The check is the
tail end of a write: whoever delivered a value into a port looks at
that station's other ports, and if all are occupied, takes one value
from each and builds a task.

## Orientation

- vision — why the project is shaped this way; read first.
- docs/000-table-of-contents.md — the documentation index.
- docs/058-guarantees.md — the numbered guarantees the runtime
  claims. A change that touches concurrency answers to this page.
- Every source file has a companion `<name>.info.md`. Prefer reading
  that over the source unless debugging that specific file.
- issues/ holds open blueprints; issues/completed/ is the buildable
  history; issues/phase-N-progress.md are the live status pages.

## Build and test

- `make` — build the test binaries and the HTML documentation mirror
- `make test` — build, then run the test binaries
- `make html` — regenerate the HTML documentation only
- `make ramdirs` — create the two-tier RAM scratch directories
- `./run-demo` — run a phase demo

## House rules (project-specific)

- C for the runtime, LuaJIT-compatible Lua for tooling; no Python.
- Errors over fallbacks: fail loudly, never silently substitute a
  default. Fallbacks are warnings, and warnings are errors.
- Indexed filenames (NNN-name) take the next number from the hidden
  .file-index-counter at the project root; bump it when adding a
  file. The numbering runs across the whole project, not per
  directory, so the tree reads in one order.
- Issue files are blueprints for building the software, not work
  logs. When one completes: move it to issues/completed/, update the
  phase progress page, and make one commit for that issue's changes.
- **Commit the transcripts with the work.** A file under
  llm-transcripts/ that appeared or grew during a session is the
  paperwork for that session's work, and it goes into the same
  commit the work produced — it is not somebody else's uncommitted
  mess to ask permission about. The general rule about leaving
  unstaged files alone when they aren't yours is about source and
  documents that another person is mid-edit on; a transcript of the
  conversation you are having is yours by definition. This had to be
  re-derived from scratch three separate times before it was written
  down here, which is the whole reason it is written down here.
  Transcripts also carry the arcane detail no document kept: when a
  decision's reasoning has gone missing, search them before guessing.
