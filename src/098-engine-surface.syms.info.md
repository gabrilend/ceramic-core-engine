# 098-engine-surface.syms — the engine's public surface

Not code. A linker input, handed over as `--dynamic-list`, naming which
of the engine's symbols go into an executable's **dynamic symbol
table** — the list a shared object consults when it needs to call back
into the program that opened it.

## What it is for

A box or a map compiled while the program runs arrives as a shared
object and is opened with `dlopen`. Its generated placement function
calls straight into the station layer. A shared object cannot see a
symbol the host executable did not publish, so without this list such a
box loads and then fails to resolve, naming an engine function rather
than anything about the box.

## Why it is narrow rather than sweeping

A published symbol is a **root the section collector cannot touch**.
That follows from what publishing means: the reason a symbol is in the
dynamic table is that code which does not exist yet may ask for it by
name, so the linker can prove nothing about who calls it and has to
keep it.

The convenient one-word form, `-rdynamic`, publishes every global
symbol — and therefore declares the whole binary reachable, at which
point `--gc-sections` has nothing left to discard. The two settings
were in direct opposition and the sweeping one won.

## What it names

Two families, both checked against what the box sources actually reach
for rather than assumed:

| pattern | what it covers |
|---|---|
| `map_*` | the construction surface — placing a station, drawing a wire, naming a door, marking an entrance or a result |
| `sora_*` | stopping — a box ending the program it is running inside |

Leaving a family out fails loudly: a shared object needing an
unpublished symbol fails at `dlopen`, naming the symbol it wanted.

## What it does not reach

A table naming every box makes every placement function reachable, and
through them every shim. While such a table exists no linker setting
shrinks a program much. See
[311](../issues/311-the-registry-dissolved.md), which is the work of
removing it, and
[311d](../issues/311d-the-map-becomes-code.md), which carries the
measurements.

## Related

- [007 — The build path](../docs/007-datapath-build.md)
- [057 — Packaging the engine as a library](../docs/implementation-notes/057-packaging.md)
