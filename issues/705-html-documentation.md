# 705 — The HTML documentation set

## Current behavior

The documentation is a set of markdown files read in a terminal or an
editor. The cross-references between them are text, and the
descriptions of data structures are tables that cannot be poked at.

## Intended behavior

A generated, cross-linked HTML documentation set at `docs/HTML/`, with
a table of contents down the left of every page and a path from every
page to every other.

**Generated, never maintained.** This is why it is deferred to phase 7
rather than built alongside each phase. A set maintained in parallel
with the markdown means every documentation edit is two edits, and the
two drift — at which point there are two descriptions of the engine and
no way to know which is true. The generator reads the markdown and the
`.info.md` files and emits; nobody edits the output.

**Every reference becomes a link.** An issue number goes to the issue.
A function or structure name goes to its `.info.md` entry. A document
reference goes to the document. As many links as the text will carry,
each landing on the specific part rather than the top of a page.

**Syntax highlighting** for embedded code.

**Things to play with, not only read.** The documentation describes a
machine with moving parts, and several of them are far clearer
manipulated than described:

- The ring buffer, with sliders for capacity and fill, showing the
  wrap and the moment growth triggers and how the unwrap copy lands.
- The readiness check, with a station whose slots can be filled and
  emptied by clicking, showing exactly when a task is produced.
- The three-port comparator, with the ports wirable, showing which
  operator the current wiring is equivalent to.
- The iterator's distribution, with adjustable box durations, showing
  fair counts alongside unfair arrival order.
- The sleep and termination protocol, steppable, including the race
  that the re-scan prevents — that one is genuinely hard to hold in
  the head and trivial to see once animated.

**A consistent aesthetic across every page**, matching the project. The
engine is about data moving through a fixed shape, and the pages should
look like they know that.

## Suggested implementation steps

1. The generator: markdown and `.info.md` in, linked HTML out.
2. Link resolution — issue numbers, document numbers, structure and
   function names — with an unresolved reference reported rather than
   silently rendered as text.
3. The shared layout, table of contents, and styling.
4. The interactive pieces, each self-contained enough to be dropped
   into whichever page it explains.
5. A check that every page is reachable from every other.
6. Wire regeneration into the build so that stale HTML cannot ship.

## Related

- Every document in `docs/`
- [000 — Table of contents](../docs/000-table-of-contents.md), the structure this mirrors
