# 611 — A dump is a program

**What a running program writes down is everything needed to build it
again, and the only other thing required is `serac`.** A dump is a map
file and a directory of C beside it:

```
grown.map
grown.functions/
    029-demo-boxes.c
    triple_it.c
```

```
serac grown.map      ->  ./grown
```

No source tree, no matching build, no knowledge of where the program
that wrote it was compiled. Two things and a compiler.

## Current behavior

**A dump names files that belong to somebody else's machine.** Every
station line addresses its box by the path that box was compiled from,
which for a box built into the program is a path in whatever tree built
it and for a box compiled while the program ran is a serial-numbered
scratch file belonging to a process that has ended.

**So a dump is only readable by a program that already holds its
boxes.** The engine's own path makes this work by writing the sources it
carries back out and compiling the map against them — which is a real
mechanism and it is why dumps reload at all today. But it means a dump
is a description of a program *plus* a running program that already has
the code, rather than a thing that stands on its own.

**And the two kinds of box do not look alike.** A box built in and a box
that arrived at run time carry different shapes of address, so reading a
dump tells you which is which — a distinction about how the program was
assembled, surfacing in a file that is supposed to say what the program
*is*.

**A box nothing uses is still carried.** The sources written out are
every source the program holds, not the ones its stations actually
place.

## Intended behavior

### What a dump is

**Two things: a map file, and a directory of C files beside it named
after it.** A dump of `grown.map` writes `grown.functions/`, and every
station line in the map addresses its box inside that directory.

`serac grown.map` builds it. That is the whole of what resuming means,
and it needs no tree, no build system and nothing that knows anything
about the program that wrote it.

### Every box looks the same

**A box built into the program and a box compiled while it ran are
written out identically**, because by the time a dump exists the
difference is a fact about history rather than about the program. Both
are a C file in the functions directory and an address pointing at it.

The file keeps the basename its source had. Two sources sharing a
basename are told apart by a number, because a name that collides is a
file that overwrites another.

### Only what is used

**A source is written out when a station in the map places one of its
boxes**, and not otherwise. A program that loaded a library of forty
boxes and placed three writes out the file holding those three.

The unit is the file rather than the box, and it has to be: a box's
source may need a struct, a helper, or an include that sits beside it in
the same file, and a file cut down to one function is a file that may
not compile.

### What this does to the engine's own reload

**The path that hands a map to a running program does not change**, and
still exists for the same reason: a program that is handed new text
while it runs compiles it against the boxes it already holds. What
changes is that a *dump* is no longer written in a form only that path
can read.

The two become the same act read two ways. A dump can be resumed by
`serac` into a fresh program, or handed to a running program that
already holds the boxes — and in the second case the functions directory
is simply not read, because every name already answers.

### The alternatives, and why not

**Keep addressing boxes by their original paths and require a matching
source tree.** What happens today. Rejected because it makes a dump
useless to anybody who does not already have the program, which is most
of the value a dump could have.

**Name each file after a digest of its contents.** Solves collisions and
makes the same source the same file everywhere. Rejected for what it
does to reading: a person opening a dump wants to see `029-demo-boxes.c`
and not `late-a91f3c2e.c`. The collision it prevents is rare and a
number suffix handles it.

**Write out every source the program holds, used or not.** Simpler, and
what happens today. Rejected because a dump is meant to say what the
program is, and forty files for three stations says something else.

## Suggested implementation steps

1. **The dump learns where it is being written.** It takes a stream
   today and cannot know where to put a directory. A path-taking form
   is what the file-writing callers use; the stream form stays for
   whoever wants the text alone and writes no sources.
2. **Collect the sources in use**: walk the stations, take each box's
   address, find the source text it came from — compiled in or arrived
   late, by the same lookup — and keep the distinct ones.
3. **Write them into the functions directory**, basenames kept, numbers
   appended where two collide.
4. **Address every station line into that directory**, which replaces
   the shortening the dump does for late boxes today.
5. **Tests.** A dump of a program that grew builds with `serac` in a
   directory that cannot see this repository, and gives the same answer.
   A box the program holds and never places is not written out. Two
   sources with one basename both survive. A dump of a dump is the same
   dump.

## Related documents and tools

- [610 — A map says where to look](610-a-map-says-where-to-look.md),
  whose relative paths are what let a dump address its own directory
- [609 — A station line names its file](609-a-station-line-names-its-file.md),
  which is why a dump can no longer write a bare function name
- [910 — The engine ships as a compiler](910-the-engine-ships-as-a-compiler.md),
  whose compiler is the one thing resuming needs
- [101 — capture](../tests/101-test-capture.c.info.md) and the dump in
  `src/cera.c`

## What building it turned up

**The reload path collapsed rather than needing a fix.** Loading a map
file used to copy it into a scratch directory, write the program's own
box sources out beside it, and compile the copy — which is why a dump's
relative paths broke the moment anything read one back. The copying
existed to give those spilled sources somewhere to live, and **a map
that names its own sources needs no spilling at all.**

So the two ways a description enters a program are split by one fact:
whether it has a home.

| | text handed to a running program | a map file |
|---|---|---|
| has a home | no | yes |
| what happens to it | copied to scratch, sources spilled beside it | compiled where it sits |
| what is compiled | the build functions alone, bound to the host's boxes | the whole program, every box from the files the map names |
| can fail to bind | yes | nothing binds, so no |

The last row is the one that matters. It is why a dump builds with
`serac` and nothing else, and it retires the whole question of whether
two paths produce the same symbol — a question that had already cost
three attempts at an answer.

**What it costs** is a second copy of any box the loading program
already holds. A box may not remember anything between calls, so two
copies of one are indistinguishable; the alternative was a description
only a program that already had it could read.

## Open questions

**Answered: the directory is named after the map.** `grown.functions/`
beside `grown.map`, so two dumps in one directory cannot take each
other's code.

**Answered: only what is placed.** A program carries the source text of
every box it was built with, whether a station places it or not — that
is what lets a description arrive naming a box nothing has placed yet.
A dump writes out the ones its stations actually place and no others: a
dump says what a program *is*, and what it could have become is a
property of the binary that wrote it rather than of the program.
