# 504 — The C front end and the dialect checker

## Status

open · phase 5 · sub of 501 (HDL compilation target). Waits on
503 (fixed-width and fixed-point types) for the type rules it
enforces, and on 502 (hardware dialect) for the rule table it
executes. Four open questions.

## Current behavior

Nothing in the tree reads C. The C language spec (issue 307)
treats a box's source as an opaque file to hand to `cc`; the
graph loader reads JSON; the compile pipeline shells out. The
one hand-written parser the project owns is the JSON parser
(issue 314), written rather than vendored because the format
was small and the failure modes mattered.

## Intended behavior

One front end reads a hardware box's C and produces a typed
syntax tree. Two things consume that tree:

- the **dialect checker**, which walks it against issue 502's
  rule table and either stops with an error or says nothing;
- the **translator** (issue 506), which lowers it to the RTL
  intermediate.

One parser, two consumers. The alternative — a checker that
pattern-matches text and a translator that parses properly — is
two chances to disagree about what a program says, and they
would disagree exactly where a user is already confused.

## The parser's coverage is the dialect

This is the design's central trick, and it is worth stating
before the mechanics.

The hardware dialect is small: declared-width integers, `if`,
`switch`, bounded loops, fixed arrays, arithmetic, and calls to
other dialect functions. A recursive-descent parser for *that*
is a weekend, not a career. A parser for all of C is neither.

So the front end parses the dialect, and **anything it cannot
parse is a dialect error by construction.** There is no gap
between "the parser didn't handle this" and "the dialect
forbids this," because they are the same sentence. A refusal
can never be a parser bug in disguise, which is the failure
mode that makes hand-written compilers untrustworthy.

The obligation this creates: the parser must recognise enough
of full C to *name* what it is refusing. Refusing `malloc(n)`
with "unexpected token" is useless; the parser has to get far
enough to know it is looking at a call to `malloc` before it
declines. So the lexer and the declarator grammar cover more of
C than the dialect does, and the refusal happens at the
semantic layer where the rule table lives, not at the syntax
layer. Full-C constructs are recognised in order to be
described and rejected — never in order to be translated.

## The preprocessor is not our problem

`#include`, `#define`, `#if` — the front end does not implement
any of it. It runs the system compiler in preprocess-only mode
(`cc -E`, plus the box's `cflags`, which issue 307 already
threads through) and parses what comes out. This is the same
`cc` the software half already depends on, so it adds nothing
new to the dependency list, and it means a hardware box's
macros behave identically in both targets because they were
expanded by the same program.

The cost is that the expanded output carries every declaration
from every included header — a few thousand lines of `stdint.h`
and friends for a fifty-line box. Two mitigations:

- **Line markers.** `cc -E` emits `# <line> "<file>"`
  directives. The front end tracks them so every error points
  at the author's file and line, not at the expanded stream.
  Without this the error messages are worthless.
- **Reachability.** Only functions reachable from the box's
  entry function are checked and translated. Header
  declarations that nothing calls are lexed, skipped, and
  forgotten. A prototype for `printf` sitting unused in the
  expanded output is not an error; *calling* it is.

## Stages

| Stage | In | Out | Notes |
|-------|-----|-----|-------|
| preprocess | box source + cflags | expanded C + line markers | forked `cc -E` |
| lex | expanded C | token stream | tracks file/line/column through the markers |
| parse | tokens | syntax tree | dialect grammar; full-C recognition where needed to name a refusal |
| resolve | tree | tree + symbol table + types | declared widths from issue 503, call-graph edges, array shapes |
| reachability | tree + entry fn | pruned tree | everything else dropped |
| check | pruned tree | verdict + errors | issue 502's rule table, executed |
| lower | pruned tree | RTL IR | issue 506's job; the tree is its input |

The first six stages are this issue. The seventh is the next
one.

## Why not libclang

libclang parses all of C correctly, resolves types, and would
save the parser entirely. It is the obvious answer and it is
being declined for three reasons, none of them about
enjoyment:

1. **It is a large external dependency** for a project whose
   entire C runtime links against libc, libdl, and nothing
   else. Requiring LLVM to build a map is a different kind of
   project.
2. **Its coverage is a liability here.** libclang cheerfully
   parses everything, so the dialect boundary would have to be
   re-drawn by hand inside the AST walk, and the "parser
   coverage is the dialect" property is lost. Every gap in the
   hand-written rule walk becomes a construct that slips
   through to the translator and produces wrong hardware
   instead of an error.
3. **The AST is C++ and versioned.** The stable C interface is
   deliberately narrow, and the parts that expose expression
   detail are the parts that move.

It stays the escape hatch. If the hand-written front end runs
aground on real box sources, the rule table and the lowering
survive a front-end swap; only the parse and resolve stages
would be rewritten against libclang's cursors.

## Where it runs

The checker runs at compile time, in the compile pipeline
(`scripts/soramech-compile.sh`), not at graph load and not on
the dispatch path. It runs whenever a hardware box's source is
newer than its last verdict, mtime-checked the same way the C
spec's lazy `.so` compile is.

Its output is a verdict file per box, cached beside the other
compile artifacts, holding the errors in a machine-readable
form so the editor's hardware lens (issue 512) can render them
on the canvas without re-running anything.

Failure is loud and total: a box that fails the dialect check
does not produce HDL, does not get placed, and does not get
silently demoted to a host box. Demotion is a fallback. The
user is told, and the user decides.

## Open questions

1. **Does the front end own type checking, or trust `cc`?**
   The box's C is compiled by `cc` anyway for the software
   target, so type errors will surface there with excellent
   messages. The front end could assume well-typed input and
   only resolve the widths it needs. That is a real
   simplification and it means the hardware compiler behaves
   badly on input that never went through `cc` first — which
   the pipeline can guarantee by ordering, if it is willing to
   compile software-first always.
2. **How much of C must the lexer recognise to name refusals
   well?** Every construct the dialect refuses needs enough
   grammar to be identified. That is a bounded list — it is
   exactly issue 502's rule table — but it is the part of the
   parser that grows every time a rule is added.
3. **What is the verdict file's format?** JSONL matches
   everything else the runtime emits and the project already
   owns a parser and writer for it (issue 314).
4. **Does the checker run on non-hardware C boxes at all?**
   Running it everywhere would tell an author "this box is
   already hardware-ready" for free, which is a genuinely nice
   thing for the editor to know. It also spends time checking
   code nobody asked about, and risks reading as a warning —
   and warnings are errors here, so a passive "not
   synthesizable" note on an ordinary box would be noise with
   the shape of an alarm.

## Suggested implementation steps

1. **Lexer with line-marker tracking**, tested against a
   preprocessed file: every token's reported position must land
   on the author's source, not the expanded stream. This is
   unglamorous and everything downstream depends on it.
2. **Declaration and declarator grammar**, enough to build a
   symbol table with types and array shapes. C's declarator
   syntax is the famously awkward part; get it done early and
   alone.
3. **Expression grammar** with precedence, producing a typed
   tree using issue 503's width resolution.
4. **Statement grammar** — the dialect's control flow, plus
   recognition-for-refusal of the rest.
5. **Call-graph resolution and reachability pruning.**
6. **The rule-table walk**, one rule at a time, each landing
   with its refusal fixture from issue 502.
7. **Verdict file writer**, and the compile-pipeline hook that
   invokes the checker and caches by mtime.

## Relevant files

- `libs/json/json.c` — the hand-written JSON parser of issue
  314; this project's precedent for writing a parser rather
  than vendoring one, and the model for its testing style
- `scripts/soramech-compile.sh` — the pipeline this hooks into
- `langs/c/spec.c` — where `cc` is already forked, and the
  natural home for the `cc -E` invocation
- issue 502 (hardware dialect) — the rule table
- issue 503 (fixed-width and fixed-point types) — the widths
  the resolver reads
- issue 506 (datapath and state-machine generation) — the
  consumer of the tree
- issue 512 (editor hardware lens) — the consumer of the
  verdict file
