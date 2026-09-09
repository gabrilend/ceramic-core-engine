# Implementation notes

Places where the engine had a choice, what the alternatives were, and
what someone building on it should consider for their own map.

These are not the same as the numbered documents one directory up. A
datapath document says **what the engine does**, in the present tense,
and is wrong the moment the code disagrees with it. An implementation
note says **what the engine could have done and might still do** — the
options that were live, what each one costs, which one was taken, and
what would have to be true for a different one to be right.

They are also not issue files. An issue file describes a unit of work to
be built, step by step, and is a blueprint. A note here describes a
decision, and stays useful long after the work is done, because the next
person to hit the same fork arrives with the map already drawn.

## What belongs here

- A design where more than one answer is defensible, and the choice
  depends on the shape of somebody's particular map.
- The reasoning behind a taken decision, especially the parts that argue
  *against* it — the cost of a guarantee, the case where the other
  option wins.
- Extension points: what the minimal implementation deliberately left as
  a named row in a table rather than a built feature, and what building
  it would actually involve.
- Ideas that are not ready to be issues. Say so plainly where they
  appear, so nobody implements a speculation by mistake.

## The rules these notes follow

**Say what is built.** Every note states, near the top, which of the
things it describes exists today. A reader must never have to guess
whether they are reading documentation or a proposal.

**Enumerate guarantees.** When a choice buys a property that is always
true — a bound, an ordering, a consistency window — write it down as a
guarantee, and write down what it cost. A guarantee is worth more than a
saved microsecond, because it is something the next person can build on
without measuring anything first.

**Argue the other side.** A note that only justifies the decision taken
is an advertisement. The comparison has to be honest enough that
somebody with a different workload can correctly disagree.

## The notes

| Note | The question it settles |
|---|---|
| [056 — Why there is no pull path](056-no-pull-path.md) | Values used to be pullable — produced on demand so they could be fresh at the moment of use. Three timings were weighed for that, and then the capability was removed rather than timed. What it was for, what killed it, and what replaced it. |
| [057 — Packaging](057-packaging.md) | What it would take to hand the engine to another project. Nothing built yet; a survey and the decisions it waits on. |
| [135 — A box and a map are one thing](135-a-box-and-a-map-are-one-thing.md) | Placing a box and placing a map hand back different shapes, which is the only real difference between them. What collapsing that costs, and five designs — field selection, a spreader kind, out-parameter boxes, poison-filled buffers, a side file for constants — that were worked out in full and turned down. Nothing built yet. |
