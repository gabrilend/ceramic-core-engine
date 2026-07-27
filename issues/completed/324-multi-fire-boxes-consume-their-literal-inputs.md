# 324 — multi-fire boxes consume their literal inputs after one revolution

Phase 3 (thread-pool runtime). A cycle dies silently after one lap because
a pop-slot input eats its start-up literal and is never re-fed.

Builds on: issue 304 (task dispatch layer — dispatch / readiness),
issue 302 (wire-value slot store — peek vs pop), issue 221
(iterator box — marks downstream multi-fire), issue 244 (read
boxes, pull-on-demand).

## Status

complete · resolved 2026-07-21 by a design ruling from the map
author, close to option (b) below but grounded in a cleaner
principle — see Current behavior.

> **Vocabulary note, 2026-07-26.** This issue's title and its
> historical sections speak of "multi-fire boxes" as a subset of
> boxes. That category has since been retired: every box is
> multi-spawn, unconditionally, and the load-time walk that
> marked the subset is being deleted in issue 305 (C graph
> loader). The ruling recorded below is
> unaffected and is in fact what the deletion leans on — it had
> already moved the peek/pop choice off the box and onto the
> port. Read "multi-fire box" below as "box", and "the marking
> walk" as a mechanism that no longer exists.

## Current behavior

The design ruling: the two slot kinds are not loop bookkeeping —
they are the **two input methods**. A value entering a port is
either *consumed on use* (each fire takes one delivery) or
*referenced on use* (read in place, never spent). And there is no
such thing as a "loop" in the traditional sense: a map is a
network of boxes that recurse through themselves and iteratively
re-process data or memory locations. The input method on each port
is what shapes how values survive that recursion.

Concretely:

- A typed-in constant whose port has **no incoming wire** is
  referenced: startup delivers it once, every fire re-reads it,
  and it is never spent. This holds no matter how many times the
  box fires.
- **Exception one — the iterator's intake.** An iterator-routing
  box is a recursion source and its input IS the conveyor; a
  literal typed there is the first delivery on the belt, not
  configuration. It stays consuming.
- **Exception two — seed ports.** A port carrying a literal AND
  wires stays consuming: the literal is a seed, and the network
  re-feeds the port each revolution. Pinning it would re-fire
  forever.
- Mechanics: referenced literal ports allocate one-cell single-ring
  peek slots — single-ring because the dual-ring read path always
  pops through its ordering ring regardless of port mode (that was
  the hidden second half of this bug) — and classify as native,
  since the literal is configured in the consumer's own box file
  and belongs to the consumer's language.

Validated by the `tests/maps/324-literal-multi-fire` fixture: a
recursing pair counts to three, which requires the constant to
survive three laps, with a wrong constant surfacing as a named
sentinel. The runtime documentation (docs/004-runtime.md,
Parallelism) now describes the two input methods in these terms.

### As reported (historical)

Two mechanisms collide.

**Peek slots vs pop slots.** A single-fire box's input ports use *peek*
slots: a pushed value sits there permanently and the readiness check just
asks "is a value present?" — fine for one fire. A box meant to keep firing
(iterators, anything downstream of one, anything in a cycle) uses *pop*
slots: each fire consumes one value per input, and the next fire needs a
fresh push.

**How a box gets marked multi-fire.** At graph-load time soramech walks
forward from every iterator-routing box and marks everything reachable as
multi-fire. In a cycle the back-edge carries the mark all the way around, so
every box in the loop becomes multi-fire and every input port becomes a pop
slot.

**The trap.** On start-up the runner does one pass pushing every literal
into its target slot. For a peek slot one push lasts forever. For a pop slot
the first fire eats the literal and the slot goes empty; the second fire's
readiness check finds no value, no fallback, nothing — so the box never
re-spawns and the cycle silently stops after exactly one revolution.
(Confirmed in the transcript: every box fired once, the back-edge push
landed but `think` never re-spawned; a per-push event showed the literal's
slot draining to empty.)

## Intended behavior

A constant that a cycle box needs on every lap (a character id, a config
value) stays available across every re-fire without the box author having to
know about peek vs pop slots. A literal wired into a multi-fire box behaves
like a constant, not a one-shot.

## Suggested implementation steps

The immediate unblock is already known; the durable fix is a runtime policy
choice.

1. **Immediate unblock — use a read box.** Soramech has a third upstream
   kind beyond literals and wires: **read boxes** (pull-on-demand, 244).
   The readiness check has a special clause — if a required input has a
   read-box predecessor it counts as satisfied regardless of slot contents,
   because the runtime can pull the value any time. Wire one `read` box
   (e.g. value `"elara"`) into every cycle box's constant port and the
   readiness check stays green across every re-fire. Same value, same wire
   shape as a literal — the runtime just treats *pullable* and *poppable*
   differently.
2. **Durable fix — decide the literal-into-pop-slot policy.** A literal
   feeding a pop slot is almost always meant as a constant. Options:
   (a) re-push the literal automatically on each fire of a multi-fire
   consumer; (b) keep literal-fed ports as peek slots even when the box is
   marked multi-fire, since a literal has no upstream to re-supply it;
   (c) at load time, lower a literal-into-multi-fire-box wire to an implicit
   read box. Whichever is chosen, the silent case must become impossible —
   an unfed pop slot with no upstream should be a load-time error, not a
   dead run.

Decision to settle: which of (a)/(b)/(c). (b) is the smallest change and
matches intent (a literal is conceptually a peek); (c) unifies on the read
model already in place.

## Related tools / files

- `src/012-dispatch.c` — readiness check and the read-box satisfied clause
- `src/010-graph-loader.c` — the forward multi-fire marking walk
- `src/009-slot-store.c` / `.h` — peek vs pop cell semantics

## Completion notes (2026-07-21)

Steps taken, for reconstruction:

1. In the graph loader's runtime-attach pass, slot mode became a
   per-port decision: a port whose only source is its typed-in
   literal (and whose box is not iterator-routing) gets a one-cell
   peek slot with no ordering ring and no dual-ring flag; all
   other ports keep the box-wide mode from the recursion walk.
2. The per-edge format classification learned the same rule: a
   feeder-less literal port classifies native, mirroring the push
   side's "literals belong to the consumer's language" reasoning.
   Two loader unit-test assertions that pinned the old
   approximation (literal ports classified as JSON, kept working
   only by the parse-failure fallback) were updated with the rule
   spelled out.
3. A note now sits in the dispatch's input reader recording that
   the dual-ring branch always pops — the reason referenced ports
   must be single-ring.
4. New integration fixture `tests/maps/324-literal-multi-fire`
   wired into the suite; docs/004-runtime.md gained the
   input-methods paragraph.

Debugging detour worth remembering: the first fix (slot mode only)
looked correct but changed nothing at runtime, because the
dual-ring reader ignored the mode entirely. The transcript's
slot-allocation and push events were what exposed it — the
back-edge push reported "ok" while the constant's slot drained.

---

## Appendix — original report (verbatim)

> 2. The "multi-spawn boxes eat their literals" rule
>
> Two pieces of soramech machinery collide here:
>
> Single-fire vs many-fire boxes. Most boxes fire once per run — a graph of fan-out-then-converge usually doesn't need re-firing. Their input
>  ports use peek slots: a value pushed into a peek slot sits there permanently, and "is this box ready?" just checks "is there a value?" —
> passes every time, fine for one fire. But boxes that are meant to keep firing — iterators, anything downstream of an iterator, anything in
> a cycle — use pop slots instead: each fire consumes one value from each input. The next fire needs a fresh value pushed in. This is correct
>  for streaming work where each iteration is its own logical tick.
>
> How soramech decides which kind a box is. At graph-load time, soramech runs a forward walk from every iterator-routing box and marks
> everything it can reach as multi-fire. In a cycle, that's everything in the cycle — the iterator marks its downstream, that downstream
> marks its downstream, and eventually the back-edge brings the mark all the way around. Our cycle-loopback is an iterator (one output, just
> a passthrough), so think / prune-1 / process / prune-2 / validate / prune-3 / execute all became multi-fire, all of their input ports
> became pop slots.
>
> The trap. When the runner starts, it does one pass to push every literal into its target slot. For a peek slot, one push lasts forever. For
>  a pop slot, the first fire eats the literal and the slot becomes empty — the second fire's readiness check fails because that port has no
> value, no fallback, no nothing. The cycle silently dies after one revolution. (I confirmed this in the transcript: every box fired exactly
> once, and the back-edge push to think landed in the slot but think never re-spawned.)
>
> The fix. Soramech has a third kind of upstream beyond literals and wires: read boxes. Read boxes don't push into slots; they're a
> pull-on-demand value source. The runtime's "is this box ready?" check has a special clause: if a required input has a read-box predecessor,
>  it counts as satisfied no matter what's in the slot. So wiring one char-elara read box (value "elara") to every cycle box's char_id port
> means the readiness check stays green across every re-fire — the runtime knows it can pull the value any time.
>
> The shape is identical to the literal — same value, same wire — but the runtime treats pullable and poppable differently, and that
> distinction is the difference between one revolution and a hundred.
>
> Both of these are the kind of thing that's obvious in hindsight and well-documented in the soramech source, but the first time you walk
> into them they look like the cycle just doesn't work. The encoder-fast-path one in particular is hard to find without the transcript's
> output_size: 0 clue.
