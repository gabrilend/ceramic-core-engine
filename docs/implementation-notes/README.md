# Implementation notes

Places where the engine had a choice, what the alternatives were, and
what someone building on it should consider for their own map.

A datapath document one directory up says **what the engine does**, in
the present tense, and is wrong the moment the code disagrees with it. A
note here says **what the engine could have done and might still do** —
the options that were live, what each one costs, which one was taken, and
what would have to be true for a different one to be right.

They are also not issue files. An issue file describes a unit of work to
be built, step by step. A note here describes a decision, and stays
useful long after the work is done, because the next person to hit the
same fork arrives with the map already drawn.

## What belongs here

- A design where more than one answer is defensible, and the choice
  depends on the shape of somebody's particular map.
- The reasoning behind a taken decision, especially the parts that argue
  *against* it — the cost of a guarantee, the case where the other option
  wins.
- Extension points: what the minimal implementation deliberately left as
  a named row in a table rather than a built feature.
- Ideas that are not ready to be issues. Say so plainly where they
  appear, so nobody implements a speculation by mistake.

## The rules these notes follow

**Say what is built.** Every note states, near the top, which of the
things it describes exists today. A reader must never have to guess
whether they are reading documentation or a proposal.

**Enumerate guarantees.** When a choice buys a property that is always
true — a bound, an ordering, a consistency window — write it down as a
guarantee and write down what it cost.

**Argue the other side.** A note that only justifies the decision taken
is an advertisement. The comparison has to be honest enough that somebody
with a different workload can correctly disagree.

## The notes

| Note | The question it settles |
|---|---|
| [056 — Why there is no pull path](056-no-pull-path.md) | Values used to be pullable — produced on demand so they could be fresh at the moment of use. Three timings were weighed, and then the capability was removed rather than timed. What it was for, what killed it, and what replaced it. |
| [057 — Packaging](057-packaging.md) | What it took to hand the engine to another project. Phase 9 executed the survey; the error handler, the out-of-tree build test and two lifecycle questions are what remain. |
| [090 — One station table per processor](090-one-table-per-processor.md) | A wire cannot leave a station table, which was decided about software and turns out to be a statement about hardware. What follows for composing, and what nothing pins down yet. |
| [135 — A box and a map are one thing](135-a-box-and-a-map-are-one-thing.md) | Placing either one hands back the same receipt — built, and described in [140](../140-a-map-inside-a-map.md). What this note keeps is the five designs worked out in full and turned down: field selection, a spreader kind, out-parameter boxes, poison-filled buffers, a side file for constants. |
