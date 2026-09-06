# 909 — The blueprints name their calls

**Refused.** A blueprint describes what to build and why it has the
shape it has; the name a call ends up with is not part of that. Somebody
rebuilding this project from `issues/completed/` is deciding what the
engine should do, and the header is right there when they want to know
what it is called.

The list of calls exists and is derived rather than typed —
`src/cera.info.md` carries all 101 with their signatures, generated from
the header, grouped into the same components both source files use. That
was the useful half. The arrow pointing back from each blueprint to the
calls it produced is the half nobody needs.

Kept rather than deleted, because a decision not to build something is
worth as much as the decision to build it, and this one was reached by
trying: the derivation was attempted, returned six issues out of
seventy-four, and the reason it returned so few turned out to be the
house style working exactly as intended.

---

## Current behaviour

**The complete public surface exists in one place and is derived** —
`src/cera.info.md` carries all 101 calls in seven sections, generated
from the header, so it cannot drift from what the engine actually
publishes.

**What does not exist is the other direction: which blueprint produced
which call.** Someone working through `issues/completed/` in order to
rebuild this project reaches the end of a ticket knowing what behaviour
was wanted and not what it is called.

### Why the obvious derivation does not work

Crediting an issue with the calls it names was tried and returns almost
nothing: **six issues out of seventy-four name a call at all**, and
three of those are phase 9's own.

That is not an accident and not a defect in the issues. It is this
project's house style working exactly as intended — describe a function
in English rather than by its name, because a name is not something a
reader can reason about. The blueprints say *a station is added to a
running program and started by the ordinary readiness check*, which is
the right sentence, and they do not say which call does it.

So the mapping cannot be extracted. It has to be decided, one blueprint
at a time, by somebody who reads the blueprint and knows the header.

## Intended behaviour

**Each completed issue that produced a call says so, once, in a short
section at the end**, naming the calls and their signatures as declared
in `cera.h`.

The section is an addition to the blueprint, not a replacement for its
prose. The English stays exactly as it is — it is what makes the ticket
readable — and the table underneath is what makes it buildable.

### What each entry has to distinguish

**Produced** and **mentioned** are different, and a table that blurs
them is worse than none, because a reader trusts a table more than a
paragraph. An issue that changed how an existing call behaves is not the
issue that introduced it, and both facts are worth having.

### Where it must not go

**Not into the tickets that only discuss calls in passing.** Phase 9's
own three name a great many calls as examples of what was moved or
renamed, and none of them produced any of those calls.

## Suggested implementation steps

1. **Start from the header's seven sections**, which map onto phases
   closely enough to narrow each call to two or three candidate
   blueprints before any reading is needed.
2. **Read the candidates and assign.** This is the work, and there is no
   way around it being the work.
3. **Generate the tables from the assignment**, so the signatures come
   from the header rather than being typed, and re-running the
   generation after a rename updates every table.
4. **Report the calls nothing claims.** A call no blueprint produced is
   either a gap in the record or a function that arrived without a
   ticket, and both are worth knowing about before the list is called
   finished.

## Related

- [902](completed/902-the-header-says-what-is-public.md), which promised
  this and settled the surface it needs
- [905](completed/905-the-prefix.md), which fixed the names, so the
  tables can be written once rather than twice
- `src/cera.info.md`, the derived list this has to be reconciled with
