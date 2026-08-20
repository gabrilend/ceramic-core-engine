# 311d — The map becomes code

Fourth child of [311](311-the-registry-dissolved.md), and the one that
removes the last name from a running program. The generator reads a map
and emits the construction calls it describes, so a map file is a
**blueprint for the compilation** rather than something a program parses
while it runs.

## Current behavior

The generator globs the box source directory and emits a shim for every
function it finds. The build has never seen a map file, so it cannot
know which of those a program uses, and it compiles in all of them.

At run time a loader reads the map as text, resolves each box name
against the registry, and calls the construction surface. So a program
carries a parser, a table, and every box anyone ever wrote — and nothing
checks that a name in a map corresponds to anything at all until that
loader runs, on somebody else's machine.

## Intended behavior

**A map is compiled into a function that builds it.**

```c
void build_program(map_t *m) {
    map_place(m, 0, place__math_dot_c__add,   PLAIN);
    map_place(m, 1, place__io_dot_c__print,   PLAIN);
    map_wire (m, 0, /*out*/0, /*to*/1, /*port*/0);
    map_set_static(m, 0, /*port*/1, 5);
}
```

**No name survives.** The text `math.c:add` was consumed by the
generator and became a pointer. The station names, the box names, the
port numbers, the constants — all of it resolved while somebody could
still read an error message about it.

**This does not weaken *one way to build a program*.** The generated
function calls the same construction surface a person calling C would,
so loading a map and building one by hand stop being two paths that
must agree and become one path with two authors. The loader stops being
a parser and becomes generated code.

**And the map file loses nothing it had.** It is still text somebody
writes by hand, still the thing a person reads to understand a program,
still what the dump produces. What changes is only what consumes it.

### Editing, which is where the distinction earns itself

Two acts that look similar and are not:

- **Editing the map file** → an incremental rebuild of the parts that
  changed. Splitting the generated material per box source is what
  makes it incremental rather than whole.
- **Editing the running program's structures** — adding a station that
  places a box already present, moving a wire, writing a constant —
  → **no compilation at all.** These are pointers and indices in RAM,
  and no name was ever involved.

So the compiler enters only when new *code* appears, which is the rule
everywhere else in this engine.

### What the build includes

**The maps a program declares are also a manifest.** Having read them
to emit the construction calls, the generator knows exactly which box
sources the program needs. It includes those files whole and emits
shims **only** for the functions the maps name. A program using three
boxes out of five hundred no longer carries five hundred shims.

**The linker decides what actually ships.** Built with
`-ffunction-sections -fdata-sections -Wl,--gc-sections`, every function
lands in its own section and the linker discards every section nothing
reaches — computing exact reachability through includes, through
hand-written `extern` declarations, and through function pointers taken
by name. That is every case a source parser would get wrong, and it
costs nothing but build time.

**Following `#include` directives instead would be a heuristic with a
hole in it**, worth naming so nobody re-proposes it: linking resolves
*symbols*, not includes, so a file may call a function it never
included a header for by declaring it by hand.

**This spends build time to save binary size, deliberately.** You still
compile five hundred files; you ship three.

### The build checks every box reference

Naming a function that does not exist, naming a file that does not
exist, or naming a bare basename that matches two files with no path
given — all of it fails at build time, on the author's machine, naming
the map line. None of that was checkable before, because the build had
never seen a map.

### When the toolchain is needed, which now follows rather than being stipulated

- **A shipped program** compiled from its own map → **no compiler at
  run time, ever.** One file, source text included, runs on a machine
  with no toolchain on it.
- **A map the binary was not built for** → compiled when it arrives,
  which is [311e](311e-running-an-arbitrary-map.md)'s path.
- **A box arriving mid-run** → compiled when the source arrives, which
  is [310](310-boxes-compiled-at-runtime.md)'s path.

The last two are the same mechanism at different scale, and both are
the compiler being needed exactly when new code genuinely arrives.

## Suggested implementation steps

1. The generator learns to read a map file. The parser moves out of the
   engine and into the generator, which is now its only home.
2. Emission of `build_program`: places, wires, constants, in an order
   that satisfies the construction surface's own rules.
3. Map files join the Makefile's dependency list, so editing one
   regenerates. Splitting generated material per box source is what
   makes it incremental.
4. The build-time reference check: every `file:function` in every named
   map exists, is unambiguous, and has the parameter count the station
   line implies.
5. The generator emits shims only for named functions, and includes
   only named sources.
6. Linker garbage collection turned on, and a test that a binary built
   from a three-box map does not contain a fourth box's code.
7. The engine's runtime map parser deleted, and
   [009](../docs/009-datapath-load.md) rewritten around what replaced
   it.
8. A test that a program built from a compiled map and the same program
   built by hand-written construction calls produce identical dumps.

## Open questions

- **Can the build now check wires, and should it?** The generator sees
  both ends of every wire a map draws. It still cannot compute a size —
  only the compiler can — but it can emit a `_Static_assert` comparing
  the two `sizeof` expressions, which makes the *compiler* do the
  comparison and fails the build with the map line in the message. That
  would move a whole class of error from run time to build time. Wires
  drawn at run time would still need the existing check, so this adds a
  second checker rather than replacing one — which is either welcome
  redundancy or two things that can disagree, and that is the part to
  decide.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [311a — Boxes addressed by file](311a-boxes-addressed-by-file.md),
  whose resolution rules this applies
- [311b — Placement instead of records](311b-placement-instead-of-records.md),
  whose functions this calls
- [311c — Source rides in the binary](311c-source-rides-in-the-binary.md),
  whose embedded text shrinks to what a map actually needs
- [311e — Running a map you were not built for](311e-running-an-arbitrary-map.md),
  the same emission performed at run time
- [310 — Boxes compiled while the program runs](310-boxes-compiled-at-runtime.md),
  the same path at the scale of one function
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  the construction surface the generated code calls
- [602 — The loader, first pass](completed/602-loader-first-pass.md)
  and [603 — The loader, second pass](completed/603-loader-second-pass.md),
  which move into the generator
- [007 — The build path](../docs/007-datapath-build.md) and
  [009 — Loading](../docs/009-datapath-load.md), both of which this
  rewrites
