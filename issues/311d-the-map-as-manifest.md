# 311d — The map as a manifest

Fourth child of [311](311-the-registry-dissolved.md), and the only one
that changes what the build does rather than what the generator emits.
The build reads the program's maps to know which box sources it needs,
and the linker decides what actually ships.

## Current behavior

The generator globs the box source directory and emits a shim for every
function it finds. The build has never seen a map file, so it cannot
know which of those a program uses, and it compiles in all of them.

A program using three boxes out of five hundred carries five hundred
shims. And nothing checks that a name written in a map corresponds to
anything at all until the map is loaded, at runtime, on somebody else's
machine.

## Intended behavior

**A map file is a build input, and remains data at runtime.** Those are
two different claims and running them together is what made this look
impossible for a while:

| question | answer | what it decides |
|---|---|---|
| does the build read the map to know what to include? | **yes** | binary size, and whether the build can check that named boxes exist |
| does the runtime resolve a name from text? | **yes** | whether maps are data or code |

The build never turns `math.c:add` into a call. It learns that it needs
to bring that function along. **Maps stay data**, still read as text at
runtime, still editable while the program runs, still resolvable
through the box table.

**What the build includes is declared, and a map list is one way to
declare it.** A directory is another — *everything under `boxes/`* — for
a general-purpose binary or the test suite, which have no map to read.
Same mechanism at different scope.

**The named files are included whole; the linker trims.** The build
does not try to work out which helper functions a box calls, because
that means understanding C — function pointers, macros, conditional
compilation — and the generator deliberately does not. Instead:

- compile every included box source to an object file,
- emit shims **only** for the functions the maps name,
- link with `-ffunction-sections -fdata-sections -Wl,--gc-sections`.

Each function lands in its own section and the linker discards every
section nothing reaches. **The linker already computes exact
reachability**, correctly, through includes, through hand-written
`extern` declarations, and through function pointers taken by name —
every case a parser would get wrong.

**This spends build time to save binary size, deliberately.** You still
compile five hundred files; you ship three. That is the right trade
here because build time is cheap and a shipped binary is not, and
because the alternative — following `#include` directives to guess a
dependency graph — is a heuristic with a real hole in it: linking
resolves *symbols*, not includes, so a file may call a function it
never included a header for by declaring it by hand.

### The build checks every box reference

This is the quiet win and it may be worth more than the size.

Naming a function that does not exist, naming a file that does not
exist, or naming a bare basename that matches two files with no path
given — all of it fails at `make` time, on the author's machine, naming
the map line. None of that was checkable before, because the build had
never seen a map.

**Wire checking does not move.** It depends on how stations are
actually connected and stays where it is, at load. What moves is
*"you named a box that isn't there,"* which stops being a runtime
discovery.

### When the toolchain is needed, which now follows rather than being stipulated

- **A map naming only boxes the binary carries** → no compiler at
  runtime, ever. Ships as one file.
- **A map naming a box the binary does not carry** → the compiler is
  needed, and it is needed precisely because new code is genuinely
  arriving. That is [310](310-boxes-compiled-at-runtime.md)'s path.

**A map edit does not require a rebuild in general**, and must not:
adding stations that use boxes already present, rewiring, and changing
constants all work on a running program, and editing a running program
is the point rather than an accident. The **only** thing a map edit can
do that a binary cannot handle is name a box it does not carry, and
that failure is specific: *this map names `math.c:divide`, which this
binary does not carry — rebuild, or hand the source to the running
program.* Two ways forward, both named.

**So the binary does not check whether the map changed**, and should
not. A hash comparing the map against the one built from would fire on
every harmless edit, turning a feature into a nuisance.

## Suggested implementation steps

1. The build gains a declared box set: a list of map files, a
   directory, or both. The test suite and the demos use the directory
   form; a shipped program uses its maps.
2. The generator takes that set as input rather than globbing, and
   emits shims only for functions the maps name.
3. Map files join the Makefile's dependency list, so editing one
   regenerates. Splitting the generated material per box source file is
   what makes that incremental.
4. The build-time reference check: every `file:function` in every named
   map exists, is unambiguous, and has the parameter count the station
   line implies.
5. Linker garbage collection turned on, and a test that a binary built
   from a three-box map does not contain the fourth box's code.
6. The load-time refusal message for a box the binary does not carry,
   naming both ways forward.

## Open questions

- **Can the build now check wires, and should it?** It could not
  before, because the map was not a build input. Now the generator sees
  both ends of every wire a map draws. It still cannot compute a size —
  only the compiler can — but it can emit a `_Static_assert` comparing
  the two `sizeof` expressions, which makes the *compiler* do the
  comparison and fails the build with the map line in the message. That
  would move a whole class of error from load time on somebody else's
  machine to build time on the author's. Wires drawn at runtime would
  still need the load-time check, so this adds a second checker rather
  than replacing one — which is either welcome redundancy or two things
  that can disagree, and that is the part to decide.

- Whether a program may declare **several** maps and carry the union of
  their boxes. It should be able to — a workbench binary that opens
  whatever map you hand it is the obvious case, and it is the directory
  form under a different name — but whether that is one declaration
  listing several maps or a general "include this too" needs deciding
  when the declaration is designed rather than now.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [311a — Boxes addressed by file](311a-boxes-addressed-by-file.md),
  which gives the manifest something unambiguous to name
- [311c — Source rides in the binary](311c-source-rides-in-the-binary.md),
  whose embedded text shrinks to what a map actually needs
- [310 — Boxes compiled while the program runs](310-boxes-compiled-at-runtime.md),
  the path a box takes when the binary does not carry it
- [306 — Build integration](completed/306-build-integration.md), the
  build this changes, and whose no-partial-output rule applies
- [007 — The build path](../docs/007-datapath-build.md), which this
  rewrites
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where a binary that needs no toolchain is the thing being bought
