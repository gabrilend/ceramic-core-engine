# 144-cerac.c — the ceramic compiler

`cerac` is the one program somebody needs to build a program with this
engine. It carries the generator and the engine's own source inside it,
so the machine it runs on needs a C compiler and nothing else.

```
cerac program.map boxes.c [more.c ...]   an executable, beside the map
cerac --shared program.map boxes.c       a shared object
cerac --shared boxes.c                   boxes alone, for a running program
cerac --emit-c program.map boxes.c       the C, and stop
cerac --unpack DIR                       cera.c, cera.h, the syms file
cerac --map-boxes program.map            which boxes a description names
```

| option | what it changes |
|---|---|
| `-o PATH` | where the result lands, instead of beside the description |
| `--main=FILE` | a C file carrying its own `main`, instead of the emitted one |
| `--root=DIR` | what a box's path is shortened against in generated symbols |
| `--cc=NAME` | the C compiler to invoke |
| `--keep-c=PATH` | also write out the whole text handed to the compiler |
| `--results=N` | how many values of each result the program has room for |

A file ending `.map` is the description and everything else is C. That
is the only thing told apart by extension, because it is the one fact
about these two files a caller has already decided.

## Nothing is written to disk except the program

The compiler is handed a whole program on its standard input — the
header, the engine, the construction code, the `main`, concatenated in
that order.

An `#include` is a filesystem lookup, so a header that exists only
inside this executable would force a directory to exist somewhere with a
copy of it in it. That directory is where three absolute paths from the
machine that ran the build used to come from, and it is the defect issue
910 exists to remove. Putting the header's text in front and deleting
the one line that included it puts the same declarations in scope by the
same rule with no file involved.

The linker's export list was the other file. Naming the exported family
in a flag does what handing over the file did, and produces the
identical set of exported symbols.

The compiler runs without a shell: an argument array goes straight to
the operating system, because a compiler's arguments are other people's
file paths and a space in one is not an error worth having.

## What the emitted `main` does

Nothing is emitted for arguments at all — the engine derives which
marked ports a command line fills and one call hands the whole line
over, including the refusal that says how many the program wanted.

Results need the emitter, because printing a value means knowing its
shape, and that is the return type of the box at the marked station: a
build-time fact that no longer exists at run time. Each result gets an
array and a printer chosen from its type. Values print one per line, in
port order, through the same writers that put a constant into a map
file — so what comes out can be fed back in, a struct result included.

Workers are one per core. The number does not change what the program
computes, only how much of the machine it uses.

## What it refuses

- **Two descriptions**, because a program is built from one.
- **No C sources**, because a description names boxes that have to be
  somewhere.
- **A result marked on a box that returns nothing**, naming the box.
- **A result array filling**, saying how many values there were. Never a
  truncation: the engine's counter climbs past the room on purpose so
  the overflow can be measured rather than guessed at.
- **The engine's own text not including its header exactly once**, which
  would mean the rule for joining it and the text being joined have gone
  out of step.

## The two stages that build it

Stage one compiles `generate`, the ordinary build-time generator, which
knows nothing about any of this. Stage two runs `generate --embed` over
`cera.h`, `cera.c` and the export list to write one C file holding them
as string literals, then compiles `cerac` from the generator's own
sources plus that file. Nothing bootstraps: the program doing the
embedding does not itself need to have been embedded.
