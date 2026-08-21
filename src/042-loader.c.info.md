# 042-loader.c — the loader, from inside

Interface in `040-mapfile.h.info.md`.

**It is a reader, not a builder** (issues 210g, 212). Every structural
thing it does is a call somebody else could make: add a station, name
it, place a box, mark a door, set a port's starting depth, give a port
a source, draw a wire, bring the program up. It owns no rule about
what a legal program is, and there is no state called *still loading*
for it to be in.

The shape:

1. **First pass** — one station per station line. Created, **named
   immediately**, box placed by name (the misspelled-box message is
   deliberately the best in the program), door marked if the line said
   so, then one call per port line: a starting depth where the line
   gave one, then the single configuration operation for a bare dash
   or for a constant. Refusals come back as sentences and go out with
   the file and the line stuck to the front.
2. **Second pass** — arrows resolved by name, which is the one thing
   here the surface cannot do: a name is a fact about the file, an
   index is a fact about the program. Forward references are why two
   passes exist at all. Then the ordinary wiring operation, which
   applies every rule including the width check.
3. **Bring-up** — the whole-program checks and the first tasks, in one
   call any caller can make (issue 212). This caller adds one policy
   of its own: a *file* that starts nothing and has no declared
   entrance is refused, because it would do nothing at all.

## What used to be here and is not

- **A private name table.** The description records in file order,
  searched by name, freed at the end. It existed because names were
  copied onto the map in a sweep after both passes, so during the
  passes the map did not know them. Naming as each station is created
  retires it — and lets every refusal raised while wiring name the
  station in the word the file's author typed.
- **A width check.** It compared the box's return size against the
  destination port's and stopped the program. The wiring operation
  does exactly that for every caller. Keeping both meant this file
  could decide what a legal wire is.
- **A port-range check.** Its wording was the better of the two — it
  named the box and remembered the comparator's threshold — so the
  wording moved into the surface rather than being discarded with the
  check.
- **The seed sweep.** Binding a constant is a write, and a write runs
  the ordinary readiness check, so the writes that build a program are
  the writes that start it (issue 212).

Removing the second of those uncovered a real hole in the surviving
check: it asked the width question only when both stations had input
port arrays, and a box taking no arguments has none. See
`052-rewire.c.info.md`.

## Reading into a program that already exists

The two passes take a **translation table** rather than a base, and
the reason is worth knowing: adding a station hands back a *freed*
place before it grows the table, so a description read into a program
that has had removals gets whatever holes exist, in whatever order.
An offset is right only while nothing has ever been taken out.

That is what makes bringing a description inside an existing program
the same act as reading one into a fresh one (issue 217). Nothing that
already exists is renumbered; new stations are built and the
description's own numbers are translated to wherever they landed.

Arrows resolve against **the description being read**, never against
the program. A name does work only inside a description, so an arrow
to `gate` can never land on somebody else's `gate`.

## The load-time breakdown

Four stages, not five: parse, first pass, second pass, bring-up.
*Validation* was the naming sweep and *seed* was a phase only this
file could enter; neither is a stage any more.
