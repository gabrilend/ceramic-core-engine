# 324 — multi-fire boxes consume their literal inputs after one revolution

Phase 3 (thread-pool runtime). A cycle dies silently after one lap because
a pop-slot input eats its start-up literal and is never re-fed.

Builds on: [304](completed/304-task-dispatch-layer.md) (dispatch / readiness),
[302](completed/302-wire-value-slot-store.md) (slot store, peek vs pop),
[221](completed/221-iterator-box.md) (iterator marks downstream multi-fire),
[244](completed/244-data-box-pull-on-demand.md) (read boxes / pull-on-demand).

## Current behavior

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
