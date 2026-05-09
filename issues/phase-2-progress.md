# Phase 2 Progress

Goal: an editor you can actually use to build maps, plus the graph
model the phase 3 runtime will consume.

## Issues

| ID   | Title                                          | Status   |
|------|------------------------------------------------|----------|
| 201  | fix box rendering closure bug                  | complete |
| 202  | fit-to-view on map load                        | complete |
| 203  | map picker panel                               | complete |
| 204  | visible interaction hints                      | complete |
| 206  | entry box designation                          | won't implement |
| 207  | source file browser and port auto-population   | complete |
| 208  | port literal values                            | complete |
| 209  | ollama query library                           | complete |
| 210  | comparator wire branching                      | complete |
| 211  | file browser library directories               | complete |
| 211a | unify src/ into same directory pipeline        | complete |
| 212  | editor interaction modes                       | complete |
| 213  | queued inputs and task model                   | won't implement |
| 214  | tmp symlink recreation on reboot               | complete |
| 215  | view source button                             | complete |
| 223  | syntax highlighting via user-written lexers    | open     |
| 224  | editable port names and canvas-side value edit | open     |
| 225  | dir picker shows files and dir/file icons      | open     |
| 226  | hide output controls for no-return functions   | open     |
| 227  | hide view button when box has no ref           | open     |
| 216  | read-file box                                  | open     |
| 217  | concat box and dynamic inputs                  | complete |
| 218  | enforce single-output driver contract          | complete |
| 219  | map compiler                                   | open     |
| 220  | root run script and entry point cleanup        | complete |
| 221  | iterator box                                   | open     |
| 222  | compile button and assets directory            | complete |

## Phase goal checklist

- [x] Boxes render on map load
- [x] Clicking a box opens the inspector
- [x] Ref/fn set by browsing src/ files; ports auto-derived
- [x] Map loads centered on screen (fit-to-view)
- [x] Map can be switched from a list, not a text prompt
- [x] Empty canvas shows interaction hints
- [—] Entry box marker (won't implement — phase 3 auto-detects, see 305)
- [ ] Comparator routing replaces the old branch box (issue 210)
- [ ] Iterator box with auto-grow output slots (issue 221)
- [x] Compile button (issue 222) — placeholder UI for the phase 3 build

## Notes on graph model changes during phase 2

These are not new features so much as model corrections that shape
what phase 3 will consume:

- **Single-output rule** (218, complete): every box has exactly one
  output wire. The driver contract emits one JSON value, not an
  array.
- **Comparator-based branching** (210, complete): the old `branch`
  box kind with named ports is removed. A box may carry a
  `comparand`; the dispatch layer fires the connection whose
  `from_branch` matches `lt` / `eq` / `gt`.
- **Iterator routing** (221): a box with `iterator_outputs` is a
  pure routing primitive; the dispatch layer rotates through the
  declared output names. Iterators have no `ref` / `fn`.
- **Connection schema**: `from_branch` field replaces the old
  `from_output` / `from_port` dual-field design.
