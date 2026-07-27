# 323 — same-language fast path silently drops table values

Phase 3 (thread-pool runtime). A correctness bug in the wire encoder,
discovered downstream in a cycle where every box is Lua.

Builds on / corrects: [312](312-same-language-wire-fast-path.md)
(same-language fast path), [313](313-research-whole-program-same-language-merge.md)
(whole-program merge), [306](306-lua-language-spec.md) (Lua spec).

## Status

complete · the floor fix landed 2026-07-21. The by-reference
ceiling stays with [313](313-research-whole-program-same-language-merge.md);
the "decision to settle" below was settled by
[325](325-language-spec-serialization-shims.md): the floor is
always-on, and the pairing becomes an explicitly declared Lua→Lua
shim when 325 lands.

## Current behavior

A box author returns a table; the wire's language pairing no longer
decides whether it survives:

- On a same-language (native) wire the Lua spec routes table
  returns through the same JSON encoder the cross-language path
  uses. The cell keeps its native tag, and the Lua input side
  rebuilds the value by parsing native bytes whose first byte is
  `{` or `[` (parse failure falls back to the raw string).
- Primitives keep the raw-bytes fast path untouched.
- The remaining non-coercible returns (nil / boolean / function /
  userdata) still travel as zero bytes but announce themselves on
  stderr — never silently.
- The misleading source comment (which claimed tables coerce to
  `table: 0x...` strings) is corrected at the fix site.

Validated twice: the dual-ring unit test now pins three cases
(native+plain stays raw, json parses, native+structured parses),
and the `tests/maps/323-table-fast-path` integration fixture proves
a producer's table crosses an all-Lua wire and arrives as a real
table.

### As reported (historical)

A Lua box's return value reaches the next box by one of two paths, chosen
per-wire by the dispatch layer via the `output_native` flag:

- **Cross-language wire** (`output_native == 0`, at least one non-Lua
  consumer): the value goes through the JSON encoder (`encode_value` in
  `langs/lua/spec.c`), which walks tables, strings, numbers, booleans, and
  nested structures correctly. Tables survive.
- **Same-language wire** (`output_native == 1`, every consumer is Lua):
  the value is coerced with `lua_tolstring`. For a primitive that returns
  copyable bytes; **for a table `lua_tolstring` returns `NULL`**, and the
  spec's `if (!result)` branch sets output size to zero and returns success
  — an empty wire, no message to stderr.

When every box downstream is Lua — as in any all-Lua cycle — every wire
takes the fast path, so every table return silently becomes zero bytes.
The cycle does not visibly fail; it simply stops propagating after the
first revolution's structured payload evaporates.

Note: the source comment at `langs/lua/spec.c:255-263` claims tables "come
back as `table: 0x...`". That is wrong — that string is what
`luaL_tolstring`/`tostring()` produce; raw `lua_tolstring` yields `NULL`.
The comment should be corrected as part of this fix so the next reader is
not misled about which branch fires.

### Present workaround (in the consuming project, not soramech)

One line in the caller's `libs/stage.lua`: each stage wrapper returns the
cognition's **id string** instead of the cognition table. The id is enough
as a "your turn" trigger because the next stage re-reads short-term memory
for the actual prior content. The one list that genuinely had to flow
between stages (the recall-hits list) is parked in a per-character file
under `tmp/`. This is a fallback and must be treated as a warning: it works
only because the payload happens to be reconstructible from disk.

## Intended behavior

A box author returns a table; the box author never learns whether the wire
was same-language or cross-language. Structured values cross every wire
intact, or the run fails loudly — never a silent zero-byte drop.

## Suggested implementation steps

Two independent fixes; the first is the floor, the second is the ceiling.

1. **Floor — never drop silently.** On the same-language fast path, when
   the return is a table (`lua_istable`), do not call `lua_tolstring` and
   fall into the `!result` zero-byte branch. Instead route it through the
   same `encode_value` JSON path the cross-language branch already uses, and
   have the Lua *input* side parse-with-fallback (it already unwraps JSON
   primitives). This costs a serialize/deserialize per table on same-lang
   wires but restores correctness. Fix the stale `255-263` comment while
   here.
2. **Ceiling — pass by reference (issue 313).** Serialization is only
   forced because producer and consumer live in different `lua_State`s.
   When a chain of boxes is provably all-Lua and co-schedulable, merge them
   into one module in one `lua_State` (313's whole-program merge) so the
   returned table is handed to the next function as a live Lua value — zero
   serialization. This is faster but constrains scheduling (the boxes must
   share a state / thread), so it applies only to merged sub-graphs, not
   every same-language wire.

Decision to settle: is the floor fix always-on (every table on a same-lang
wire gets JSON'd), or only when merge is unavailable? See the design
discussion in the appendix and the parallelism-vs-by-reference tension.

## Related tools / files

- `langs/lua/spec.c` — the two paths and the silent-drop branch
- `encode_value` (same file) — the JSON encoder both paths can share
- `src/012-dispatch.c` — where `output_native` is decided per wire
- [244](244-data-box-pull-on-demand.md) — a related pull model

## Completion notes (2026-07-21)

Steps taken, for reconstruction:

1. Output side (`langs/lua/spec.c`, same-language branch): a table
   return is detected before the string coercion and routed through
   the shared JSON encoder; encode failure and buffer overflow are
   loud errors. The stale comment was replaced with one explaining
   the actual NULL-return mechanism and the amended contract.
2. The silent zero-byte branch now writes a typed notice to stderr
   for non-coercible, non-table returns.
3. Input side (same file, native branch): first-byte sniff for
   `{` / `[` parses structured native bytes back into real values;
   everything else keeps the raw-string fast path.
4. The dual-ring per-cell-format unit test was amended: its raw-
   fidelity half now uses unstructured bytes, and a third case pins
   the structured-native sniff. The fixture helper's comment
   (`tests/maps/calc/src/calc.lua`) was updated to match.
5. New integration fixture `tests/maps/323-table-fast-path` wired
   into the suite.

Discovery worth keeping: the issue-317 fidelity convention means an
integer `n` field on a user's table is read as an array-length
declaration by the encoder — the fixture initially tripped over it.
Box authors returning objects should avoid a bare integer `n` key;
325's shim redesign is the place to revisit that edge.

---

## Appendix — original report and design discussion (verbatim)

Preserved unaltered; the emotional record at the end is part of the ticket.

> --> initial bug report:
>
>   1. The "tables don't ride on Lua-to-Lua wires" wrinkle
>
>   The soramech lua spec has two paths for shipping a box's return value to the next box:
>
>   - Cross-language path (a Lua box feeding a non-Lua consumer): goes through a proper JSON encoder that walks tables, encodes
>   strings/numbers/booleans/nested structures correctly. Tables work fine here.
>   - Same-language fast path (every consumer is also Lua): coerces with Lua's lua_tolstring, which produces a string for primitives but
>   returns nil for tables. The spec sees nil and silently sets the output size to zero — no error to stderr, just an empty wire.
>
>   The spec's author was aware of this and left a comment saying so: tables on the fast path is a documented limitation pending the
>   "whole-program merge" feature (soramech's issue 313) that would let one Lua box pass a real Lua table by reference to another Lua box.
>   Until then, Lua → Lua wires only carry primitives.
>
>   What that meant for us: every cycle box is Lua, so every cycle wire is "all-Lua-downstream," so every wire takes the fast path, so every
>   table return silently became zero bytes. The cycle didn't visibly fail — it just didn't propagate.
>
>   The workaround is one line in libs/stage.lua: each stage wrapper returns the cognition's id string instead of the cognition table. The id
>   is enough as a "your turn" trigger because the next stage reads short-term memory for the actual prior content anyway. The recall-hits list
>    — the one piece that genuinely had to flow between stages — sits in a per-character file under tmp/ instead.
>
>   The right long-term fix is the soramech-side: either implement same-language by-reference passing, or detect tables on the fast path and
>   silently fall through to the JSON encoder. Both are upstream changes.
>
> --> response:
>
> this behavior is unacceptable. Not only is 313 already supposed to be implemented, but also we should never have a situation where we
> are dropping data like that. My understanding was that we faced this limitation because each luastate has it's own set of variables,
> and we need to pass them through the stack right? And we can't refer to values outside of the luastate by pointer or anything useful
> like that. So, help me come up with a solution. How do we resolve this for the user? We need to probably just, coerce to json or
> something. It says lua_tolstring just, skips tables for some reason. Can we use a different function that actually handles tables well?
> or is lua going to be forced to read from json every time? that sounds like a terrible problem. How do other threading systems handle
> this? We have Effil as a library on the computer somewhere in /home/ritz/programming/ai-stuff/ or perhaps /home/ritz/programming/lua/ and
> I think they can handle cross-thread synchronization using channels or something. Can you look into that? Are we going to need to
> implement something like that? If so, then how the heck are we going to be able to define that in the user spec? Remember, the user will
> need to define *how they implement multithreading synchronization* within the goddamn language spec, which is absolutely not what we want
> so I really hope you have a better solution for me because I've caught a lot of flak for this. I had to skip my son's basketball game and
> my sister's kid's birthday party! I really wanted some cake but by the time I got there, the traffic was really bad and so I only got to
> scrape some of the frosting off of the platter. It was nice but my favorite part is the cake. The kids had already run out of energy so
> they kinda just sat around and poked at their ipads which was kind of sad. My sister said "gabrilend, why didn't you show up? You're
> always able to get the kids up and moving. I am dissapointed in you." It was really lame. I kinda just stood there and said something
> about how work was getting in the way but... ah whatever. Let's just try and do better next time okay?
