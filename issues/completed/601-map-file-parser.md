# 601 — The map file parser

## Current behavior

**Built, and losing one input-line form while gaining another.**

`in 0 reader` — this port pulls from the station named `reader` — goes
with the pull path
([056](../../docs/implementation-notes/056-no-pull-path.md)). A port is
now a ring buffer or a static, and a station whose value another
station reads simply has an arrow drawn to it. In its place comes a
form for a port with **no source at all**, since a station can be
created before it is wired and the dump writes what is actually there
rather than a tidied version of it ([210](210-input-port-record.md)).

The design decision this issue made is untouched by any of that and is
the reason it reads well now: **the parser reads and never
constructs.** It produces a description; something else builds from it.
That is what lets the loader become a thin reader under
[212](../212-one-way-to-build-a-program.md) — turning each line into
one call on a construction surface — rather than having to be
disentangled from the thing it feeds.

The remainder describes it as built.

Built, in its own parser file that reads and never constructs. Line
oriented, first word dispatching exactly as designed: `in`, `out`,
the `statics` section header, station lines as the remainder, the
kind written as one letter so a forgotten threshold line errors
instead of silently demoting. Statics values ride as raw text to the
end of the line; the `$` stays for the reason this issue gives.
Every malformed shape dies naming file, line, and expectation, each
proven by a forked child whose dying words are checked. One
addition the format needed and the design never specified: `#`
comments to end of line, without which the dump (issue 703) would
have nowhere to put derived facts — docs 008 now records it, and the
first-pass report carries the gap.

## Intended behavior

A parser that reads a map file into an in-memory description, doing no
construction of its own. Building stations from that description is
issue 602; this issue only reads.

**Line-oriented, first word dispatches.** Three keywords: a station
line, an `in` line, an `out` line, plus a `statics` section header.
Three entries in a dispatch table, no grammar, no nesting. Indentation
is for the reader and means nothing to the parser.

**A station line is three words**: the station's name, the box function
it places, and its kind as `p`, `c`, or `i`.

Names rather than numbers because a map is read by people and because
it makes an error message legible. The names cost one lookup table
thrown away when loading ends.

**The kind is written rather than inferred.** A comparator *is*
inferable — it is the station with one more input port than its
function has parameters — but that means forgetting the threshold line
silently demotes a comparator to a plain box that routes everything one
way. One letter of redundancy buys an error instead of a wrong answer.

**A port is a ring buffer unless a line says otherwise.** Only the
exceptions are written, and those carry the port index: `$n` for a
static, a bare name for a gatherer source. A station with no input
lines has nothing unusual about it.

Ring buffers carry no capacity, because they grow on their own.

**The `$` is technically unnecessary** — a static reference is a number
and a gatherer source is a name, so they are already distinguishable.
It stays because `in 1 0` reading as "static entry zero" is not
something anyone will guess a year from now.

**An output line names a port and one destination**: which station,
which port. Repeat the line to fan out. Port numbers stay explicit
because they mean something — a comparator's three are less, equal and
greater in that order, and an iterator's are the sequence it walks.

**A syntax error stops the load and names the line.** A parser that
skips what it does not understand produces a map with a missing wire,
which surfaces much later as a station that never runs.

## Suggested implementation steps

1. Read the file into a description: a list of stations with their
   names, box names, kinds, input overrides, and output lines; plus the
   statics section as numbered text values.
2. The keyword dispatch table.
3. The statics section, with values kept as text — turning them into
   bytes needs the type, which comes from the registry later.
4. Error reporting carrying file, line, and what was expected.
5. Tests over: the worked example from the format document, each
   malformed line kind, a station with no input lines, a port with
   several destinations, and a nested struct constant.

## Related

- [008 — Map file format](../../docs/008-map-file-format.md)
- Issue 602 — what consumes this description
