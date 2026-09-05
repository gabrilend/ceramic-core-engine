# 098-engine-surface.syms — the engine's public surface

Not code. A linker input, handed over as `--dynamic-list`, naming which
of the engine's symbols go into an executable's **dynamic symbol
table** — the list a shared object consults when it needs to call back
into the program that opened it.

## What it is for

A box or a map compiled while the program runs arrives as a shared
object and is opened with `dlopen`. Its generated station-builder
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
| `cera_*` | stopping — a box ending the program it is running inside — **and every generated station-builder**, whose symbol carries its box's full path under the same prefix |

The station-builders are there for a different reason than everything
else. Not because the engine calls them, but because **a map compiled
next year has to bind to a box compiled today**, and a shared object can
only bind to a name the program it was loaded into published.

That is also where the cost lands. A published symbol is a root the
collector may never touch, and a station-builder holds its shim and the
box body behind it, so a program keeps every box it was built with:

| published | code |
|---|---|
| everything (`-rdynamic`) | 126,225 |
| the surface, and the station-builders | 117,400 |
| the surface only | 95,742 |

21,658 bytes is what staying extendable costs on that binary. A program
publishing only the construction surface would be smaller and could be
extended only by boxes carrying their own copy of everything they
touch, which is extendable in name only.

Leaving a family out fails loudly: a shared object needing an
unpublished symbol fails at `dlopen`, naming the symbol it wanted.

## What it does not reach

A table naming every box makes every station-builder reachable, and
through them every shim. While such a table exists no linker setting
shrinks a program much — so the middle row above is what a binary
measures today either way, and publishing the station-builders is what
keeps them held once the table goes. See
[311](../issues/completed/311-the-registry-dissolved.md), which is the work of
removing it, and
[311d](../issues/completed/311d-the-map-becomes-code.md), which carries the
measurements.

## Related

- [007 — The build path](../docs/007-datapath-build.md)
- [057 — Packaging the engine as a library](../docs/implementation-notes/057-packaging.md)
