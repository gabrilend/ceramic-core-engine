# 705 — The HTML documentation set

## Current behavior

IN PROGRESS — the generator and site stand; two of the five
interactive pieces remain. Both structural faults are fixed: discovery
walks rather than being told where to look, and the output is swept of
pages no source produces.

Built so far: a Lua generator (`make html`, also run by a full
build so stale HTML cannot ship) reading every markdown source —
docs, notes, issues open and completed, every interface file, and
the sealed vision rendered verbatim — and emitting seventy-odd
cross-linked pages into `docs/HTML/` with one aesthetic, a table of
contents down the left of every page (reachability is structural
and checked), issue numbers linkified wherever they appear, code
fences highlighted, and unresolved references reported rather than
silently rendered. Three interactive pieces ride the pages they
explain: the ring buffer with a capacity slider showing wrap and
the growth unwrap, the readiness check firing on the click that
fills the last slot, and the iterator dealing even counts in
scrambled order under an adjustable slow consumer.

Remaining for the second pass: the wirable three-port comparator
piece and the steppable sleep-and-termination protocol (the race
animation), plus deeper links that land mid-page rather than at
page tops.

**Both structural faults are fixed.**

*Discovery walks rather than lists.* Every subdirectory of `docs/`
becomes its own section, named from the directory itself, so the
hand-written pass for implementation notes is gone and the next
subdirectory enrols itself. Interface files are found anywhere under
the project, because one lives beside the source it describes and
which directory that is should never decide whether it reaches the
site. The top-level document listing stays one level deep on purpose:
those documents are a reading order, and a subdirectory folded into
them would break the sequence.

*And the output is swept.* Anything in it that no source produces is
deleted, and the count is printed rather than done quietly. Deleting
is safe precisely because the directory is entirely derived — nothing
in it is authored, so nothing in it can be lost.

The sweep earned itself immediately. Over one working session a
document was renamed, four issue files were renamed, and six issues
moved to completed; every one left a stale page behind that had to be
found and deleted by hand, each with a live URL serving content that
quietly disagreed with the project. Anything a generator does not do
automatically is something a person has to remember, and forgetting is
silent.

Both were checked by doing them: a directory nobody had named appeared
on the site with its own heading, and vanished again when the
directory went away.

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

**No document can be silently absent.** Every markdown file under the
documentation roots reaches the site, however deeply nested, and a
directory that yields no pages is reported rather than passed over. The
generator's promise is that the site is the documentation — a file that
exists but does not appear breaks that promise in the one way nobody
notices, because the missing page leaves no gap to see.

Nesting is meaningful, though, and flattening it would lose the reading
order: the numbered documents at the top level are a sequence, and a
subdirectory is a group with its own character. So discovery walks
downward, and each directory becomes its own heading in the table of
contents rather than being folded into its parent's list. A directory
supplies its own name and description by carrying a `README.md`.

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

The colour scheme the first pass arrived at is settled and stays —
described as seraphic, then corrected to ceramic, which is a better
word for it. What the second pass owes is everything else: the pages
should be more artistic than a stylesheet applied to markdown. This
is a deliberate re-opening of a part that was called done, and it is
the same request the workbench in [801](801-browser-workbench.md)
opens with, because the two will sit beside each other and must not
look like they came from different projects.

## Suggested implementation steps

1. The generator: markdown and `.info.md` in, linked HTML out.
   Discovery walks each documentation root downward instead of listing
   it one level deep, so nesting costs the generator nothing and a new
   subdirectory needs no edit here. A directory's `README.md` names it
   in the sidebar; a directory that produced no pages is reported.
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
