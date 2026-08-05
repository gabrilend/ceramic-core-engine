# 062-demo-scene.sh — the shell-driven demos' voice

The shell counterpart of `060-demo-scene.h`. Phases 3 and 6 are
demonstrated from the shell, because what they prove is about builds
and files rather than about running code — a generator being rerun, a
text file being edited between runs. They still have to read like the
other five.

Same block shapes, same column widths, same wrap width as the compiled
presenter. If one moves, both move: two demos side by side in one
terminal with different margins look like two projects.

Sourced, not run. The contract a scene's story has to satisfy is issue
707.

## Functions

| Function | Takes | Does |
|---|---|---|
| `sora_demo_open` | report path, title | Starts the report fresh, decides whether anybody is watching, arms the cursor traps, prints the banner. Exits if the report cannot be written. |
| `sora_demo_note` | a paragraph | A wrapped paragraph belonging to the whole demo rather than a scene. |
| `sora_demo_page_break` | nothing | Ends the current page without opening a scene, for handing the demo to the compiled half and back. |
| `sora_scene_open` | scene number, heading | Ends the previous page — waiting, then erasing it — and prints this scene's heading. |
| `sora_scene_problem` | a paragraph | The problem in the engine's own vocabulary, no analogy. |
| `sora_scene_imagine` | a paragraph | The image. The framing sentence is added here, so it matches the compiled presenter word for word. |
| `sora_scene_stands_for` | engine term, story thing, **because** | One correspondence. Accumulates; rendered as a table and then as justified sentences when the next block arrives. |
| `sora_scene_measured` | label, story-units figure, optional engine-units figure | One measured row; two columns when the third argument is absent or empty. |
| `sora_scene_line` | one line of text | An indented line, for a listing rather than a row. |
| `sora_scene_quote` | reads standard input | Several lines of something the demo did not write — a file, a command's output — indented as one block and mirrored. Blank lines stay blank rather than becoming four spaces. |
| `sora_scene_finding` | a paragraph | What the rows above mean. |
| `sora_scene_blank` | nothing | One blank separator. |
| `sora_demo_close` | closing line | Erases the last page, prints the closing line, gives the cursor back. |

## Two details worth knowing before editing

**Paragraphs are flattened before they are folded.** Stories are
written across several source lines for the sake of whoever reads the
script, and `fold` treats every input line as its own paragraph.
Without flattening, a story appears on screen broken wherever it
happened to be broken in the source — which looks like a wrapping bug
and is one.

**Trailing spaces are stripped.** `fold` leaves them; they are
invisible on a terminal and permanent in the mirrored report, which is
the worst combination — nobody sees them, so nobody removes them.

## Sharing one report with a compiled half

The phase 3 and phase 7 demos have compiled scenes in the middle. The
shell opens the report here; the compiled program calls `demo_join`
from `060-demo-scene.h`, which appends rather than truncating and
prints no banner of its own. The shell closes the document afterwards.
