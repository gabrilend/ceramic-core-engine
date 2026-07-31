# 503 — Fixed-width and fixed-point types

## Status

open · phase 5 · sub of 501 (HDL compilation target). The
declaration scheme is undecided and is the single most
load-bearing choice in the phase — every other sub-issue reads
widths through whatever this settles on. Three candidate
schemes are laid out below, and the section that picks between
them is deliberately left empty.

## Current behavior

A C box declares its port types in the box JSON as `string`,
`int`, `long`, `double`, `bool`, `bytes`, or `json`, and the
language spec marshals bytes into those C types on the way in
and back out on the way out (issue 307). Inside the function,
the author writes whatever C types they like. `int` is whatever
the host says it is. There is no way to say "this value is nine
bits wide," because on a CPU there is no such thing — nine bits
occupy a register of thirty-two and nobody asks about the other
twenty-three.

## Intended behavior

A hardware box's values carry a **declared width**, visible in
the C source, meaning the same thing to both compilers. A
twelve-bit counter is twelve flip-flops in silicon and wraps at
4096 in software, and neither of those facts is a surprise to
the other.

The scheme also has to cover fixed-point, because issue 501's
ground rule 6 pushes the `nonlinearity` and `weighted` routing
kinds off `double` and onto exact arithmetic that both targets
can perform identically.

## Why this is the hard part

C and Verilog disagree about the width of an expression, and
the disagreement is silent.

```c
uint8_t a = 200, b = 100;
uint8_t c = a + b;          /* C: promote to int, 300, then
                               truncate on store -> 44 */
```

```verilog
logic [7:0] a = 200, b = 100;
logic [7:0] c = a + b;      // 8-bit add, wraps -> 44
```

Those agree — but only because the result was immediately
stored into an eight-bit destination. Change one line:

```c
uint8_t a = 200, b = 100;
uint16_t c = a + b;         /* C: 300 */
```

```verilog
logic [7:0] a = 200, b = 100;
logic [15:0] c = a + b;     // still an 8-bit add, zero-extended -> 44
```

Now they differ, and nothing warns anyone. Verilog sizes an
expression from its *operands*; C sizes it from a promotion
rule that ignores the destination until the store. Any scheme
that does not close this gap is not a scheme.

There are two ways to close it: police it, or make it
structural. Policing means the checker walks every expression
and proves the destination is exactly as wide as the operands.
Structural means picking a declaration form where C's own rules
already produce the hardware answer, and then there is nothing
to police. Structural is better, and two of the three candidates
below achieve it.

## Candidate scheme A — typedefs over standard types

```c
typedef uint16_t sm_u12;    /* 12 significant bits */
```

The width is a comment. The C compiler enforces nothing; the
checker has to prove every store is masked, and the author has
to write the masks:

```c
sm_u12 c = (a + b) & 0xFFF;
```

Honest C, portable to every compiler, and it puts the entire
burden on the checker and the author. The mask is exactly the
kind of thing a person forgets once and then debugs for a day.
Listed for completeness; not recommended.

## Candidate scheme B — unsigned bitfields in a struct

```c
typedef struct { uint16_t v : 12; } sm_u12;

sm_u12 c;
c.v = a.v + b.v;            /* stores modulo 2^12, automatically */
```

C's rule for an unsigned bitfield of width N is that it behaves
as an unsigned integer type of N bits, so the store wraps
modulo 2^N with no mask and no policing. The truncation the
hardware performs is the truncation C performs. The gap closes
structurally.

What it costs: `.v` on every use, which is noise; no pointers
to bitfields, so a fixed-width value can never be passed by
address (arguably a feature, given issue 502 refuses that
anyway); each value occupies its declared storage type rather
than its declared width, so an array of 12-bit values is an
array of 16-bit slots in software and a 12-bit-wide memory in
hardware — a size difference that never becomes a value
difference; and signed bitfields are messier than unsigned
ones and would need pinning down separately.

Works on every C compiler back to C89.

## Candidate scheme C — `_BitInt(N)`

```c
typedef unsigned _BitInt(12) sm_u12;

sm_u12 c = a + b;           /* 12-bit arithmetic, no promotion */
```

C23's `_BitInt(N)` is an integer type of exactly N bits whose
arithmetic is *not* subject to integer promotion — two twelve-
bit operands produce a twelve-bit result. This is precisely
Verilog's rule. The gap does not merely close, it never opens.

What it costs: a compiler from the last couple of years — gcc
14 or clang 16 and up — which is a real constraint for a
project whose C spec deliberately targets whatever `cc` happens
to be. Signed `_BitInt` overflow is still UB, so `-fwrapv`
stays necessary. Older compilers have clang's `_ExtInt` and
gcc's `__attribute__((mode))` as near-equivalents, but leaning
on those puts a compiler-specific spelling in the author's
source, which issue 501's ground rule 1 refuses.

## The declaration scheme

TODO(human)

## Fixed point

Whatever the integer scheme, fixed point is a declared-width
integer plus a documented binary point:

```c
typedef sm_i32 sm_q16_16;   /* 16 integer bits, 16 fractional */
```

Addition and subtraction are ordinary integer operations.
Multiplication needs a shift, and the shift is where precision
is decided:

```c
static inline sm_q16_16 sm_qmul(sm_q16_16 a, sm_q16_16 b) {
    return (sm_q16_16)(((sm_i64)a * (sm_i64)b) >> 16);
}
```

Two things make this the right shape. First, the intermediate
is explicitly wider, which is a declared width and therefore a
declared multiplier size in hardware — no inference. Second,
`sm_qmul` is itself written in the hardware dialect, so it goes
through the same translator as user code rather than being a
hand-written HDL primitive that has to be kept in sync with a C
version. The helper library is not a special case; it is boxes
all the way down.

Rounding is truncation toward negative infinity via the
arithmetic shift, which both targets do identically. Round-to-
nearest is available as a separate helper for authors who want
it, never as a default, because a default that silently changes
results is exactly the kind of thing this phase exists to
prevent.

## Where a width comes from at the wire

Two widths meet at a box boundary: the width of the C variable
the author declared, and the width of the wire the value
travels on. They must be the same number, and only one of them
should be written down twice.

The proposal is that the wire's width is **derived** from the
box's declared return type and the consumer's declared input
type, and that a mismatch is a load-time error rather than an
implicit conversion. A box returning `sm_u12` wired into a port
expecting `sm_u8` is a truncation nobody asked for; SoraMech
does not silently substitute, so it stops and says so, and the
author inserts an explicit narrowing box or widens the port.
This is the same discipline the routing schema already applies
to types, moved into a domain where the cost of getting it
wrong is a wrong number rather than a crash.

## Open questions

1. **The declaration scheme itself** — the empty section above.
2. **Signed widths.** Unsigned is the easy half. Signed
   narrow types need their sign-extension rule pinned in both
   directions, and under scheme B signed bitfields are the
   murkiest corner of the standard. A dialect that supports
   only unsigned narrow types is smaller and covers less.
3. **Does the box JSON learn the new types, or only the C?**
   Port types today are a short list of names. Either the list
   grows a width syntax (`"type": "u12"`), or the port type
   stays coarse and the real width is read out of the C
   declaration by the front end. The second keeps one source of
   truth and makes the editor's port display depend on parsing
   C.
4. **How wide may a wire be?** A 4096-bit value is a legal
   declared width and an unreasonable physical link. There
   should be a cap, and the cap should be a stated number with
   a reason attached, not a place where the tools quietly get
   slow.
5. **Fixed-point format per box or per map?** One Q format
   everywhere is simple and wastes bits; per-box formats are
   right and mean every wire between differently-formatted
   boxes needs a shift the user did not write.

## Suggested implementation steps

1. **Settle the declaration scheme.** Everything below is
   written against it.
2. **Write the header** — `libs/hdl/soramech-hdl.h` or
   equivalent — containing only typedefs, `static_assert`s
   about their widths, and the fixed-point helpers. No macros
   that hide control flow, no conditional compilation between a
   software and a hardware spelling. One file, readable by a
   person who has never seen this project.
3. **Prove the agreement before building anything on it.**
   A test that, for each declared type, exercises add, subtract,
   multiply, shift, and compare at and around the wrap
   boundary — in C, and in the emitted HDL under a simulator —
   and diffs the two. This test exists before the translator
   does, because if it cannot pass, the phase has no floor.
4. **Add the fixed-point curve tables** that the nonlinearity
   routing kind will need (issue 507), and prove those the same
   way.
5. **Teach the graph loader the wire-width rule** and make a
   mismatch a load-time error with both widths in the message.

## Relevant files

- `libs/` — where the header lands; it is a shipped library,
  not a generated artifact
- `src/010-graph-loader.c` — port type parsing and the
  wire-width check
- `src/001-schema.lua` — the editor-side schema, if port types
  gain widths
- issue 502 (hardware dialect) — the rules that lean on these
  types
- issue 506 (datapath and state-machine generation) — the
  consumer that turns a declared width into a bus width
- issue 507 (routing kinds as hardware cells) — the
  fixed-point curve work
