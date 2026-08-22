# 079-rename-identifiers.lua — changing what things are called

A rename tool for names that appear everywhere. Takes a list of
old-to-new pairs and applies all of them in one pass, so that a rename
which collides with another rename still comes out right.

Written for [215](../issues/completed/215-ports-and-slots.md), where
three renames had to happen at once and two of them collided;
[711](../issues/711-the-index-means-reading-order.md) needs the same
thing for file indices and is the reason it is a tool rather than an
afternoon.

## Why one pass rather than several

If A must become B while some existing B must become C, renaming in
sequence turns the original A into C. This substitutes every old name
for a private placeholder first and only then substitutes placeholders
for new names, so **no name is ever renamed twice and the order of the
pairs cannot change the result**.

That is not a theoretical concern. In the rename this was built for,
the word for an input port had to become *port* while the word already
meaning *port* had to become *output port*, in the same files.

## What counts as a match

**Whole identifiers only.** The match requires a non-identifier
character on both sides, so `port` inside `report` is not the word
`port`. A tool without that rule would silently turn a project's
reporting into something else, and the damage would look like a
successful rename.

## Running it

| Argument | Does |
|---|---|
| *mapping file* | Required. Lines of `old new`; blank lines and `#` comments ignored. A name appearing twice on the left is an error, because the mapping would be disagreeing with itself. |
| `--apply` | Write the changes. **Without it nothing is modified** and the run is a report. |
| `--roots=a,b,c` | Which directories to walk, relative to the project root. Defaults to everything documentary and everything source. |
| *a path* | The project root, if not the one hard-coded at the top. |

Reaches `.c`, `.h`, `.lua`, `.md` and `.map` files. Never reaches the
generated C file or the published HTML, because both are derived and
rewriting a derived file makes it disagree with what derives it.

## What it prints

A count per file and a count per pair. **A pair that matched nothing
is called out**, because a silent zero is usually a misspelling in the
mapping, and a misspelled pair is how a rename half-happens — which is
worse than not starting, since half the tree then disagrees with the
other half and both look deliberate.

## What it does not do

**It does not know what code means.** It cannot tell that a local
variable holding an output port should not take the input port's new
name, and it cannot rewrite an English sentence that used the old word
as a word. Both jobs are left to the person running it, which is why
the recommended shape is: run the identifier mapping, compile, read
the diff for prose, and fix sentences by hand.

Two habits make that tractable. Run code and prose as **separate
passes with separate mappings** — a struct field renamed to `out_ports`
is right in a declaration and absurd in a sentence. And compile with
shadow warnings on afterwards, because the one failure this tool can
produce that a compiler otherwise accepts is a renamed variable
quietly shadowing another.
