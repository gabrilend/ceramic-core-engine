# 147-build-serac.sh — building the compiler from source

```
147-build-serac.sh [DIR] [-o OUTPUT]
```

The script somebody runs when all they have is a directory of source
files and they want `serac`. The project's Makefile knows the same three
steps, but a release bundle does not carry the Makefile — it carries
fifteen files and this.

| | |
|---|---|
| `DIR` | where the sources are. Defaults to the project directory named at the top of the script. |
| `-o OUTPUT` | where `serac` lands. Defaults to `DIR/serac`. |
| `CC` in the environment | the C compiler, default `cc`. **Baked into `serac`**, because whatever compiles code added to a program has to agree with it about `sizeof`. |

## The three stages, and why there are three

`serac` contains a copy of the engine, and something has to put it
there.

1. **The generator** — ordinary C depending on nothing.
2. **The engine, as text** — the generator run in `--embed` mode over
   `cera.h`, `cera.c` and the export list, writing them out as C string
   literals.
3. **`serac`** — the generator's own sources plus stage two's output.

There is no bootstrap problem: the program doing the embedding does not
itself need to have been embedded, so stage one builds from nothing.

## Two shapes of directory, found rather than configured

A **release** has every file sitting together. **This repository** has
the generator's sources in `scripts/` and the engine in `src/`. The
script looks for `144-serac.c` in each place and works out which it is
standing in, because somebody who has just unpacked a tarball should not
have to say.

Anything else is refused by name rather than compiled halfway.

## It checks what it built

The last thing it does is `--unpack` into a directory it is about to
throw away and compare the three files against the ones it was built
from, byte for byte. A `serac` carrying a different engine than the one
beside it would build programs nobody could explain, and the check costs
one unpack.

## What a release holds

Fifteen files, about 640 KB: the engine's three, the generator's six,
`serac`'s two, the two headers they share, and this script. Everything
else in this repository is tests, documentation and the tools that
maintain them.
