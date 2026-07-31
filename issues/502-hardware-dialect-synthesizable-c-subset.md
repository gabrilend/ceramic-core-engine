# 502 — The hardware dialect: a synthesizable C subset

## Status

open · phase 5 · sub of 501 (HDL compilation target). The rule
list below is a first draft meant to be argued with, not a
settled specification. Five open questions.

## Current behavior

A C box may contain any C its compiler accepts. It may call
`malloc`, recurse, take a function pointer, loop until a
condition it computes at runtime, use `double`, call `printf`,
and keep mutable file-scope state that races across worker
threads (issue 307 documents that last hazard and asks authors
to be careful, which is the only enforcement that exists). None
of this is checked, because nothing needs it to be: the target
is a CPU, and a CPU will do all of it.

## Intended behavior

A box marked for hardware is checked against a named subset of
C — the **hardware dialect** — before anything is translated.
The dialect is a list of constructs with, for each, a defined
hardware meaning or a refusal. A refusal is a compiler error
that names the file, the line, the construct, and the reason in
hardware terms — not "unsupported construct" but "recursion
requires a call stack; hardware has no stack, and the depth
cannot be bounded at compile time."

The dialect is not a new language. Every program in it is a
program in C, with C's meaning. The dialect only says which
programs in C are also programs about circuits.

## The shape of a rule

Every rule in the dialect has four parts, and the checker
carries all four so the error message can, too:

- **The construct** — what the checker matches in the AST.
- **The verdict** — permitted, permitted with a condition, or
  refused.
- **The hardware reason** — what the construct would have to
  become, and why it cannot or must not.
- **The way out** — what the author should write instead. A
  refusal with no alternative is a dead end, and a dead end is
  where users stop trusting a compiler.

## Rules: memory and lifetime

| Construct | Verdict | Hardware reason |
|-----------|---------|-----------------|
| `malloc` / `calloc` / `realloc` / `free` | refused | There is no heap. Storage in a circuit is decided when the circuit is built; a request for storage at runtime has nothing to answer it. Use a fixed-size array, whose size becomes a memory's depth. |
| Automatic arrays with constant size | permitted | Becomes registers if small, a block memory if large. The threshold is a cost-model decision (issue 509), not a rule. |
| Variable-length arrays (`int a[n]`) | refused | The memory's depth is a physical quantity fixed at build time. `n` is not known then. |
| `static` at file or function scope, mutable | refused by default | In software this is a race across workers (issue 307). In hardware it is a register shared by every instance of the module, which is worse: two boxes calling the same function would share state through the netlist. Permitted only when the box declares itself single-instance and the state is explicitly its own. |
| `const` and `static const` data | permitted | Becomes a ROM, or constant-folds away entirely. |
| Pointer arithmetic beyond indexing a declared array | refused | A pointer in hardware is an address into one specific memory. Arithmetic that could leave that memory has no meaning; there is no flat address space to walk. |
| Two pointer parameters that may alias | refused | Aliasing means two names for one memory port, and the schedule depends on which. Pointer parameters are `restrict` by default in the dialect; the checker rejects a call site that could pass the same object twice. |
| Taking the address of a local | refused | A local is a wire or a register; it has no address. |

## Rules: control flow

| Construct | Verdict | Hardware reason |
|-----------|---------|-----------------|
| `if` / `else` / `switch` | permitted | Becomes a multiplexer when the branches are expressions, a state choice when they contain statements. |
| `for` / `while` with a compile-time constant bound | permitted | Unrolled into parallel logic, or run as an iterated state with a counter. Which one is a cost decision, not a semantic one (issue 506). |
| `for` / `while` with a data-dependent bound | permitted with a condition | The circuit is an FSM that loops until the condition clears — which is fine, and gives variable latency. The condition is that the box declares a maximum iteration count. The generated FSM traps when it is exceeded and raises a fault on the transcript, rather than hanging a device with no way to ask what it is doing. |
| `break` / `continue` | permitted | Ordinary edges in the state graph. |
| `goto` within a function | permitted | A state graph is what `goto` always was. |
| Recursion, direct or mutual | refused | Requires a call stack with runtime depth. There is no stack, and the depth cannot be bounded by looking at the source. |
| Function pointers, including callbacks | refused | The target of a branch has to be known when the wires are drawn. |
| Variadic functions | refused | The argument count and types are part of the module's port list. |
| `setjmp` / `longjmp` | refused | See recursion, but worse. |
| Calls to other dialect functions in the same box | permitted | Inlined, or emitted as a submodule and shared. Issue 506 owns the choice. |

## Rules: arithmetic and types

| Construct | Verdict | Hardware reason |
|-----------|---------|-----------------|
| Declared-width integer types (issue 503) | permitted | The point of the exercise. |
| Bare `int`, `long`, `unsigned` | permitted with a condition | Legal C, but their widths are a property of the host, not of the design. Permitted only where the checker can prove the value never exceeds a width it can pin down — loop counters and array indices, mostly. Everything else must be declared-width. |
| `float` / `double` | refused by default | A soft-float core and libm do not agree bit-for-bit, and chasing that agreement is a bad use of a life. Use the fixed-point types (issue 503). Permitted only when the box explicitly opts into a named floating-point IP and accepts that the equivalence test moves to a tolerance — which is a fallback, and therefore something the user has to ask for in writing. |
| Signed overflow | permitted, redefined | Undefined behavior in C; wrap in hardware. Hardware boxes compile with `-fwrapv`, which makes the C side promise what the silicon already does. The dialect documents wrapping as the defined behavior. |
| Division and modulo by a non-constant | permitted with a condition | A divider is large and slow. Permitted, but it becomes a multi-cycle operation with its own states, and the cost model flags it loudly. Division by a power of two constant is a shift and free. |
| Shifts by a non-constant amount | permitted | A barrel shifter. Costs real area; the cost model says so. |
| Shift of a signed value | permitted, pinned | C's right shift on a negative value is implementation-defined; Verilog distinguishes `>>` from `>>>`. The dialect pins signed right shift to arithmetic and emits `>>>`, matching what every real C compiler does anyway. |
| Side effects inside expressions (`a[i++]`, assignment as a value, comma operator) | refused | C leaves the order unspecified; hardware evaluates the whole expression at once. Rather than define an order, remove the question. |
| Implicit narrowing without an assignment | refused | The place where C's integer promotion and Verilog's operand-width arithmetic disagree. Issue 503 owns the full rule. |

## Rules: the outside world

| Construct | Verdict | Hardware reason |
|-----------|---------|-----------------|
| `printf` and friends | refused | There is no console. Debug output from silicon goes through the trace stream (issue 508), not through libc. |
| File I/O, sockets, time, environment | refused | These are the host's, and a box that needs them belongs in the host region. The checker's error says exactly that, and the editor offers to move the box. |
| `memcpy` / `memset` with a constant size | permitted | Unrolls into wires or a memory-fill state. |
| `memcpy` with a runtime size | permitted with a condition | Becomes a copy loop with a bound, so it needs the same declared maximum a data-dependent loop needs. |
| Any other libc call | refused | The whitelist is short on purpose and grows one function at a time, each with a stated hardware form. |
| `#include <stdint.h>`, `<stdbool.h>`, `<stddef.h>` | permitted | Types only, no code. |
| Arbitrary `#include` | permitted | The preprocessor runs before the dialect check, exactly as it runs before the C compiler. The dialect is checked on what comes out. |

## What the error looks like

The error is the product. A checker that says "line 14: not
synthesizable" has failed at the only job that distinguishes it
from `cc`.

```
hardware dialect: box 'accumulate' (src/accumulate.c:14)
  refused:  malloc
  because:  storage in a circuit is fixed when the circuit is
            built. A runtime allocation has nothing to answer
            it -- there is no heap on the device.
  instead:  declare a fixed-size array. Its size becomes the
            depth of a memory:  sm_u16 buf[256];
  note:     if this box genuinely needs dynamic storage, it
            belongs in the host region. Mark it hardware:false
            and it will keep working exactly as it does today.
```

Four lines of shape: what, why in hardware terms, what to write
instead, and the escape hatch. Every refusal carries all four,
which means the rule table above is the data the checker is
built from rather than prose someone has to keep in sync with
the code. The table becomes a C array of rule records, and the
document is generated from it.

## The dialect is not the same thing as a synthesis subset

Worth stating plainly, because the two get conflated. A vendor
synthesis tool's "synthesizable subset" of Verilog is a list of
what the tool will accept. This dialect is stricter and aimed
elsewhere: it is a list of what has the *same meaning* in both
targets. A construct can be perfectly synthesizable and still
be refused here because the C and the hardware would disagree
about it — unbounded `while` is synthesizable and is permitted
only with a declared bound, not because the tool would choke
but because a device that hangs cannot be asked what happened.

## Open questions

1. **Is `int` permitted at all?** Permitting it with a proof
   obligation ("the checker can pin the width") is friendly and
   makes the checker much harder. Refusing it outright is
   brutal, obvious, and makes every hardware box's source
   visibly a hardware box's source — which arguably breaks
   ground rule 1 of issue 501.
2. **Where does the maximum-iteration bound live?** On the box
   JSON (schema change, editor field, invisible in the source)
   or in the source as a `static_assert` or a loop-adjacent
   constant (visible, but starts looking like a pragma).
3. **Does the dialect apply to the whole translation unit or
   only to the reachable call graph from the box's entry
   function?** Reachability is the useful answer and needs the
   front end to resolve calls before the checker runs.
4. **Should `static` mutable state get a hardware meaning
   rather than a refusal?** A per-instance register is a real,
   useful thing — an accumulator box wants exactly that. The
   catch is that it means something different in the pool
   runner, where the same static is shared across workers. That
   is a genuine dual-compliance break and may be the strongest
   argument for a "hardware boxes are single-instance" rule.
5. **How does a whitelisted libc function get its hardware
   form?** Hand-written HDL kept beside the checker, or a
   dialect-C reimplementation that goes through the same
   translator as user code? The second is far more honest and
   means the whitelist is just a library of boxes.

## Suggested implementation steps

1. **Write the rule table as data.** A C array of records:
   construct matcher, verdict, reason text, alternative text,
   escape-hatch text. Everything else in this issue is a
   consumer of that array.
2. **Generate the dialect document from the table** so the
   document cannot drift from the checker. This is the
   project's standing rule about hard-coded numbers in
   documentation, applied to rules instead of numbers.
3. **Pick the smallest viable subset for the first
   translator**: declared-width integers, `if`/`else`,
   constant-bound `for`, fixed arrays, arithmetic, no calls.
   That is enough for a real box and small enough to finish.
4. **Grow the subset one rule at a time**, each with a fixture
   box that exercises it and an equivalence test that proves
   the two targets agree on it.
5. **Write the refusal fixtures too** — one box per refused
   construct, each asserting the checker stops with the right
   reason. A rule with no test that it fires is a rule that
   will quietly stop firing.

## Relevant files

- `langs/c/spec.c` — where the hardware path forks off the
  existing compile
- `src/010-graph-loader.h` — `box_t`, which grows the hardware
  flag and whatever the iteration bound turns out to be
- `docs/005-writing-boxes.md` — the C box-author guide, which
  gains a hardware-dialect section
- issue 503 (fixed-width and fixed-point types) — the type
  rules this dialect leans on
- issue 504 (C front end and dialect checker) — the parser and
  the enforcement
- issue 307 (C language spec) — the thread-safety discussion
  that the `static` rule extends
