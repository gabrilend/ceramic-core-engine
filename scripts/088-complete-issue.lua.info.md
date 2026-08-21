# 088-complete-issue.lua — finishing an issue, from outside

Moves one or more issue files from `issues/` into `issues/completed/`
and leaves the written record walkable.

## Usage

    luajit scripts/088-complete-issue.lua <issue> [more...] [--apply] [DIR]

| Argument | Meaning |
|---|---|
| `<issue>` | The file, with or without `.md`. A bare prefix like `210g` is enough when exactly one open issue starts with it. |
| `--apply` | Actually move and repair. Without it, nothing changes and the report says what would have happened. |
| `DIR` | Project root, if not the one hard-coded at the top. |

Several issues can be named at once, which is the case worth having:
a parent and its last child finish together, and moving them one at a
time would repoint each one's links to the other as though the other
had stayed put.

## What it does

1. **Resolves each name to one file**, and stops if a name matches
   none or more than one. Guessing which issue somebody meant to
   finish is the one mistake it must never make.
2. **Refuses to overwrite** a file already in `completed/`. That means
   somebody has done this already, and doing it twice destroys the
   version carrying the record.
3. **Moves through `git mv`**, so the history holds both paths rather
   than a delete and an unrelated add — which is the project's rule
   about never moving a directory or file without both versions being
   tracked.
4. **Runs the link repairer** (`089-repair-doc-links.lua`) over the
   whole project, which is what fixes the links in both directions.

## What it does not do

It does not work out where a link should point. That is one problem
with one right answer and the repairer answers it for every link in
the project, not only for the ones a move happened to break. This file
used to contain a second implementation of that; the two disagreed the
first time a batch of issues moved together.
