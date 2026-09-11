# 148-name-the-file.lua — every station line says which file its box is in

The one-pass rewrite for [609](../issues/609-a-station-line-names-its-file.md).
A station line used to address its box by a bare function name, searched
for among whatever sources a build happened to be handed. It now always
names the file.

```
148-name-the-file.lua [DIR] [--check]
```

`--check` reports what would change and writes nothing. Running it twice
is harmless: a line that already says its file is one it has nothing to
do with.

## What it rewrites

| where | how it appears | what it becomes |
|---|---|---|
| map files | `station gate (keep)` | `station gate (boxes/029-demo-boxes.c:keep)` |
| documents | the same, in fenced examples | the same |
| C sources | `"station head (seven)\n"` | `"station head (" CERA_ROOT "/src/boxes/029-demo-boxes.c:seven)\n"` |

**C string literals get an absolute path**, spliced in as a
concatenation against the project root the build already defines.
Relative would not do: those descriptions are written into a scratch
directory at run time, and a path relative to *that* points nowhere near
the box sources.

**Completed issues are never touched.** A finished issue describes the
format as it stood when that issue was done, and rewriting its examples
would make it claim to have produced something it did not.

## What it will not do

**Guess.** A name found in two files is reported and skipped. The whole
point of the change is that nobody should guess which file was meant.

## What it cannot see

**Boxes written into a shell heredoc.** A test that builds its own box
source at run time has boxes this tool's index knows nothing about, and
if such a box shares a name with a project box the lookup finds the
project's and writes an address pointing at the wrong file. That happened
on the first run, to a test's own `swallow`.

This is not fixable — the tool would have to run the shell script to know
what it writes. So every name it could not place is reported, and map
text inside a test that builds its own sources is read over by hand.
