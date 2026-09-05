# 215 — Ports and slots, named correctly

A naming debt, paid. The documents have used one vocabulary since the
beginning and the source has used another, and the two disagree about
the two most common nouns in the engine.

**This goes first, ahead of the rest of the port family.** The
alternative was one deliberate pass over code that had stopped moving,
which is cheaper on paper and loses on the thing that matters: the
concurrency line, the port conversion, and the construction surface
are all about to be written, their blueprints already use the correct
vocabulary, and somebody implementing them would read one word in an
issue file and type a different one into the source. **That is exactly
how this debt was contracted the first time.** The price is that the
rename moves code four open issues are about to touch, which makes
their diffs noisier and conflicts with any half-finished branch — paid
once, and paid before the branches exist rather than after.

## Current behavior

**Done for the code, the documents, and the interface files.** 787
identifier replacements across 40 source and test files, 115 more
across 14 documents, the document renamed, every link and every piece
of link text followed, and the state-machine test renamed with it. The
build is clean, every test passes, and the word *cell* no longer
appears anywhere in the project.

**Two things this issue claimed turned out to be wrong**, and both are
worth keeping visible because they are the kind of thing an issue gets
wrong about itself:

**"The documents are right and the source is wrong" was half true.**
Issue 210 states the vocabulary correctly, and the *newer* documents
follow it. Document 002 did not: its body called an input port a slot
throughout, so it needed the same rename its filename did. The step
list below only ever anticipated moving the file. A document can drift
from the vocabulary it is the authority for, and the drift is invisible
precisely because everybody trusts it.

**The rename could not be done in the order the steps give.** Step 1
says rename the cell first "since nothing outside the ring buffer says
the word", and steps 2 and 4 rename the two records separately. Done
sequentially, the input port's new name lands on the word the output
port has not yet vacated — the map file parser has an input-port
variable and an output-port variable in one file, and the input one
was being renamed onto the output one's name. **All three renames had
to happen in a single substitution**, which is what
[079](../../scripts/079-rename-identifiers.lua.info.md) exists for and
why it was built rather than the rename being done by hand.

### The mapping, so this is reproducible

Everything not listed took the obvious form: `slot_*` became
`in_port_*`, `SLOT_*` became `IN_PORT_*`, `map_slot_*` became
`map_in_port_*`.

| was | is | why |
|---|---|---|
| `cell`, `cells`, `CELL_*` | `slot`, `slots`, `SLOT_*` | one place one value sits |
| `slot_t`, `struct slot` | `cera_in_port_t`, `struct in_port` | the standing interface for one input |
| `port_t`, `struct port` | `cera_out_port_t`, `struct out_port` | where a result goes |
| `slots`, `n_slots` on a station | `in_ports`, `n_in_ports` | |
| `ports`, `n_ports` on a station | `out_ports`, `n_out_ports` | |
| `slot_cell`, `slot_cell_move` | `in_port_slot`, `in_port_slot_move` | a port's slot, which is now sayable |
| `.slot` on a destination | `.port` | it always named an input port |
| `station_port`, `port_dests` | `station_out_port`, `out_port_dests` | |

**Three names were deliberately left alone**, each because the rename
made them correct rather than wrong:

- the pool's ring field, which holds one task pointer per **slot** and
  always did
- the generator's growable-array helper, whose local names one element
  position
- `022-test-slots.c`, which proves ring buffers and whose title was
  wrong under the old vocabulary and is right under this one

### What it took beyond the substitution

The compiler found the one collision the tool could not: a delivery
function whose input-port parameter and output-port local both landed
on `port`. It was an error rather than a silent shadowing, which is
the argument for renaming a type to a name already in use rather than
to a fresh one — the collisions surface at build time instead of
becoming a variable that quietly means something else. A shadow-warning
pass afterwards found nothing further.

Four comments meant an input port where they said *slots* and were
rewritten as sentences rather than substituted. One comment naming the
seed sweep and the pull path was corrected while it was open, both
having been removed.

---

The record of what was wrong, kept:

**The documents were right and the source was wrong.**
[210](210-input-port-record.md) states the vocabulary plainly:

> **A port** is the standing interface for one input of one station —
> where its value comes from, what type it is, how many bytes one value
> occupies, and the storage it keeps. **A slot** is one place where one
> value physically sits. **The port decides how a value is stored; the
> slot is where it lands.**

And then admits the gap: *the source calls the port a slot, which is a
naming debt this family does not pay off.*

**What the source actually says:**

| in the code | what it is | what it should be called |
|---|---|---|
| `slot_t *slots` on a station | its **input ports** | ports |
| `port_t *ports` on a station | its **output ports** | ports |
| a *cell*, inside a ring buffer | one place a value sits | **slot** |
| `{station, slot}` in a destination | names an input **port** | port |

So the word *slot* currently means an input port, and the thing that is
actually a slot is called a cell. Every one of those is backwards.

## Intended behavior

**A port is either an input port or an output port, and both are
ports.** A port is the standing interface: it says how a value gets
in or out, and it owns whatever storage that takes. An input port owns
a ring buffer or holds a constant; an output port holds the list of
places its value goes.

**A slot is one place where one value sits.** A ring buffer is made of
slots. A task's argument area is made of slots. Nothing else is a slot.

**So a port may have many slots**, which is the sentence the current
naming makes impossible to say — under it, a slot has many cells, and
*slot* has quietly come to mean the thing that contains slots.

**The shim is not on a port.** It is a field on the station, because a
station places one box and a box has one shim. What output ports carry
is where the result goes: one port for a plain box, three for a
comparator, as many as are wired for an iterator.

## What the rename reaches

This is why it has not happened by accident, and why it should happen
in one deliberate pass rather than drift:

- the station header, where both records are declared
- delivery, which walks both on every value that moves
- statics, which write into an input port
- the loader and the dump, which speak both words in messages a person
  reads
- **a document's filename**: `002-stations-and-slots.md` becomes
  `002-stations-and-ports.md`, which reaches the table of contents and
  every link into it
- the error messages, which are the most visible surface of all — a
  message naming the wrong noun teaches the wrong noun

## Suggested implementation steps

1. Rename the *cell* to the **slot** first, since nothing outside the
   ring buffer says the word and the change is contained.
2. Rename the input port record from `slot_t` to a port type, and its
   field on the station from `slots` to something that says *input*.
   This is the large one and it is mostly mechanical.
3. Rename the destination record's `slot` field, which names an input
   port rather than a slot.
4. The output port record keeps its name and gains a field name that
   says *output* on the station, so the two are symmetrical rather than
   one being named and the other implied.
5. Error messages and reports, checked by reading them rather than by
   compiling — a message can be wrong without failing to build.
6. The document rename, its table-of-contents entry, and every link.
7. A pass over the `.info.md` files, which describe the interfaces in
   the old vocabulary.

## The issue files, and why they could not be done in one pass

**Decided: one vocabulary everywhere, open and completed alike**, so
that nobody reading a blueprint has to translate it into the words the
source uses. Four issue files were renamed — ring-buffer input ports,
port buffer growth, static ports, gatherer ports — and one open child,
*a state on every slot*. The project's own instruction file stated the
one rule in the old words and now does not.

**The first attempt at this was wrong and had to be reverted**, and
the reason is the most useful thing this issue found:

**The issue corpus is written in two vocabularies, and some files are
written in both.** Everything predating the port record calls an input
port a slot; everything written after it already says *port* for an
input and already uses *slot* for the place a value sits. A single
mapping applied to all of them renames the
second group *a second time* — turning "a slot is one place a value
sits" into "a port is one place a value sits". It corrupted the
vocabulary definition in this family's own parent, which is the one
sentence in the project that the rename exists to make true.

**The damage is invisible in the way that matters**: the result is
well-formed prose that reads naturally and says the wrong thing. A
build catches a bad rename in source; nothing catches one in a
document except somebody reading it.

So the corpus was classified before anything was rewritten — files
saying *input port* got the ring-position rename only, files saying
*input slot* got both renames in one substitution, and the handful
using both were done by hand.

**Two kinds of file were deliberately left alone.** Verbatim quoted
notes keep their words, because a note that has been edited to agree
with a later decision is no longer evidence of what somebody actually
asked for — issue 407's argument survives only in the note that
prompted it. And this issue keeps both vocabularies on purpose, since
a record of a rename that speaks only the new words cannot say what
changed.

**One more premise here was wrong.** This issue claimed issue 202's
title was "already correct under the new vocabulary". It was not: the
title read *Ring-buffer input slots*, meaning input ports, and it was
renamed with the rest. What is true is the narrower thing — the word
*slots* in that title is correct *now*, for a different reason than it
was written.

## Open questions

None outstanding.

## Related

- [210 — What an input port is](210-input-port-record.md), which states
  the correct vocabulary and names the debt
- [002 — Stations and ports](../../docs/002-stations-and-ports.md), whose
  filename and body were both part of the debt
- [205 — The delivery walk](205-delivery-walk.md), which
  speaks both nouns constantly
- [202 — Ring buffer slots](202-ring-buffer-ports.md), whose
  title is already correct under the new vocabulary and was not under
  the old one
