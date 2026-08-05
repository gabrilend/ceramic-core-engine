/*
 * 060-demo-scene.h — how a phase demo talks to a person.
 *
 * What this is: the presentation half of every compiled demo. A scene
 * measures something and hands the numbers here; this decides how they
 * are worded, coloured, laid out, paged, and mirrored to the report
 * file. Nothing in here knows what a station is.
 *
 * How it does it, in general terms: a demo is a sequence of pages, one
 * per scene. Each page states its problem in the engine's own terms,
 * offers an image to hold it by, tabulates the correspondence between
 * the two, justifies every pairing in that table, shows its
 * measurements, and ends with a finding. Then it waits for a keypress
 * and erases itself, so the reader always faces one scene rather than
 * a transcript.
 *
 * Why the shape: the demos used to open on their measurements, which
 * meant a reader met the number before the question it answered. They
 * then acquired stories, which helped and left the join between story
 * and mechanism implicit — a table of "a pigeonhole // a ring cell"
 * with no statement of *why* those are the same thing. The missing
 * clause is the whole value of an analogy, so it is now required by
 * the interface: there is no way to name a correspondence here without
 * also saying what makes it one.
 *
 * See issue 707 for the contract a scene has to satisfy.
 */
#ifndef SORA_DEMO_SCENE_H
#define SORA_DEMO_SCENE_H

#include <stdio.h>

/* {{{ opening and closing a demo */

/* Opens the report under the project root's shared-memory tier, prints
 * the banner, fixes the run's seed, and puts the terminal into the
 * mode the pager needs.
 *
 * The report is not optional. A demo that cannot open it stops here,
 * naming the path, because every demo ends by telling the reader where
 * its evidence was mirrored — and saying that while having failed to
 * mirror it is worse than not running at all. */
void demo_open(const char *project_root,
               const char *report_name,
               const char *title);

/* Joins a report a shell-driven demo has already started, appending
 * rather than truncating, and printing no banner of its own. Phases 3
 * and 6 are demonstrated mostly from the shell but have scenes that
 * need a running engine; those scenes belong to the same document. */
void demo_join(const char *project_root, const char *report_name);

/* A wrapped paragraph at the level of the whole demo rather than a
 * scene — printed under the banner, before the first scene opens.
 * Chiefly for warning a reader about output that will appear and did
 * not come from the demo. */
void demo_note(const char *paragraph);

/* Ends the current page — waiting for the reader, then erasing it —
 * without opening a scene.
 *
 * Needed where a demo is handed between two programs: phases 3 and 6
 * are driven from the shell and call a compiled half partway through,
 * and whichever side is about to stop printing has to settle its own
 * page first. Otherwise the other side's pager erases a count it did
 * not write, and takes the wrong lines. */
void demo_page_break(void);

/* Erases the last page, prints the closing line, restores the
 * terminal, and closes the report. */
void demo_close(const char *closing);

/* }}} */

/* {{{ the shape of one scene */

/* Ends the previous page — waiting for the reader, then erasing it —
 * and prints this scene's heading. */
void scene_open(int number, const char *heading);

/* The problem this scene exists to show, in the engine's own
 * vocabulary. No analogy: a reader who already knows the engine should
 * be able to read this and skip the rest of the page. */
void scene_problem(const char *paragraph);

/* The image to hold the problem by. The presenter supplies the framing
 * sentence; the caller supplies only the picture. */
void scene_imagine(const char *paragraph);

/* One correspondence: the engine's term, the story's counterpart, and
 * why they are the same shape.
 *
 * The third argument is the point of the whole interface. "A ring cell
 * // a pigeonhole" asserts a similarity and leaves the reader to guess
 * at it; "because each holds exactly one item, is addressed by
 * position, and does not care what is in its neighbours" is the thing
 * actually worth reading. Write it as a clause that completes "X is
 * like Y because ...".
 *
 * Calls accumulate. They are rendered when the next kind of block
 * arrives: first as a two-column table for pattern-matching, then as
 * one justified sentence per row. */
void scene_stands_for(const char *engine_term,
                      const char *story_thing,
                      const char *because);

/* One measured row: what was counted, the figure in the story's units,
 * and the same figure in the engine's own. Pass NULL for `engine` when
 * the story's unit *is* the engine's unit and repeating it would only
 * add noise.
 *
 * Both figures must come from the run. The story's unit is a relabel
 * of a measurement, never a second calculation performed for effect. */
void scene_measured(const char *label, const char *story, const char *engine);

/* The finding: what the rows above mean, wrapped like the prose. */
void scene_finding(const char *paragraph);

/* An unstructured line, indented and mirrored, for evidence that is a
 * drawing or a listing rather than a row. Trailing whitespace is
 * trimmed. */
void scene_line(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

/* A blank separator. Its own call rather than an empty scene_line(),
 * which would print an indent and leave trailing whitespace in the
 * mirrored report where nobody would ever see it to remove it. */
void scene_blank(void);

/* }}} */

/* {{{ live panels the reader can steer */

/* A scene whose evidence is a process rather than a total runs a live
 * panel: a frame redrawn on a timer, with keys the reader can press to
 * change the conditions while it runs.
 *
 * Frames are screen-only. Mirroring thousands of them would drown the
 * report in redraws, so the panel writes nothing and the scene reports
 * its outcome afterwards in the ordinary way.
 *
 * Shape of use:
 *
 *     scene_live_begin();
 *     scene_live_key('1', "add a hundred tasks");
 *     scene_live_key('2', "run the workers faster");
 *     while (!scene_live_done()) {
 *         switch (scene_live_pressed()) { ... }
 *         scene_frame_begin();
 *         scene_bar(...);
 *         scene_frame_end();
 *     }
 *     scene_live_end();
 */
void scene_live_begin(void);

/* Declares a key the panel accepts, for the legend. Call between
 * scene_live_begin() and the first frame. */
void scene_live_key(char key, const char *what_it_does);

/* Non-blocking. Returns the key pressed since the last call, or 0.
 * Space and q are handled by the presenter and never returned: space
 * ends the panel, q ends the demo. */
int scene_live_pressed(void);

/* True once the reader has pressed space, or once the panel's own
 * caller has called scene_live_finish(). */
int scene_live_done(void);

/* Ends the panel from the demo's side — for a panel that is finished
 * because the work is finished, rather than because the reader said
 * so. The last frame stays on screen and the legend is replaced by a
 * note saying the work is done. */
void scene_live_finish(void);

/* Erases the previous frame and starts a new one. */
void scene_frame_begin(void);

/* Ends the frame and sleeps until the next one is due. The interval is
 * the panel's own, changed by whatever key the demo wired to it. */
void scene_frame_end(void);

/* One horizontal bar inside a frame. `fraction` is clamped to 0..1;
 * `value_text` is printed after the bar, and is where the real number
 * goes. Colour follows how full the bar is, because a bar that is
 * nearly full usually means something different from an empty one. */
void scene_bar(const char *label, double fraction, const char *value_text);

/* A line inside a frame that is not a bar — a count, a state, a note.
 * Screen only, like everything else in a frame. */
void scene_frame_line(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

/* A blank line inside a frame. Its own call because an empty format
 * string is a compile error under the warning settings this project
 * builds with, and rightly so. */
void scene_frame_blank(void);

/* Leaves the panel, keeping its final frame on screen as part of the
 * page, and mirrors that final frame to the report so the document has
 * a picture of where things ended up. */
void scene_live_end(void);

/* }}} */

/* {{{ formatting, randomness, and quoting the engine */

/* Formats into one of a small ring of buffers and returns it, so a
 * call to scene_measured() can build both of its figures inline.
 *
 * Eight are live at once and the ninth overwrites the first. Each slot
 * holds a whole paragraph, because findings are built here too. */
const char *scene_text(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

/* The seed this run drew its varying values from, printed in the
 * banner. Set SORA_DEMO_SEED in the environment to repeat a run. */
unsigned scene_seed(void);

/* A value in [low, high]. Used where the mechanic under demonstration
 * does not depend on the exact number, which is most places: a figure
 * that is identical on every run invites the suspicion that it was
 * written rather than measured. Where the mechanic *does* depend on
 * the value, the demo names the value and does not draw it. */
int scene_pick(int low, int high);

/* The two streams everything here is written to.
 *
 * Exposed because the engine's own reporting functions take a FILE*
 * and do their own printing, so a demo that wants to show one of those
 * reports verbatim calls it once per stream. Quoting the engine beats
 * paraphrasing it — a scene that retypes what a report says can be
 * wrong about it, and this way it cannot.
 *
 * Neither stream carries colour: the engine's output is its own, and
 * the report is plain text on purpose. */
FILE *scene_screen_stream(void);
FILE *scene_report_stream(void);

/* True when the demo is driving a terminal a person is watching.
 * False when its output is being piped or redirected, in which case
 * there is no paging, no colour and no waiting — a demo run into a
 * file has nobody to press the key. */
int scene_interactive(void);

/* }}} */

#endif /* SORA_DEMO_SCENE_H */
