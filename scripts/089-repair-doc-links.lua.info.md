# 089-repair-doc-links.lua — making the record walkable, from outside

Reads every link in every markdown document in the project, and where
one names a file that is not at that path, re-aims it at the file it
was asking for.

## Usage

    luajit scripts/089-repair-doc-links.lua [--apply] [DIR]

| Argument | Meaning |
|---|---|
| `--apply` | Actually write. Without it nothing changes and the report says what would have happened. |
| `DIR` | Project root, if not the one hard-coded at the top. |

Safe to run at any time; a project with no broken links reports zero
and writes nothing.

## How it decides

**By filename, not by similarity.** Every document filename in this
project is unique — which is what the numbered-filename rule buys — so
a broken link is not ambiguous about *which* document it wanted, only
about how to get there. It finds the one file with that name and
computes the relative path from the linking document's directory.

Two cases are reported and left alone rather than repaired:

- a name matching **no** file in the project, which means the document
  was renamed or never written. A tool that picked a near-match would
  quietly point a reader at the wrong document.
- a name matching **more than one** file, which cannot happen while
  the numbering rule holds, and is worth hearing about immediately if
  it ever does.

Anchors (`#section`), external links, and the generated site under
`docs/HTML/` are untouched — that last because `make html` rebuilds it
from these very files.

## What it found the first time it ran

107 dead links across 54 files, nearly all of them in
`issues/completed/`: issues moved into the completed record over the
project's history whose outbound links were never repaired. The
completed issues are the project's stated blueprint for rebuilding
itself, and a blueprint whose cross-references lead nowhere is a
blueprint somebody has to reconstruct by hand.
