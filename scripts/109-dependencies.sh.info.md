# 109-dependencies.sh — what the project needs, and how to get it

The one place that knows what the build wants beyond a C compiler,
whether this machine has it, and what may be done about it when it
does not.

## Usage

    ./scripts/109-dependencies.sh [--check] [--local NAME|all] [--yes] [DIR]

| Argument | Meaning |
|---|---|
| *(none)* | Report, then fix what is safe to fix: update stale local copies, fetch cheap missing things, offer the rest. |
| `--check` | Report only. Changes nothing. Exits unhappily if something required is absent, so it can gate a build. |
| `--local NAME` | Fetch a copy belonging to this project, whether or not the system has one. `all` for everything that can have one. |
| `--yes` | Take every offer without asking. For unattended runs. |
| `DIR` | Project root, if not the one hard-coded at the top. |

`make dependencies` runs the `--check` form.

## The three things it manages

| Tool | For | Required | If missing |
|---|---|---|---|
| a C compiler | the engine, the generator, the tests | yes | nothing can be done; it says so and stops |
| LuaJIT | regenerating the documentation site | no | downloaded and **built from source** |
| a WebAssembly linker | regenerating the workbench's browser module | no | offered as a **prebuilt download** |

Neither optional tool is needed to build or run the engine. Both
regenerate something that is committed in its finished form, so
somebody who never edits a document and never changes the map format
never needs either one.

## The rules, which are deliberately not symmetric

**A local copy is this project's, and is updated without asking.**
Agreeing to one was the decision; keeping it current is the
consequence. A local copy nobody refreshes is worse than none, because
it is silently different from what the project was tested against.

**A system copy is never touched.** Not upgraded, not patched, not
moved, at any version, for any reason. It belongs to a package manager,
and two things writing to one file is how a machine reaches a state
nobody can explain afterwards. When the version is one this project
cannot use, the script says so and suggests a local copy — which is the
most it may do.

**How something missing gets fetched depends on what fetching it
honestly costs.** LuaJIT is built from source: one tarball, nothing but
a C compiler and make, about a minute. The WebAssembly linker is
downloaded prebuilt, and that exception is stated in the script rather
than hidden, because building it from source means building LLVM —
gigabytes of C++ and an hour at best.

**Running it twice does nothing the second time.** This is the property
protected above the others, since it is what allows the script to sit
in front of a build.

## Two questions that look like one

Whether a copy is *the one we pinned* and whether a copy is *good
enough* are different questions, and the first version of this script
asked only the first. That compared what a tool reports against what
the script would build, which for LuaJIT meant comparing a version
string against a commit hash — never equal, so a working system LuaJIT
reported as out of date forever.

So there are two: a **pin**, used when building or downloading a local
copy, exact so that two machines end up with the same binary; and
`acceptable_version`, one test per tool, because the tests are
different shapes. LuaJIT must be from the 2.1 line. The WebAssembly
linker must be the same major version as the clang whose output it
links, which is a number the script looks up rather than one anybody
can write in a table.

## Two facts about WebAssembly worth knowing before reading that part

**gcc cannot target WebAssembly**, so that half of the project is a
clang job whatever `cc` points at.

**A linker and the compiler feeding it must match.** Object files carry
a format version and a linker from a different major refuses them —
correct behaviour, and a baffling message if you do not expect it. This
is why a local copy takes the compiler out of the archive as well as
the linker: from one archive they match by construction, where a
linker alone would be matched against whatever clang the machine
happens to have, which is true today and strange after somebody's next
upgrade.

## Where things land

Everything fetched goes under `toolchain/`, which git ignores —
downloaded and derived, so the project's standing rule about derived
files keeps it out of history, and a local toolchain in history would
also be a several-hundred-megabyte answer to a question nobody asked
when cloning.

`toolchain/installed` is the manifest: one line per fetched tool,
naming the version and how it arrived. Written only after an install
succeeds, so an interrupted download cannot leave it claiming something
that is not there.

The build prefers `toolchain/bin` over the system for anything it finds
there, which is the point of having fetched it.

## What is not tested

The LuaJIT path has been run end to end. **The WebAssembly linker
download has not** — the archive's layout was checked by reading the
first entries of it, and the extraction and symlinking below that have
never executed. Recorded here rather than left for somebody to discover
at the moment they need it to work.
