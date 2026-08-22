# 709 — The slideshow and the transcript library

## Current behavior

The documentation site is a reference. It is not an introduction, and
it does not contain the record of how any of it was decided.

[705](705-html-documentation.md) built a generator that reads every
markdown source and emits a cross-linked page per document, per issue,
and per interface file, with a table of contents down the left of every
page and a path from any page to any other. Three of its five
interactive pieces are live. What it produces is excellent for someone
who already knows what they are looking for and has a question about
it.

Two things it does not do:

**There is no way in.** A reader arriving with no context meets a
sidebar of seventy entries and a numbered reading order that asks them
to start at the overview and work down. That is the right structure for
a reference and the wrong one for a first encounter. The engine's whole
subject is data moving through a fixed shape, which is a thing to be
*watched*, and every page describing it is prose.

**The transcripts are not in it.** Five conversation logs sit in
`llm-transcripts/`, running to several hundred kilobytes, and they are
the most complete record the project has of why anything is the way it
is — the arguments, the reversals, the moments something got unstuck.
The project's own conventions treat them as the place to go for the
most arcane detail, and treat the completed issues as blueprints that
should carry references into them. None of that is reachable from the
site. The generator does not know the directory exists.

## Intended behavior

Two additions to the documentation set, sharing its aesthetic and its
generator.

### The slideshow

A sequence of screens covering the parts of the system that most need
seeing rather than reading, shown as motion and diagram with text as
caption rather than as the content.

**It is an argument, not a table of contents.** Each screen answers one
question a newcomer actually has, in an order where each answer makes
the next question askable. What is a station. How does a value get from
one box to the next. What happens when the destination is full. What is
the difference between a value that is pushed and one that is pulled.
Why does a comparator have three exits. How does a text file become a
running graph. What does the pool do while all this is happening.

**The mechanics shown must be the real ones.** The same rule the demos
are held to under [707](completed/707-demos-as-word-problems.md): a picture that
simplifies away the thing that makes a mechanism interesting teaches
someone a machine that does not exist. If a screen cannot be drawn
honestly, it is cut and its subject stays prose.

**It narrates the engine, not the project.** The sequence follows one
value through the machine — what a station is, how a value reaches the
next box, what happens when the destination is full, why a comparator
has three exits, how a text file becomes a running graph, what the
pool is doing throughout. The phases and the lessons of building them
are a different sequence and a good one, and they are not what a
reader arriving with no context needs: the problem this issue names is
that such a reader meets a sidebar of seventy entries, and the first
thing they need is to know what the thing in front of them *is*. The
project's own story is what someone wants once they already care, and
the first-pass report already tells it in prose.

**Artwork: drawn live, screen by screen.** Each screen is a script
that draws stations and moving values in the browser, the way the
three live interactive pieces from [705](705-html-documentation.md)
already are. This is the expensive choice and it is taken on purpose —
generated image frames would be far easier to make beautiful, and they
would be dead on a page whose neighbours can be paused, stepped and
steered, which the reader would feel as two different grades of thing
on one site. The gif generator at
`/mnt/mtwo/programming/ai-stuff/gif-generator/` remains available for
any still or decorative piece that is genuinely an illustration rather
than a mechanism.

**The honesty rule is kept by hand here, so it has to be kept
deliberately.** Nothing about a hand-drawn screen prevents it from
showing a machine that does not exist. Every screen's drawing owes a
reading of the mechanism it depicts, and a screen whose drawing and
whose engine disagree is a bug in the screen.

**Where it sits.** It is one of the site's front doors — the first
thing `index.html` offers, with the reference behind it — because a
reader who wants the reference will find it and a reader who wants an
introduction currently has nowhere to be sent. Phase 8's workbench
([801](801-browser-workbench.md)) becomes a third door beside it, so
the entrance reads *watch, read, build*; the slideshow should be
written knowing it will have neighbours rather than the whole page.

### The transcript library

The conversation logs, readable on the site as a book.

**Paged, not scrolled.** They are hundreds of kilobytes of dialogue,
which is a length that wants chapters, a position that persists, and a
way to skim. A page per session, a contents listing by date, and
navigation forward and back.

**Linked in both directions.** A transcript that discusses an issue
links to that issue's page; an issue links back to the passages where
its work happened. The project's convention already asks for completed
issues to carry references into the transcripts by line number, and
those references are exactly what becomes a hyperlink here — which
makes the convention worth keeping rather than a chore whose payoff is
theoretical.

**Rendered, not dumped.** They are markdown containing code fences,
tool output, and long machine-generated blocks. The same highlighting
and the same layout as the rest of the site, with the long mechanical
passages foldable so the argument stays readable through them.

**Whole and unedited.** The logs go up as they happened — the false
starts, the reversals, the parts where something was got wrong and
then corrected, and the asides that were never written as
documentation. Nothing is selected for them, because a record that was
chosen from is a curated artifact, and the completed issues are
already the curated artifact: they are blueprints that deliberately
drop the arguments. The transcripts are worth having precisely as the
place the arguments survive, which only holds if nobody decided which
arguments were worth surviving.

This was chosen rather than defaulted to, because it is the one part
of this issue that is not reversible the way an unpublished file is.
As of this writing the repository has no remote and the site is local
files, so the decision binds a future publication rather than
describing a present one.

**They get decorated, every once in a while.** A recurring pass over
the transcripts that adds cute emoji and MS-Paint-style drawings —
flowers, hearts, a smiling sun, carrots, horses, stick figure people,
and whatever else belongs beside a passage. This is not in tension
with unedited: **decoration adds and never removes**, so the record
stays whole underneath and gains a margin. It is the difference
between a transcript and a scrapbook, and the scrapbook is the thing
worth having on a site meant to be read by a person rather than
searched by a tool. Nothing about it is automated and nothing about it
is scheduled; it happens when someone is in the mood, which is the
correct trigger for a drawing of a horse.

### What both need from the generator

**The prerequisite has landed.** This was written when discovery was
a fixed list of directories, each read one level deep, so a directory
the generator was not told about was missing from the site *silently*
— and `llm-transcripts/` was precisely such a directory. Under
[705](705-html-documentation.md) discovery now walks each documentation
root downward, every subdirectory becomes its own section named from
the directory itself, and the output is swept of pages no source
produces. Nothing about the generator stands in the way any more.

**What keeps the transcripts off the site is now one deliberate
line**: the generator's file search carries an explicit exclusion for
the transcript directory. That is a better state than the one this
paragraph originally described, because an exclusion is a decision
somebody can find and reverse, where a directory nobody listed was an
omission nothing reported. Removing it is where this half of the issue
starts, and everything after it is rendering rather than plumbing.

That same walk is the whole of the transcript library's inclusion
policy: whatever is in the directory is on the site after the next
build, with no list in between. The generator therefore needs a title
and a date for each log without being told them, which the existing
filenames already carry, and needs to survive a log it has never seen
a shape like before rather than refusing the build.

## Suggested implementation steps

1. **Done, in 705.** Discovery walks the documentation roots downward.
   What is left of this step is deleting the generator's one explicit
   exclusion of the transcript directory.
2. The transcript library, because it is the smaller and more certain
   of the two: render, page, and list the five existing logs, folding
   the mechanical blocks.
3. The cross-links: issue pages to transcript passages and back, driven
   by the line references the completed issues carry. Report a
   reference that resolves to nothing, rather than rendering a dead
   link.
4. Decide the slideshow's medium — generated frames or live drawing —
   and build one screen end to end as the reference for the rest.
5. The remaining screens, each cut if it cannot be drawn honestly.
6. Make the slideshow the site's entry point, with the reference one
   click behind it.

## Open questions

**Answered:**

- *Generated frames or live drawing?* **Live, hand-drawn per screen.**
  The deciding fact is not which is better in isolation but that the
  site already has live, steerable pieces on it: flat images placed
  beside them are not merely less capable, they announce themselves as
  a lesser grade of thing, and a front door is the worst page to say
  that on. The cost is real and is accepted — this is the most
  expensive way to build the slideshow, and it buys a site with one
  texture instead of two.

- *Does the slideshow narrate the engine or the project?* **The
  engine.** Both sequences are worth telling, and only one of them
  answers the question this issue exists to answer. A reader who does
  not know what a station is cannot be told what phase 3 added; a
  reader who does know will find the project's story in the first-pass
  report and the completed issues, which already tell it well.

- *Whole transcripts or an edited log?* **Whole and unedited**, with a
  recurring decorating pass that only ever adds. Written up above.

- *Does a new transcript reach the site automatically, or by a
  deliberate act?* **Automatically, on the next build.** The generator
  walks the directory and renders what it finds, so a session becomes
  a page the next time the site is built. This is the unedited
  decision carried one level outward: if nobody selects which passages
  are worth keeping, nobody selects which sessions are either, and a
  list of approved transcripts would be that selection wearing a
  different hat.

  **The consequence is accepted: building the site is publishing it.**
  Someone who builds to check a broken link puts that day's
  conversation up along with the fix. That is only tolerable because
  the whole-and-unedited decision already means nothing is being held
  back for review — there is no state in which a transcript exists but
  is not meant to be seen, so there is nothing for a build to leak.
  A deliberate list would also have reintroduced the exact failure 705
  is being fixed to eliminate: something missing from the site because
  nobody remembered to name it, with no complaint from the build.

## The note that started this

Kept verbatim:

> we should add to the HTML documentation a slideshow that goes through
> the most crucial aspects of the system, and shows it working using
> visuals over text.
>
> feel free to use /mnt/mtwo/programming/ai-stuff/gif-generator/ to
> create the artwork if you please.
>
> the HTML documentation should also include the llm-transcripts,
> viewable like a book.

## Related

- [705 — The HTML documentation set](705-html-documentation.md), whose
  generator, aesthetic, and directory-walk fix this builds on
- [707 — The demos told as word problems](completed/707-demos-as-word-problems.md),
  the other half of making this project legible to someone who did not
  build it, and the source of the rule about honest mechanics
- [801 — The workbench in the browser](801-browser-workbench.md), which
  is the third thing that lives on a page and will want the same
  aesthetic
- `../notes/first-pass-report.md`, the prose the project-shaped
  slideshow would be drawn from
- [000 — Table of contents](../docs/000-table-of-contents.md), the
  structure the site mirrors
