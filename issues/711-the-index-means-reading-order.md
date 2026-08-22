# 711 — An index is a position in the reading order

**Very low priority.** Nothing is broken, nothing is blocked, and no
other issue waits on this. It is here so the rule is written down
somewhere before the numbering drifts further, and so that whoever
eventually cares has a plan rather than an argument.

## Current behavior

**The rule is written down, the tool exists, and the order has not
been changed yet** — which is the sequence this issue itself
prescribes, because deciding what should be read after what is the
actual work and the renaming is the mechanical part.

**The convention is stated** where a reader arrives, in the table of
contents: an index is where a file sits in the reading order, and
nothing else. Adjacency is a consequence rather than a purpose; a
companion takes its source's number because it is the same step; a
letter suffix is the second half of one step. Issue files are numbered
differently and deliberately — a phase and a sequence within it, which
is where a piece of *work* sits in the construction of the software
rather than where a file sits in the reading of it.

**The tool exists** and does the whole job: it takes a desired order,
refuses anything missing or listed twice, renames through git so both
names are in the history, and rewrites every reference in source
includes, companion documents, issues and prose. Renames are staged
behind placeholders, because 019 becoming 021 while some 021 becomes
019 turns the first into the second if done one at a time.

**And it answers a second question that turned out to be worth
asking**: which indexed filenames are mentioned anywhere in the
project that are not files. Its first run found two, both in one
header — the dump was said to live in `050-dump.c` and rewiring in
`051-rewire.c`, when they are `051` and `052`. Both are now right.
Everything else it reported was somebody being deliberate: an issue
describing a rename that already happened, a comment illustrating the
convention with a file that never existed.

### Where the sequence stops reading well

Read end to end, four things break the order. They are listed so the
judgement can be argued about rather than made silently, and none of
them is fixed yet.

**Tools sit between engine files.** The documentation-site generator
is `054`, between the rewire source and an implementation note; the
identifier renamer is `079`, between two station tests; the issue and
link tools are `088` and `089`; this renumberer is `097`. A reader
following the order meets a build tool in the middle of the engine
four separate times. These are project tooling, not steps in the
explanation, and they want to be together and somewhere a reader
reaches deliberately.

**The generator comes after everything it generates.** The registry
header is `026` and its test is `030`; the generator that *writes*
that registry is `065` to `070`. The producer sits forty places after
the product, so a reader meets a table with no account of where it
came from and finds the account much later.

**Tests have drifted from what they prove.** The station layer is
`018` to `020` and its first tests are `021` to `024`, which reads
exactly right. But the fan-in cost test is `063`, the slot-state test
is `064`, and destinations, removal, growth, page growth, placement
and construction are `076` to `082` — all of them about the station
layer, forty to sixty places away from it. That is the drift toward
*relatedness by subsystem* this issue names, arriving from the other
direction: they were numbered when they were written rather than where
they belong.

**The guarantees page is `058`.** It is the page this project's own
orientation tells a reader to consult, and the page a change touching
concurrency answers to. Fifty-eighth is not where that belongs.

### What stood before



Every file in the project carries an index — `019-station.c`,
`020-delivery.c` — and the numbers run across the whole tree rather
than per directory, so the project sorts into one sequence. The next
number comes from the hidden `.file-index-counter` at the project root.

The sequence genuinely reads well in places. Engine files interleave
with the tests that prove them: 018 and 019 are what a station is and
how one is placed, 020 is a value arriving, and 021 through 024 are the
tests for exactly those. A thing, then its proof, then the next thing.

**Two problems, and only the second one needs work.**

**The rule was never written down.** A reader meeting `033-statics.c`
beside `034-gather.c` has no way to learn whether the numbers mean
*read these in this order*, *these were created in this order*, or
*these are related to each other*. The convention is followed carefully
and communicates nothing to anyone who was not told about it. That was
raised while numbering the phase demo runners and is larger than they
were.

**And the numbering has drifted toward meaning relatedness.** Where
files sit near each other because they are *about* the same thing
rather than because one should be read after the other, the index has
quietly acquired a second meaning. Two meanings in one channel is the
thing this project avoids everywhere else.

## Intended behavior

**An index means one thing: where this file sits in the order somebody
should read the project.** Not what it is about, not what it relates
to, not when it was made. If two files should be read one after the
other they are adjacent; if they merely concern the same subsystem,
that is what the directory and the name say, and the index says nothing
about it.

**Adjacency is a consequence, not a purpose.** Related files will often
end up near each other, because a thing and its test usually are read
together. That is fine. What is not fine is *choosing* a number in
order to place a file near its relatives, because then a reader
following the order meets a detour nobody signposted.

**The letter suffix survives untouched.** A demo runner takes its
source's index with a letter appended — `037a` runs `037` — and that is
consistent rather than an exception: a runner is not a step in the
story, it is the second half of the step its source is. The same shape
the issues use when one splits into parts.

**Renumbering is done by a tool, never by hand.** The tool reads the
tree, takes a desired order, rewrites the filenames, updates the
counter, and — this is the part that matters — **rewrites every
reference to every renamed file**, in source includes, in the build, in
the info files, in the documents, and in the issues. A renumbering done
by hand is a renumbering that leaves stragglers, and a straggler here
is a broken build or a dead link rather than a cosmetic flaw.

**The order has to be decided before it can be applied**, and that is
the actual work. The tool is mechanical; deciding what should be read
after what is a judgement about how the project is best explained, and
it should be made by reading the sequence end to end rather than by
sorting on anything.

## Suggested implementation steps

1. **Done.** Read end to end, with four findings recorded above:
   tools interleaved with the engine, the generator after everything
   it generates, tests drifted from what they prove, and the
   guarantees page at fifty-eight.

   *(The original wording of this step follows.)* Read the current
   sequence end to end and write down where it stops
   being a reading order — the places a number was chosen for
   adjacency rather than for sequence. That list is the specification
   for everything after it.
2. **Done.** `scripts/097-renumber.lua`: read the tree, accept a
   desired order, rename,
   rewrite every reference, update `.file-index-counter`. It must be
   able to report what it *would* do without doing it, because the
   first run of a tool like this is the one nobody trusts.
3. **Done**, as `--check` on the same tool, and it found two real
   stale references on its first run. A check that nothing references
   a name that no longer exists, run
   after the rename — the same sweep the project already expects by
   hand whenever a path changes.
4. **Not done, and deliberately the last thing.** The four findings
   above are a judgement about how the project is best explained, and
   they should be argued about before fifty files move. Apply it. One
   commit, so the before and the after are both in the
   record and the move is reversible.
5. The documentation site's navigation follows the index afterward, so
   the order is visible rather than merely true.

## Open questions

- **How does the convention explain itself to a reader?** Renumbering
  makes the order correct; it does not make it legible. Four shapes
  were considered and none chosen, because the choice is worth making
  when somebody is actually doing the work rather than now:
  - a line in each file's own header naming what comes before and
    after, generated by the same tool — which reaches a reader who
    never opens a document, and that is the failure case;
  - a generated reading index listing every file in order with a line
    about each, taken from the header each file already has, which
    shows the whole shape at once but has to be found;
  - both, with the documentation site's navigation ordered by index and
    next-and-previous links at each page foot;
  - nothing, on the grounds that the convention is for the author and a
    stranger reads in whatever order they like anyway.

## Related

- [710 — The demos after the pull path](710-demos-after-the-pull-path.md),
  where this was raised, and which settled the runner suffix
- [705 — The HTML documentation set](705-html-documentation.md), whose
  navigation would carry the order if it is to be shown rather than
  stated
- [000 — Table of contents](../docs/000-table-of-contents.md), which
  states the reading order for documents and is the model for stating
  it for everything else
