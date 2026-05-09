# 225 — Directory picker: show files alongside dirs, distinguish them visually

## Status
complete

## Implementation notes

Server-side: `handle_list_dirs` in `src/005-http-server.lua` now
returns `{ path, dirs, files }` — files filtered to the small phase 2
allowlist (`.lua`, `.c`, `.sh`) via `find -maxdepth 1`. Phase 3 will
swap the hard-coded list for whatever the language specs declare.

Client-side:
- `assets/js/007-filebrowser.js::show_dir_picker` renders dirs and
  files separately. Dirs use `.fb-dir-row` (accent blue, `>` prefix,
  click navigates); files use `.fb-file-readonly` (dim gray, `·`
  prefix, no hover, no pointer cursor).
- The parent-dir `..` row also gets the dir-row treatment so it
  matches the look of other navigation targets.
- The main file list (`.fb-file-row`) gets the same `·` glyph
  prefix for visual consistency between the picker and the post-pick
  view; the click handler still uses the closed-over `filename`
  variable so the cosmetic prefix doesn't leak into the lookup.
- New CSS classes added in `assets/index.html`.

Empty-directory message expanded to "(empty directory)" — covers
both no-subdirs and no-files cases.

## Current behavior

When the user clicks "+ library dir" in the file browser, the
directory picker (`show_dir_picker` in `assets/js/007-filebrowser.js`)
fetches the directory contents via `API.list_dirs(path)`, which
returns `{ path, dirs }`. Only the subdirectories are rendered; any
source files in the current directory are invisible.

This makes navigation harder than it should be — there's no visible
confirmation that you've reached the right directory until you click
"use this directory" and the file list re-renders.

The list itself uses the `.fb-file-row` class for both dirs and the
parent-directory `..` row, so dirs and the (currently absent) files
would render identically. There's no visual cue distinguishing
"this is a directory I can enter" from "this is a file I'd see if I
picked this directory."

## Intended behavior

The directory picker shows both subdirectories and source files in
the current directory.

- **Directories**: clickable, navigate into them on click, render
  with a visual marker (icon, prefix, color, underline, or
  combination — see "Visual differentiation" below).
- **Files**: shown as a read-only list — they can't be picked
  individually here (this is a directory picker), but their
  presence tells the user what they'd be importing if they picked
  this directory.

### Visual differentiation: dir vs file

Three approaches; pick one or layer them.

1. **Icon prefix** — a small character before the name. `📁` for
   dirs and `📄` for files, or pure-text alternatives like `▸ /`
   and `· `. Friendly to colorblind users.
2. **Color** — directories in the editor's blue accent
   (`#4a9eff`), files in dim gray (`#6c72a0`). Easy to do but
   inaccessible alone.
3. **Underline** — directories underlined to suggest "clickable
   link". Already a familiar web idiom. Pairs well with color.

Recommendation: layer (1) and (2) — icon prefix for accessibility,
color for at-a-glance identification. The icon doesn't have to be
emoji; ASCII glyphs like `>` for dirs and `·` for files are
unambiguous and font-portable.

### Implementation notes

The server endpoint `/fs/dirs` currently returns `{ path, dirs }`.
Either:

- Extend the response to `{ path, dirs, files }`, where `files` is
  an array of source-file basenames in the current directory. The
  server filters by extension to avoid listing every file (only
  show `.lua`, `.c`, `.sh`, etc., matching what the language specs
  declare).
- Or add a sibling endpoint `/fs/files?path=…` and call both in
  parallel from the picker.

Extending the existing endpoint is simpler. The filter
(file-extensions to include) lives server-side and references
`langs/*/spec.so`'s `file_ext` field at startup once phase 3
ships; in phase 2 we hard-code the current set
(`.lua`, `.c`, `.sh`).

## Suggested implementation sequence

1. `src/006-server-main.lua` — extend the `/fs/dirs` handler to also
   return matching source files. Filter by a small allowlist of
   extensions for now.
2. `assets/js/003-api.js::list_dirs` — already returns the response
   verbatim, so just consume the new `files` field.
3. `assets/js/007-filebrowser.js::show_dir_picker` — render files
   below the directory list, in dim color, non-clickable.
4. `assets/index.html` — add CSS classes `.fb-dir-row` and
   `.fb-file-row-readonly` (or reuse the existing `.fb-file-row`
   with a modifier class). Icon prefix added via a `::before`
   pseudo-element or a leading span.
5. The same icon/color treatment optionally applies to the main
   file list (`render` function), which currently uses
   `.fb-file-row` for clickable file entries — they'd get the file
   icon + dim color while keeping their click behavior.

## Open questions

- Should files in the dir picker be clickable to "preview" their
  contents (open the source viewer from issue 215)? Probably not —
  it adds a click target where there shouldn't be one. The user is
  picking a directory, not a file. Defer.
- Which icon set: ASCII (`>`, `·`) or unicode glyphs (`▸`, `·`) or
  emoji (`📁`, `📄`)? ASCII / unicode for portability; emoji
  rendering varies wildly across systems.
- Should the file list hide when collapsed (collapsible group
  header)? Probably not necessary for the picker — the file list
  is informational, and short.

## Relevant files

- `assets/js/007-filebrowser.js` — `show_dir_picker`, file rendering
- `src/006-server-main.lua` — `/fs/dirs` handler
- `assets/js/003-api.js` — `list_dirs` API call
- `assets/index.html` — CSS for `.fb-file-row` etc.
- `issues/completed/207-source-file-browser-and-port-population.md`
  — original file browser issue
