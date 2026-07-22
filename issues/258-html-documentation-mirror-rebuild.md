# 258 — HTML documentation mirror: regenerate from current markdown

## Status

open · the stale pre-phase-3 pages under `docs/HTML/` were removed
in the change that opened this issue (git history preserves them);
this issue builds their replacement as a generated artifact.

## Current behavior

There is no browsable HTML view of the documentation. An earlier
era shipped seven handcrafted pages (architecture, roadmap, driver
system, IPC and threading, language specs, a task-slab-allocator
page, and an index, with shared style and nav assets) under
`docs/HTML/`. They described the pre-phase-3 shape of the project,
were referenced by no current document, and had drifted far enough
from the markdown docs to be actively misleading, so they were
removed. The markdown documentation (docs/, notes/, the per-file
info.md companions) is current but readable only as plain files.

## Intended behavior

Every documentation file — docs/, notes/, and the info.md
companions — is reachable as a styled HTML page with a
table-of-contents sidebar on the left, so every page can reach
every other page. Cross-references are links: issue numbers click
through to the issue, file mentions click through to that file's
info.md page. Code blocks carry syntax highlighting. The pages
share one aesthetic consistent with the project's editor, with
room for charts and interactive toys where a document benefits.

Per the project rule that nothing is created manually: the pages
are the output of a generator tool, regenerated whenever the
markdown changes — never hand-edited, so they can never drift the
way the removed pages did.

## Suggested implementation steps

1. Write a generator script (`scripts/build-docs-html.sh` shape:
   hard-coded ${DIR}, override argument) that walks docs/, notes/,
   and `**/*.info.md`, converting markdown to HTML through one
   shared page template.
2. Derive the sidebar from `docs/000-table-of-contents.md` so the
   TOC stays the single source of navigation truth.
3. Rewrite intra-doc links during generation: relative markdown
   links become page links; `issue NNN` and bare issue-number
   mentions become links into the rendered issue files.
4. Add syntax highlighting for fenced code blocks (a small
   client-side highlighter keeps the generator dependency-free).
5. Emit into `docs/HTML/` and add a make target; regenerating on
   documentation change becomes part of the doc-editing habit.
6. Seed the page style from the editor's aesthetic so the two
   surfaces read as one project.

## Related tools / files

- `docs/000-table-of-contents.md` — navigation source of truth
- `assets/` — the editor whose aesthetic the pages should share
- git history of `docs/HTML/` — the removed handcrafted
  predecessor pages, for reference on what the style attempted
