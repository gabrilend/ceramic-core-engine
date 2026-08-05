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
are held to under [707](707-demos-as-word-problems.md): a picture that
simplifies away the thing that makes a mechanism interesting teaches
someone a machine that does not exist. If a screen cannot be drawn
honestly, it is cut and its subject stays prose.

**Artwork.** The gif generator at
`/mnt/mtwo/programming/ai-stuff/gif-generator/` is available for
producing the animated pieces. Whether frames are generated ahead of
time as images or drawn live in the browser is an open question below,
and it is not a small one — the interactive pieces 705 already built
are drawn live and steerable, and a slideshow of flat images beside
them would read as a different, lesser thing on the same site.

**Where it sits.** It is the site's front door — the thing
`index.html` offers first, with the reference behind it — because a
reader who wants the reference will find it and a reader who wants an
introduction currently has nowhere to be sent.

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

**This is a decision about what the project is.** Publishing the
transcripts means publishing the reasoning with its false starts
attached, including the parts where something was got wrong and then
corrected. That is the point — the completed issues are blueprints and
deliberately do not carry the arguments — but it is worth choosing
rather than doing by default, because it is not reversible in the way
an unpublished file is.

### What both need from the generator

[705](705-html-documentation.md) already carries the fix that makes
this possible and names it as outstanding: discovery is currently a
fixed list of directories, each read one level deep, and a directory
the generator was not told about is missing from the site *silently*.
`llm-transcripts/` is precisely such a directory. Walking the
documentation roots downward, with a directory that yields no pages
reported rather than passed over, is the prerequisite for both halves
of this issue and belongs to 705 rather than here.

## Suggested implementation steps

1. Take the discovery fix from 705 first; without it neither half of
   this reaches the site.
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

- Generated frames or live drawing? Frames are easier to make
  beautiful and are dead on the page; live drawing matches what the
  interactive pieces already do and costs a great deal more work per
  screen. A mixture is possible and might be the worst of both, since
  the seam would be visible.
- Should the slideshow narrate the *engine* or the *project*? Those are
  different sequences. The engine's is the datapath; the project's is
  the phases, and would double as an answer to "what did building this
  teach", which the first-pass report already answers in prose.
- The transcripts contain the user's own words at length, including
  asides that were never meant as documentation. Is the whole log
  published, or an edited one? Editing them makes them a curated
  artifact rather than a record, which costs the thing that makes them
  worth publishing.
- The transcripts grow every session. Does the site regenerate to
  include new ones automatically — in which case a build publishes
  whatever was said that day — or does adding a transcript stay a
  deliberate act?

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
- [707 — The demos told as word problems](707-demos-as-word-problems.md),
  the other half of making this project legible to someone who did not
  build it, and the source of the rule about honest mechanics
- [801 — The workbench in the browser](801-browser-workbench.md), which
  is the third thing that lives on a page and will want the same
  aesthetic
- `../notes/first-pass-report.md`, the prose the project-shaped
  slideshow would be drawn from
- [000 — Table of contents](../docs/000-table-of-contents.md), the
  structure the site mirrors
