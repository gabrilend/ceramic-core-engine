# 143-cerac.h — what the compiler carries inside itself

One type and one table, which are how `cerac` holds the engine's own
source as part of its executable.

## What it declares

**`cerac_embedded_t`** — one file carried inside the binary.

| field | type | what it holds |
|---|---|---|
| `name` | `const char *` | the file's basename as it stood in `src/` — the name `--unpack` writes it back out under |
| `text` | `const char *` | the file's bytes exactly as they were read, NUL-terminated |

**`cerac_embedded`** and **`cerac_n_embedded`** — the table itself, in
the order the command line named the files, which for the build is the
order they have to be concatenated in: the header before the body,
because the body's declarations must be in scope before its definitions
arrive.

**`cerac_embedded_text(name)`** — one file by name, or null. The caller
decides what missing means; nothing here substitutes a default, because
a `cerac` built without the engine in it is a broken build rather than a
degraded one.

## Where the definitions come from

Not from a source file anybody wrote. `generate --embed` writes them,
reading each file and emitting its bytes as C string literals, and the
build compiles that output into `cerac` alongside the generator's own
sources.

The text is the source that was compiled, not the source on somebody's
disk now — the same property the box sources riding in a built program
have, and the reason either is worth carrying.

## Why a table rather than three named arrays

`--unpack` writes the files back out under the names they had, and a
table carries the name beside the bytes. Adding a fourth file is then a
change to one command line in the Makefile rather than to three
declarations, three definitions and a writer.
