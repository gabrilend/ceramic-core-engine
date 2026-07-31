# 510 — The RTL intermediate, the backends, and the toolchain

## Status

open · phase 5 · sub of 501 (HDL compilation target). Owns the
target-neutral intermediate that 506 emits into, the per-HDL
emitters that consume it, and the path out to a simulator or a
bitstream. Five open questions.

## Current behavior

The project has one compile pipeline
(`scripts/soramech-compile.sh`), one artifact directory
(`compiled/`), and a reference-counted artifact scheme (issue
315) that lets a running map hold a generation of compiled
output while a newer one is built. Compiled output means `.so`
files and merged Lua modules. There is no second target and no
notion of a build that produces something other than code for
this machine.

## Intended behavior

Translation stops at a **target-neutral RTL intermediate**.
Everything downstream — Verilog, SystemVerilog, VHDL, whatever
comes later — is an emitter reading that intermediate. Issue
501's ground rule 7: Verilog is the first backend, not the
interface.

The build produces, per device in the placement plan, a
self-contained project directory: HDL sources, a constraints
file, a testbench, and a Makefile that builds it with no
SoraMech present. The artifact belongs to the user, the same
way issue 307 insists a box's C source belongs to the user.

## Why an intermediate at all

Emitting Verilog text directly from the syntax tree would be
less code today and would cost three times over:

- **Three emitters would share nothing.** VHDL is not Verilog
  with different punctuation; its type system and its
  concurrent-statement rules differ enough that a text-level
  translation is a rewrite. A structural intermediate is the
  only thing all three can be written against.
- **The cost model needs structure, not text.** Issue 509
  counts adders, comparators, and memories. Counting them by
  reading generated Verilog is parsing your own output, which
  is a smell that never stops smelling.
- **The testbench generator needs to know the shape.** Issue
  511 wires stimulus into ports by name and width; it should
  read a data structure, not a regular expression.
- **Debuggability.** A dumped intermediate is the thing to look
  at when a design is wrong, and "is the bug in the lowering or
  the emitter?" is otherwise an expensive question.

## What is in it

A netlist plus a control description, per module:

| Element | Carries |
|---------|---------|
| Nets | name, width, signedness, and where the width came from — a source line, so an error downstream can point at the author's code |
| Cells | an operator (add, compare, mux, shift, multiply, divide), its inputs, its output, its latency |
| Registers | width, reset value, enable condition |
| Memories | depth, width, port count, read latency, initial contents |
| Instances | submodule instantiations: FIFOs, routing cells, operator primitives, other boxes |
| State machine | states, transitions with conditions, and which cells and registers are enabled in which state |
| Ports | the module contract from issue 505 |

Two properties are non-negotiable. Every net's width is
explicit — nothing is inferred at emit time, because inference
is where two emitters quietly disagree. And every element
carries provenance back to a source line, because an error
message that cannot name the author's code is a bad error
message no matter how correct it is.

## The backend interface

The same shape `langs/` already uses. A backend is a plugin
implementing a small struct, discovered and dlopened by a
registry, exactly as the language specs are:

```
hdl/
    verilog2005/    emit.c, Makefile, emit.so
    systemverilog/  emit.c, Makefile, emit.so
    vhdl/           emit.c, Makefile, emit.so
    hdl-backend.h   the interface every backend implements
```

The parallel to `langs/` is not decoration. The build system
(issue 309) already discovers plugin directories, builds them,
and registers what it finds; the registry (issue 311's spec
registry) already dlopens by name. A second plugin family costs
almost nothing because the first one paid for the mechanism.

The interface is roughly: given a module from the intermediate
and an output stream, write it; plus a name, a file extension,
and a declaration of which intermediate features the backend
supports, so an unsupported feature is a stated refusal rather
than malformed output.

### Verilog-2005 first

Because every tool reads it, including the open ones, and
because it is the target the other two get checked against. A
design emitted as Verilog and as VHDL should simulate
identically against the same testbench, and that comparison is
only possible if one of them is trusted first.

### SystemVerilog second

Readable output — `logic`, packed structs, `always_ff` — for
users who will read the generated HDL, and they will. Same
intermediate, different spelling.

### VHDL third

Because a large part of the world uses it, and because it is
the backend that proves the intermediate was actually neutral
rather than Verilog with extra steps. If VHDL emission is
awkward, the intermediate is wrong, and it is better to learn
that from a third backend than from a customer.

## The generated project

```
compiled/hdl/<map>/<generation>/
    device-0/
        sm_box_*.v          generated box modules
        sm_fifo.v           shipped primitives
        sm_route_*.v        shipped routing cells
        sm_top.v            the device's top level: instances and wiring
        constraints.<ext>   pins and clock, from the board descriptor
        tb_*.v              per-box testbenches (issue 511)
        Makefile            simulate, synthesise, program
    device-1/
        ...
    plan.jsonl              the placement plan that produced this
    manifest.jsonl          what was built, from what, with which tools
```

Generations and reference counting follow issue 315 exactly: a
running or simulating build holds its generation; a rebuild
lands beside it; superseded generations are reaped when nothing
holds them. Nothing about that scheme is specific to `.so`
files, which is why it survives the new artifact type without
change.

The Makefile in each device directory works with SoraMech
uninstalled. That is a test, not an aspiration: the build
should be exercised from a copy of the directory somewhere
else.

## The toolchain

Every tool is invoked by a script with a hard-coded `${DIR}`
overridable by argument, paths relative to it, per house rules.
No tool is required to be present unless the target that needs
it is asked for, and a missing tool is an error naming the tool
and the target — never a skipped step that reports success.

| Tool | Role | Required for |
|------|------|--------------|
| Verilator | fast cycle-accurate simulation; the equivalence harness's engine | `make hdl-sim`, and therefore for issue 511 |
| Icarus Verilog | a second, independent simulator | cross-checking; catching designs that depend on one simulator's interpretation |
| GHDL | VHDL simulation | checking the VHDL backend against the Verilog one |
| yosys | synthesis, and the resource reports that calibrate issue 509's cost model | `make hdl-synth` |
| nextpnr | place and route for ice40 / ecp5 | `make hdl-bits` |
| Vendor tools | the large parts | optional, never assumed |

Targets: `make hdl` emits, `make hdl-sim` simulates,
`make hdl-synth` synthesises and writes resource reports,
`make hdl-bits` produces a bitstream. Each depends on the one
before it.

## Open questions

1. **What is the intermediate's on-disk form?** JSONL matches
   everything else here and the project owns a parser and
   writer for it. A binary form is faster and unreadable, and
   the readability is most of the point.
2. **Is the intermediate stable enough to be a documented
   interface?** If someone can write a fourth backend against
   it, it needs a spec and a version. If it is internal, it can
   move freely and the three backends move with it.
3. **Should yosys RTLIL be a backend?** Emitting RTLIL directly
   skips Verilog entirely for the synthesis path, which is
   fewer translations and less standing on the correctness of
   generated Verilog. It also binds that path to one tool.
4. **How much of the top level is generated versus shipped?**
   Wiring box instances together is generated. Clocking,
   reset distribution, and the link physical layer are the same
   every time and want to be shipped, parameterised files.
5. **Do the per-device Makefiles get generated, or are they one
   shipped Makefile plus a generated variables file?** The
   second is far less generated text to maintain and makes the
   directory slightly less self-contained.

## Suggested implementation steps

1. **Define the intermediate as a C data structure first**, and
   write the dumper and loader for its on-disk form. Everything
   in this issue and the two around it reads it.
2. **Hand-write one module in the intermediate**, by code, and
   emit it as Verilog. Compare against a hand-written Verilog
   module for the same box. This validates the whole path
   before the front end can produce anything.
3. **The backend registry and the `hdl/` plugin directory**,
   reusing the build system's existing plugin discovery.
4. **The Verilog-2005 emitter**, feature by feature, each
   feature gaining a simulation test as it lands.
5. **The generated project layout, the manifest, and the
   generation / reference-counting hookup.**
6. **The toolchain scripts and Make targets**, with
   missing-tool errors that name the tool.
7. **The SystemVerilog emitter**, which should be small.
8. **The VHDL emitter**, and with it, the honest verdict on
   whether the intermediate was neutral.

## Relevant files

- `langs/lang-spec.h` and the spec registry — the plugin
  pattern being reused
- `src/011-spec-registry.c` — the dlopen-by-name registry
- issue 309 (build system and Makefile orchestration) — plugin
  discovery and the build the new targets join
- issue 315 (reference-counted compiled-map artifacts) — the
  generation scheme extended to a new artifact type
- issue 506 (datapath and state-machine generation) — the
  producer of the intermediate
- issue 509 (resource estimation and device packing) — the
  consumer that counts cells and reads synthesis reports
- issue 511 (equivalence testing from the run transcript) — the
  consumer that generates testbenches
