# 060-demo-scene.h — the compiled demos' voice

The presentation half of every compiled phase demo. A scene measures
something and hands the numbers here; this decides how they are
worded, laid out, and mirrored to the report file. Nothing in it knows
what a station is.

Implemented by `061-demo-scene.c`, which is compiled into every demo by
the build front in `059-demo-engine.sh`. The shell-driven demos use
`062-demo-scene.sh`, which offers the same shapes so the two families
look identical on screen.

The contract a scene's story has to satisfy is issue 707.

## Opening and closing

| Function | Takes | Gives | Does |
|---|---|---|---|
| `demo_open` | project root, report filename, title (all `const char *`) | nothing | Opens the report under the shared-memory tier, prints the banner, fixes the run's seed, and puts the terminal into the mode the pager needs. Exits if the report cannot be opened — a demo that promises a mirrored report has to keep the promise. |
| `demo_join` | project root, report filename | nothing | Appends to a report a shell-driven demo already opened, printing no banner. Used by the compiled halves of the phase 3 and phase 7 demos, whose scenes are the middle of a longer document. |
| `demo_note` | one paragraph (`const char *`) | nothing | A wrapped paragraph belonging to the whole demo rather than a scene. Chiefly for warning a reader about output that will appear and did not come from the demo. |
| `demo_page_break` | nothing | nothing | Ends the current page without opening a scene. Needed where a demo is handed between a shell half and a compiled half: whichever side is about to stop printing settles its own page, so the other's pager never erases a count it did not write. |
| `demo_close` | closing line (`const char *`) | nothing | Erases the last page, prints the closing line, restores the terminal, closes the report. |

## The shape of one scene

Called in this order. Blank lines between the blocks are inserted by
the presenter, never by the caller.

| Function | Takes | Does |
|---|---|---|
| `scene_open` | scene number (`int`), heading (`const char *`) | Ends the previous page — waiting for the reader, then erasing it — and prints this scene's heading. |
| `scene_problem` | one paragraph (`const char *`) | The problem, in the engine's own vocabulary. No analogy: a reader who already knows the engine should be able to stop here. |
| `scene_imagine` | one paragraph (`const char *`) | The image to hold it by. The presenter supplies the framing sentence ("To picture it more easily, imagine …"); the caller supplies only the picture, so the hinge is worded identically on all thirty-five pages. |
| `scene_stands_for` | engine term, story thing, **because** (all `const char *`) | One correspondence. Calls accumulate and render when the next kind of block arrives: first as a two-column table to pattern-match against, then as one justified sentence each. The third argument is the point of the interface — write it as a clause completing "X is like Y because …". At most eight per scene; a ninth is an error, because a scene naming more than eight correspondences is a scene doing two jobs. |
| `scene_measured` | label, figure in story units, figure in engine units (all `const char *`; the third may be `NULL`) | One measured row. `NULL` for the third gives a two-column row, for when the story's unit is the engine's unit. |
| `scene_line` | printf-style format and arguments | An indented line, for evidence that is a drawing or a listing rather than a row. Trailing whitespace is trimmed. |
| `scene_blank` | nothing | One blank separator, and forgets what block came before so nothing doubles it. |
| `scene_finding` | one paragraph (`const char *`) | What the rows above mean, wrapped like the prose. |

## Live panels the reader steers

For a mechanic that is a process rather than a total. Frames are
screen-only — mirroring a redraw loop would bury the report under
thousands of near-identical pictures — so a live scene still ends with
ordinary `scene_measured` rows, and those are what the document keeps.

| Function | Takes | Gives | Does |
|---|---|---|---|
| `scene_live_begin` | nothing | nothing | Starts the panel. |
| `scene_live_key` | key (`char`), what it does (`const char *`) | nothing | Declares a key for the legend, which is redrawn with every frame so it cannot scroll away from what it describes. Six at most. |
| `scene_live_pressed` | nothing | `int` | Non-blocking. The key pressed since the last call, or 0. Space and q never come back: space ends the panel, q ends the demo. |
| `scene_live_done` | nothing | `int` | True once the reader has pressed space. Always true immediately when nobody is watching, so a redirected run draws one frame and moves on. |
| `scene_live_finish` | nothing | nothing | Ends the panel from the demo's side, for work that has finished on its own. |
| `scene_frame_begin` | nothing | nothing | Erases the previous frame and starts a new one. |
| `scene_frame_end` | nothing | nothing | Ends the frame, draws the key legend, and sleeps until the next is due. |
| `scene_bar` | label, fraction (`double`, clamped 0..1), value text | nothing | One horizontal bar. Colour follows how full it is, because a nearly full bar usually means something different from an empty one. |
| `scene_frame_line` | printf-style format and arguments | nothing | A non-bar line inside a frame. |
| `scene_frame_blank` | nothing | nothing | A blank line inside a frame. Its own call because an empty format string is a compile error under this project's warning settings. |
| `scene_live_end` | nothing | nothing | Leaves the panel, keeping its final frame on screen as part of the page. |

Two more are declared by the demos that want them rather than in the
header, since they exist only to be wired to a key: `scene_live_faster`
and `scene_live_slower` halve and double the frame interval, clamped.

## Formatting, randomness, and quoting the engine

| Function | Takes | Gives | Does |
|---|---|---|---|
| `scene_text` | printf-style format and arguments | `const char *` | Formats into one of eight rotating buffers, so a call to `scene_measured` can build both its figures inline. The ninth call overwrites the first. |
| `scene_seed` | nothing | `unsigned` | The seed this run drew its varying values from, printed in the banner. Set `SORA_DEMO_SEED` in the environment to repeat a run. |
| `scene_pick` | low, high (`int`, inclusive) | `int` | A value in the range. Exits if the range is empty, rather than quietly returning the low end. |
| `scene_screen_stream` | nothing | `FILE *` | Standard output. |
| `scene_report_stream` | nothing | `FILE *` | The open report file. |
| `scene_interactive` | nothing | `int` | True when the demo is driving a terminal somebody is watching. False when piped or redirected, in which case there is no paging, no colour and no waiting — a demo run into a file has nobody to press the key. |

The two streams are exposed for one purpose: the engine's own reporting
functions take a `FILE *` and print for themselves, so a demo that
wants to show one of those reports verbatim calls it once per stream.
Quoting the engine beats paraphrasing it — a scene that retypes what a
report says can be wrong about it.

The engine's teardown alarm writes to standard error on its own
initiative and is not mirrored anywhere. That is correct; a demo that
expects one should say so, which is what `demo_note` is for.
