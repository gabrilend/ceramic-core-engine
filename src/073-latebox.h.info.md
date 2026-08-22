# 073-latebox.h — boxes that arrive after the program started

The door through which C source becomes a box a running program can
place.

- **`late_compile_source(text)`** — compiles C into the running
  program and adds every box it defines. Returns how many were added,
  or −1. A failure adds nothing at all and puts **the compiler's own
  output** on stderr rather than a summary, because the message that
  says what is wrong with a piece of C is the one the compiler wrote.
- **`late_box_count()` / `late_box_at(i)`** — the rows added
  since the program started, oldest first. Almost nothing needs these;
  lookup by name goes through `box_place_find`, which walks both the
  generated rows and these.
- **`late_source_dir()`** — where saved sources go.
- **`late_source_text(path)`** — the C a late arrival was compiled
  from, by the path it was compiled under, or NULL. Points into the
  loaded object rather than copying, so it lives as long as that object
  does. Reach for `box_source_text` instead, which asks the build first
  and this second: whether a box arrived early or late is not part of
  the question.

Two functions are declared where they are used rather than here,
because only one caller each needs them: `late_place_find` (by
`box_place_find`) and `late_recover_box` (by the loader and by
placement).

**Why a signature is not enough**, since it is the obvious idea: a
signature gives names. It cannot give sizes and offsets, which are what
the engine actually runs on, and the standing rule is that the compiler
computes every one of them. So the signature supplies the paperwork and
a compiler supplies the numbers. There is no version of this that skips
the compiler.

**Which compiler is not incidental.** The build records the one that
built the binary and this uses that one, so a program has exactly one
answer to `sizeof` by construction rather than by checking.

A program that never brings in new code never invokes a compiler and
never needs one present.
