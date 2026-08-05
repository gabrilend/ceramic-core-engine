/*
 * 061-demo-scene.c — the presenter behind 060-demo-scene.h.
 *
 * What this is: the only code in the project that decides what a demo
 * looks like on a terminal. Column widths, wrap width, indents, the
 * colour palette, the order of blocks within a scene, and the paging
 * behaviour all live here and nowhere else.
 *
 * How it does it, in general terms: everything printed goes through
 * one emit function that writes to the screen with colour and to the
 * report without it, so the mirrored copy is plain text and cannot
 * drift from what was seen. That same function counts the screen rows
 * it produced, which is what lets a finished scene erase exactly
 * itself and no more.
 *
 * The terminal work: canonical mode and echo are switched off so a
 * single keypress advances a page, while ISIG is deliberately left on
 * so ctrl-c still raises SIGINT rather than arriving as a byte nobody
 * checks for. A handler and an atexit hook both restore the terminal,
 * because a demo that dies owing the shell its settings leaves a
 * person typing blind.
 */
#include "060-demo-scene.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* The page. Seventy-two columns because these reports are read in a
 * terminal beside the map files that produced them, and both want to
 * sit inside eighty without wrapping at the mercy of the window. */
#define SCENE_WIDTH        72
#define SCENE_INDENT       "  "
#define SCENE_ROW_INDENT   "    "

/* The measured row's columns: what was counted, the story's figure,
 * the engine's. A label longer than its column pushes the two figure
 * columns out of alignment for that row only, so these are sized for
 * the longest label any demo actually uses rather than for comfort. */
#define SCENE_LABEL_WIDTH  24
#define SCENE_STORY_WIDTH  22

/* The correspondence table's two columns. */
#define SCENE_TERM_WIDTH   28

/* {{{ the palette */
/*
 * Colour carries one meaning each, consistently across all seven
 * demos, because a reader who has to relearn the scheme per scene is
 * worse off than one who was given no colour at all.
 *
 *   heading   the scene's name and number
 *   engine    anything in the engine's vocabulary
 *   story     anything in the analogy's vocabulary
 *   figure    a measured number
 *   quiet     framing text that is structure rather than content
 *   key       something the reader can press
 */
#define C_RESET    "\033[0m"
#define C_HEADING  "\033[1;36m"
#define C_ENGINE   "\033[33m"
#define C_STORY    "\033[32m"
#define C_FIGURE   "\033[1;37m"
#define C_QUIET    "\033[2m"
#define C_KEY      "\033[1;35m"
#define C_ALERT    "\033[31m"
/* }}} */

static FILE *report;
static unsigned seed_value;
static unsigned random_state;

static int interactive;
static int terminal_width = 80;
static int page_rows;          /* screen rows written since the page began */
static int frame_rows;         /* screen rows in the live frame being drawn */
static int in_frame;           /* inside scene_frame_begin/end */
static int live_active;
static int live_finished;
static int live_interval_us = 120000;

static struct termios saved_modes;
static int modes_saved;

/* Which kind of block was printed last. The presenter puts the blank
 * lines between blocks itself, because leaving it to the demos meant
 * every demo deciding separately how a scene is spaced — and they
 * would not have agreed for long. */
enum {
    BLOCK_NONE = 0,
    BLOCK_HEADING,
    BLOCK_PROSE,
    BLOCK_IMAGINE,
    BLOCK_TABLE,
    BLOCK_MEASURED,
    BLOCK_LINE,
    BLOCK_FINDING
};
static int last_block;

/* The correspondences named so far in this scene, held until something
 * else needs printing. They are rendered twice — once as a table to
 * pattern-match against, once as justified sentences — so they cannot
 * be printed as they arrive. */
enum { MAPPING_MAX = 8 };
static struct {
    const char *engine_term;
    const char *story_thing;
    const char *because;
} mapping[MAPPING_MAX];
static int mapping_count;

/* The live panel's key legend. */
enum { LIVE_KEYS_MAX = 6 };
static struct {
    char        key;
    const char *what;
} live_keys[LIVE_KEYS_MAX];
static int live_key_count;

static void flush_mapping(void);

/* {{{ visible_rows() */
/* How many screen rows a line of the given printed length occupies.
 *
 * Erasing a page means moving the cursor up by the number of rows the
 * page actually filled, which is not the number of lines printed: a
 * line longer than the window wraps and costs two. Counting in
 * characters and dividing by the window's width is the only way to get
 * this right in a window narrower than the text. */
static int visible_rows(size_t length)
{
    if (length == 0)
        return 1;
    return (int)((length + (size_t)terminal_width - 1) / (size_t)terminal_width);
}
/* }}} */

/* {{{ emit() */
/*
 * Every character this file prints passes through here.
 *
 * Screen and report are written from one place so a report can never
 * be a different document from the run that produced it — and the
 * report never receives colour, because it is a text file somebody may
 * grep in six months.
 *
 * `colour` may be NULL for unhighlighted text. The row count is taken
 * from the plain text, so escape sequences never inflate it.
 */
static void emit(const char *colour, const char *format, ...)
{
    char text[2048];
    va_list args;
    size_t length;

    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);

    length = strlen(text);

    if (colour && interactive)
        printf("%s%s%s", colour, text, C_RESET);
    else
        printf("%s", text);

    /* A frame is screen-only: mirroring a redraw loop would bury the
     * report under thousands of near-identical pictures. */
    if (report && !in_frame)
        fputs(text, report);

    /* Newlines inside one emit are counted, since the callers below do
     * sometimes print a trailing one separately. */
    {
        size_t line_start = 0;
        size_t i;
        int rows = 0;
        for (i = 0; i < length; i++)
            if (text[i] == '\n') {
                rows += visible_rows(i - line_start);
                line_start = i + 1;
            }
        if (line_start < length)
            rows += visible_rows(length - line_start) - 1;
        if (in_frame)
            frame_rows += rows;
        else
            page_rows += rows;
    }

    fflush(stdout);
}
/* }}} */

/* {{{ erase_rows() */
/* Walks the cursor up, clearing each line as it goes, and leaves it at
 * the start of the first line erased. Used to take back a finished
 * page or a superseded frame — never the whole screen, because
 * whatever the reader had above the demo is theirs. */
static void erase_rows(int rows)
{
    int i;

    if (!interactive || rows <= 0)
        return;

    for (i = 0; i < rows; i++)
        printf("\033[1A\033[2K");
    printf("\r");
    fflush(stdout);
}
/* }}} */

/* {{{ terminal_restore() */
static void terminal_restore(void)
{
    /* Registered with atexit and also called explicitly on the way
     * out, so it has to be safe twice. Without the guard the cursor
     * request is written a second time after the demo's last line,
     * which lands in whatever the shell prints next. */
    static int restored;

    if (restored)
        return;
    restored = 1;

    if (modes_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_modes);
    if (interactive) {
        printf("\033[?25h");    /* the cursor, back */
        fflush(stdout);
    }
}
/* }}} */

/* {{{ on_interrupt() */
/* ctrl-c during a demo. The terminal is handed back before the process
 * goes, because the alternative is a shell with echo switched off and
 * a person who cannot see what they are typing to fix it. */
static void on_interrupt(int signal_number)
{
    (void)signal_number;
    terminal_restore();
    if (report)
        fclose(report);
    _exit(130);
}
/* }}} */

/* {{{ terminal_setup() */
static void terminal_setup(void)
{
    struct termios modes;
    struct winsize window;

    interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    if (!interactive)
        return;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 && window.ws_col > 20)
        terminal_width = window.ws_col;

    if (tcgetattr(STDIN_FILENO, &modes) != 0) {
        /* A terminal that will not describe itself is not one this can
         * page through. Saying so beats paging blind. */
        fprintf(stderr, "demo: cannot read the terminal's modes; "
                        "running without paging\n");
        interactive = 0;
        return;
    }
    saved_modes = modes;
    modes_saved = 1;

    /* Canonical mode off so a keypress arrives without a return key;
     * echo off so the key does not appear in the middle of a scene.
     * ISIG stays ON deliberately — ctrl-c should remain a signal
     * rather than becoming a byte that some read loop has to remember
     * to check for. */
    modes.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    modes.c_cc[VMIN] = 1;
    modes.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &modes);

    atexit(terminal_restore);
    signal(SIGINT, on_interrupt);
    signal(SIGTERM, on_interrupt);

    printf("\033[?25l");        /* the cursor, hidden while paging */
    fflush(stdout);
}
/* }}} */

/* {{{ read_key() */
/* Blocks for one key. Returns 0 if the input ended. */
static int read_key(void)
{
    unsigned char c;
    ssize_t got;

    for (;;) {
        got = read(STDIN_FILENO, &c, 1);
        if (got == 1)
            return (int)c;
        if (got == 0)
            return 0;
        if (errno == EINTR)
            continue;           /* a window resize, say */
        return 0;
    }
}
/* }}} */

/* {{{ poll_key() */
/* Takes a key if one is waiting, and does not wait for one. */
static int poll_key(void)
{
    struct termios modes;
    unsigned char c;
    ssize_t got;
    int key = 0;

    if (!interactive)
        return 0;

    if (tcgetattr(STDIN_FILENO, &modes) != 0)
        return 0;
    modes.c_cc[VMIN] = 0;
    modes.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &modes);

    got = read(STDIN_FILENO, &c, 1);
    if (got == 1)
        key = (int)c;

    modes.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &modes);
    return key;
}
/* }}} */

/* {{{ page_end() */
/*
 * Ends a page: offers it to the reader, waits, then takes it back.
 *
 * The waiting is what makes a demo a sequence of scenes rather than a
 * transcript that scrolls past faster than anybody reads. The erasing
 * is what keeps the reader facing one scene: everything said is in the
 * report, so nothing is lost by clearing it from the screen.
 *
 * Non-interactive runs skip both. A demo whose output is being
 * redirected has nobody to press the key, and erasing into a file
 * would write escape sequences nobody can read.
 */
static void page_end(void)
{
    int prompt_rows_before;
    int key;

    if (!interactive || page_rows == 0) {
        page_rows = 0;
        return;
    }

    prompt_rows_before = page_rows;

    printf("\n");
    printf("  %s[space]%s next scene   %s[q]%s quit   "
           "%severything here is mirrored to the report%s\n",
           C_KEY, C_RESET, C_KEY, C_RESET, C_QUIET, C_RESET);
    fflush(stdout);
    page_rows += 2;

    for (;;) {
        key = read_key();
        if (key == 'q' || key == 'Q') {
            erase_rows(page_rows);
            page_rows = 0;
            printf("  stopped early; the report holds every scene, "
                   "including the ones not shown\n");
            terminal_restore();
            if (report)
                fclose(report);
            exit(0);
        }
        if (key == ' ' || key == '\r' || key == '\n' || key == 0)
            break;
        /* Any other key: ignore it and keep waiting, rather than
         * advancing on a key the reader hit by accident. */
    }

    erase_rows(page_rows);
    (void)prompt_rows_before;
    page_rows = 0;
    last_block = BLOCK_NONE;
}
/* }}} */

/* {{{ begin_block() */
/* Called at the top of everything that prints inside a scene. Renders
 * any pending correspondence table first, then emits the separating
 * blank line when the kind of thing being printed has changed. */
static void begin_block(int kind)
{
    if (mapping_count && kind != BLOCK_TABLE)
        flush_mapping();

    if (last_block != BLOCK_NONE && last_block != kind)
        emit(NULL, "\n");
    last_block = kind;
}
/* }}} */

/* {{{ emit_wrapped() */
/*
 * Prints a paragraph broken at spaces to fit the page, each line
 * carrying the given indent.
 *
 * Wrapping here rather than in the demo sources is what lets a story
 * be written as one long string: rewording it costs nothing, where
 * hand-broken lines would have to be re-broken by hand every time.
 * A word longer than the page is emitted whole and overhangs, because
 * breaking a word is worse than a ragged edge — and the only words
 * that long are file paths, which must stay copyable.
 */
static void emit_wrapped(const char *colour, const char *indent,
                         const char *text)
{
    size_t room = SCENE_WIDTH - strlen(indent);
    const char *word = text;

    while (*word) {
        size_t used = 0;

        emit(NULL, "%s", indent);
        while (*word) {
            const char *end = strchr(word, ' ');
            size_t length = end ? (size_t)(end - word) : strlen(word);

            /* Past the edge, and something is already on this line:
             * the word starts the next one instead. */
            if (used && used + 1 + length > room)
                break;

            if (used) {
                emit(colour, " ");
                used += 1;
            }
            emit(colour, "%.*s", (int)length, word);
            used += length;

            word += length;
            while (*word == ' ')
                word++;
        }
        emit(NULL, "\n");
    }
}
/* }}} */

/* {{{ flush_mapping() */
/*
 * Renders the correspondences: first the table, then the reasons.
 *
 * Both, and in that order, on purpose. The table exists to be scanned
 * — two columns, aligned, so the shape of the mapping can be taken in
 * at a glance and referred back to while reading the rest of the page.
 * The sentences exist to be believed: an analogy nobody justified is a
 * claim the reader has to take on trust, and the demos are supposed to
 * be arguing rather than asserting.
 */
/* {{{ emit_padded() */
/* Writes a column and the gap after it.
 *
 * printf's %-*s gives no gap at all when the text is as wide as the
 * column, which silently welds two columns together — and the fix
 * cannot be "make the column wider", because there is always a longer
 * term. Two spaces are guaranteed instead, so an over-long entry
 * pushes its row out of alignment rather than becoming unreadable. */
static void emit_padded(const char *colour, const char *text, int width)
{
    int used = (int)strlen(text);
    int gap = (used < width) ? width - used : 2;

    emit(colour, "%s", text);
    emit(NULL, "%*s", gap, "");
}
/* }}} */

static void flush_mapping(void)
{
    int i;
    int pending = mapping_count;

    mapping_count = 0;          /* before emitting, so begin_block below
                                 * cannot recurse back into here */

    if (last_block != BLOCK_NONE)
        emit(NULL, "\n");

    emit(NULL, "%s", SCENE_ROW_INDENT);
    emit_padded(C_QUIET, "in the engine", SCENE_TERM_WIDTH);
    emit(C_QUIET, "in the story\n");
    emit(NULL, "%s", SCENE_ROW_INDENT);
    emit_padded(C_QUIET, "--------------------------", SCENE_TERM_WIDTH);
    emit(C_QUIET, "--------------------------\n");

    for (i = 0; i < pending; i++) {
        emit(NULL, "%s", SCENE_ROW_INDENT);
        emit_padded(C_ENGINE, mapping[i].engine_term, SCENE_TERM_WIDTH);
        emit(C_STORY, "%s", mapping[i].story_thing);
        emit(NULL, "\n");
    }

    for (i = 0; i < pending; i++) {
        char sentence[1024];

        emit(NULL, "\n");
        snprintf(sentence, sizeof sentence, "%s is like %s because %s.",
                 mapping[i].engine_term, mapping[i].story_thing,
                 mapping[i].because);
        /* The demos write their terms in lower case, because that is
         * how the terms read inside the table's columns. A sentence
         * still starts with a capital. */
        if (sentence[0] >= 'a' && sentence[0] <= 'z')
            sentence[0] = (char)(sentence[0] - 'a' + 'A');
        emit_wrapped(NULL, SCENE_INDENT, sentence);
    }

    last_block = BLOCK_TABLE;
}
/* }}} */

/* {{{ next_random() */
/* Xorshift32. Chosen over rand() because the seed printed in the
 * banner has to reproduce the run on any machine, and rand()'s
 * sequence is the C library's business rather than ours. */
static unsigned next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
/* }}} */

/* {{{ seed_from_environment() */
static void seed_from_environment(void)
{
    const char *requested = getenv("SORA_DEMO_SEED");

    /* An explicit seed repeats a run exactly; otherwise the clock and
     * the process id together make runs differ, which is the point —
     * a number that never moves reads like a number that was typed. */
    if (requested)
        seed_value = (unsigned)strtoul(requested, NULL, 10);
    else
        seed_value = (unsigned)time(NULL) ^ ((unsigned)getpid() << 16);

    /* Xorshift is fixed at zero, which would make a zero seed produce
     * nothing but zeroes. Any constant does; this one is arbitrary. */
    random_state = seed_value ? seed_value : 0x9e3779b9u;
}
/* }}} */

/* {{{ open_report() */
static void open_report(const char *project_root, const char *report_name,
                        const char *mode)
{
    char path[4096];

    snprintf(path, sizeof path, "%s/tmp/shared-memory/%s",
             project_root, report_name);
    report = fopen(path, mode);
    if (!report) {
        /* No screen-only mode. The demo's last line promises a
         * mirrored report; keeping that promise is part of running. */
        fprintf(stderr, "demo: cannot write its report to %s\n", path);
        fprintf(stderr, "demo: the shared-memory tier is missing or "
                        "unwritable; the launcher creates it\n");
        exit(1);
    }
}
/* }}} */

/* {{{ demo_open() */
void demo_open(const char *project_root, const char *report_name,
               const char *title)
{
    open_report(project_root, report_name, "w");
    terminal_setup();
    seed_from_environment();

    emit(C_HEADING, "%s\n", title);
    emit(NULL, "%s", SCENE_INDENT);
    emit(C_QUIET, "values vary from run to run; this one drew them from "
                  "seed %u\n", seed_value);
    emit(NULL, "%s", SCENE_INDENT);
    emit(C_QUIET, "set SORA_DEMO_SEED=%u to see this exact run again\n",
         seed_value);

    if (!interactive)
        emit(C_QUIET, "%s(not a terminal: no paging, no colour, "
                      "every scene printed at once)\n", SCENE_INDENT);

    last_block = BLOCK_HEADING;
}
/* }}} */

/* {{{ demo_join() */
void demo_join(const char *project_root, const char *report_name)
{
    open_report(project_root, report_name, "a");
    terminal_setup();
    seed_from_environment();

    /* Said here as well as in demo_open, because these scenes draw
     * values too and a run that cannot be repeated is a run whose
     * numbers nobody can check. */
    emit(NULL, "%s", SCENE_INDENT);
    emit(C_QUIET, "these scenes drew their values from seed %u\n", seed_value);
    last_block = BLOCK_PROSE;
}
/* }}} */

/* {{{ demo_note() */
void demo_note(const char *paragraph)
{
    begin_block(BLOCK_PROSE);
    emit_wrapped(NULL, SCENE_INDENT, paragraph);
}
/* }}} */

/* {{{ demo_page_break() */
void demo_page_break(void)
{
    page_end();
}
/* }}} */

/* {{{ demo_close() */
void demo_close(const char *closing)
{
    page_end();
    /* Interactive runs reach here on a cleared screen; a redirected
     * one reaches it directly after the last finding, and wants the
     * separating line the pager would otherwise have provided. */
    if (!interactive)
        emit(NULL, "\n");
    emit(C_HEADING, "%s\n", closing);
    terminal_restore();
    if (report)
        fclose(report);
    report = NULL;
}
/* }}} */

/* {{{ scene_open() */
void scene_open(int number, const char *heading)
{
    page_end();
    emit(NULL, "\n");
    emit(C_HEADING, "scene %d — %s\n", number, heading);
    last_block = BLOCK_HEADING;
}
/* }}} */

/* {{{ scene_problem() */
void scene_problem(const char *paragraph)
{
    begin_block(BLOCK_PROSE);
    emit_wrapped(NULL, SCENE_INDENT, paragraph);
}
/* }}} */

/* {{{ scene_imagine() */
/* The framing sentence belongs to the presenter rather than to each
 * demo, so that the hinge between mechanism and analogy is worded the
 * same way in all thirty-five scenes. A reader learns the shape once
 * and then knows, on every page, exactly where the engineering stops
 * and the picture starts. */
void scene_imagine(const char *paragraph)
{
    char framed[1200];

    begin_block(BLOCK_IMAGINE);
    snprintf(framed, sizeof framed,
             "To picture it more easily, imagine %s", paragraph);
    emit_wrapped(C_STORY, SCENE_INDENT, framed);
}
/* }}} */

/* {{{ scene_stands_for() */
void scene_stands_for(const char *engine_term, const char *story_thing,
                      const char *because)
{
    if (mapping_count >= MAPPING_MAX) {
        /* A scene naming more than eight correspondences is a scene
         * doing two jobs. Saying so is more useful than silently
         * dropping the ninth. */
        fprintf(stderr, "demo: scene names more than %d correspondences; "
                        "split the scene\n", MAPPING_MAX);
        exit(1);
    }
    mapping[mapping_count].engine_term = engine_term;
    mapping[mapping_count].story_thing = story_thing;
    mapping[mapping_count].because = because;
    mapping_count++;
}
/* }}} */

/* {{{ scene_measured() */
void scene_measured(const char *label, const char *story, const char *engine)
{
    begin_block(BLOCK_MEASURED);

    emit(NULL, "%s", SCENE_ROW_INDENT);

    /* Two columns when the story's unit is the engine's unit, three
     * when they differ. Printing "501 tasks   (501 tasks)" would teach
     * a reader to stop reading the second column. */
    if (engine) {
        emit_padded(NULL, label, SCENE_LABEL_WIDTH);
        emit_padded(C_FIGURE, story, SCENE_STORY_WIDTH);
        emit(C_QUIET, "%s", engine);
    } else {
        emit_padded(NULL, label, SCENE_LABEL_WIDTH);
        emit(C_FIGURE, "%s", story);
    }
    emit(NULL, "\n");
}
/* }}} */

/* {{{ scene_finding() */
void scene_finding(const char *paragraph)
{
    begin_block(BLOCK_FINDING);
    emit_wrapped(NULL, SCENE_INDENT, paragraph);
}
/* }}} */

/* {{{ trim_trailing() */
static void trim_trailing(char *text)
{
    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t'))
        text[--length] = '\0';
}
/* }}} */

/* {{{ scene_line() */
void scene_line(const char *format, ...)
{
    /* Formatted into a buffer rather than straight out, so the
     * trailing spaces that fixed-width columns leave behind can be
     * cut. They are invisible on a terminal and permanent in the
     * mirrored report, which is the worst combination: nobody sees
     * them, so nobody ever removes them. */
    char text[1536];
    va_list args;

    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    trim_trailing(text);

    begin_block(BLOCK_LINE);
    emit(NULL, "%s%s\n", SCENE_ROW_INDENT, text);
}
/* }}} */

/* {{{ scene_blank() */
void scene_blank(void)
{
    if (mapping_count)
        flush_mapping();
    emit(NULL, "\n");
    /* Forget what came before, so whatever prints next does not add a
     * second blank on top of this one. */
    last_block = BLOCK_NONE;
}
/* }}} */

/* {{{ scene_live_begin() */
void scene_live_begin(void)
{
    if (mapping_count)
        flush_mapping();
    /* The panel is its own block; without this it starts flush against
     * the last justification sentence. */
    emit(NULL, "\n");
    last_block = BLOCK_NONE;
    live_active = 1;
    live_finished = 0;
    live_key_count = 0;
    live_interval_us = 120000;
    frame_rows = 0;
}
/* }}} */

/* {{{ scene_live_key() */
void scene_live_key(char key, const char *what_it_does)
{
    if (live_key_count >= LIVE_KEYS_MAX) {
        fprintf(stderr, "demo: a live panel offers more than %d keys; "
                        "a reader will not find them\n", LIVE_KEYS_MAX);
        exit(1);
    }
    live_keys[live_key_count].key = key;
    live_keys[live_key_count].what = what_it_does;
    live_key_count++;
}
/* }}} */

/* {{{ scene_live_pressed() */
int scene_live_pressed(void)
{
    int key = poll_key();

    if (key == ' ') {
        live_finished = 1;
        return 0;
    }
    if (key == 'q' || key == 'Q') {
        erase_rows(frame_rows + page_rows);
        printf("  stopped early; the report holds every scene, "
               "including the ones not shown\n");
        terminal_restore();
        if (report)
            fclose(report);
        exit(0);
    }
    return key;
}
/* }}} */

/* {{{ scene_live_done() */
int scene_live_done(void)
{
    /* A run that nobody is watching cannot be steered and must not
     * wait to be: it takes one frame and moves on. */
    if (!interactive)
        return 1;
    return live_finished;
}
/* }}} */

/* {{{ scene_live_finish() */
void scene_live_finish(void)
{
    live_finished = 1;
}
/* }}} */

/* {{{ scene_frame_begin() */
void scene_frame_begin(void)
{
    erase_rows(frame_rows);
    frame_rows = 0;
    in_frame = 1;
}
/* }}} */

/* {{{ scene_frame_end() */
void scene_frame_end(void)
{
    int i;

    /* The legend, redrawn with the frame so it cannot scroll away from
     * the thing it describes. */
    if (interactive && live_key_count) {
        emit(NULL, "\n%s", SCENE_ROW_INDENT);
        for (i = 0; i < live_key_count; i++) {
            emit(C_KEY, "[%c]", live_keys[i].key);
            emit(C_QUIET, " %s   ", live_keys[i].what);
        }
        emit(NULL, "\n");
        emit(NULL, "%s", SCENE_ROW_INDENT);
        emit(C_KEY, "[space]");
        emit(C_QUIET, " move on   ");
        emit(C_KEY, "[q]");
        emit(C_QUIET, " quit\n");
    }

    in_frame = 0;

    if (interactive && !live_finished)
        usleep((useconds_t)live_interval_us);
}
/* }}} */

/* {{{ scene_bar() */
void scene_bar(const char *label, double fraction, const char *value_text)
{
    enum { BAR_WIDTH = 28 };
    char bar[BAR_WIDTH + 1];
    const char *colour;
    int filled;
    int i;

    if (fraction < 0.0)
        fraction = 0.0;
    if (fraction > 1.0)
        fraction = 1.0;
    filled = (int)(fraction * BAR_WIDTH + 0.5);

    for (i = 0; i < BAR_WIDTH; i++)
        bar[i] = (i < filled) ? '#' : '.';
    bar[BAR_WIDTH] = '\0';

    /* A bar that is nearly full usually means something different from
     * an empty one — a queue about to grow, a buffer absorbing a
     * mismatch — so the colour says which end it is at without the
     * reader having to measure the bar against its own frame. */
    if (fraction >= 0.85)
        colour = C_ALERT;
    else if (fraction >= 0.5)
        colour = C_ENGINE;
    else
        colour = C_STORY;

    emit(NULL, "%s%-*s", SCENE_ROW_INDENT, SCENE_LABEL_WIDTH, label);
    emit(colour, "%s", bar);
    emit(NULL, "  ");
    emit(C_FIGURE, "%s", value_text);
    emit(NULL, "\n");
}
/* }}} */

/* {{{ scene_frame_line() */
void scene_frame_line(const char *format, ...)
{
    char text[1024];
    va_list args;

    va_start(args, format);
    vsnprintf(text, sizeof text, format, args);
    va_end(args);
    trim_trailing(text);

    /* An empty frame line is a spacer, and a spacer made of an indent
     * is trailing whitespace with extra steps. */
    if (text[0] == '\0')
        emit(NULL, "\n");
    else
        emit(NULL, "%s%s\n", SCENE_ROW_INDENT, text);
}
/* }}} */

/* {{{ scene_frame_blank() */
void scene_frame_blank(void)
{
    emit(NULL, "\n");
}
/* }}} */

/* {{{ scene_live_end() */
void scene_live_end(void)
{
    /* The panel's frames were screen-only, so the report has nothing
     * from it. That is deliberate and it is why a live scene still
     * ends with ordinary measured rows: those are what the document
     * keeps, and they are the same numbers the last frame showed. */
    live_active = 0;
    frame_rows = 0;
    last_block = BLOCK_LINE;
}
/* }}} */

/* {{{ scene_live_interval() — used by demos through their own keys */
/* Exposed through the header only as a consequence of the keys a demo
 * declares; there is no reason for a demo to set it to an arbitrary
 * number, so it is adjusted in steps. */
void scene_live_faster(void);
void scene_live_faster(void)
{
    live_interval_us /= 2;
    if (live_interval_us < 8000)
        live_interval_us = 8000;
}

void scene_live_slower(void);
void scene_live_slower(void)
{
    live_interval_us *= 2;
    if (live_interval_us > 500000)
        live_interval_us = 500000;
}
/* }}} */

/* {{{ scene_screen_stream() */
FILE *scene_screen_stream(void)
{
    return stdout;
}
/* }}} */

/* {{{ scene_report_stream() */
FILE *scene_report_stream(void)
{
    return report;
}
/* }}} */

/* {{{ scene_interactive() */
int scene_interactive(void)
{
    return interactive;
}
/* }}} */

/* {{{ scene_text() */
const char *scene_text(const char *format, ...)
{
    /* The ring. Eight slots, because the widest call in any demo needs
     * two live at once and eight leaves room without inviting anyone
     * to treat this as storage.
     *
     * Each slot holds a whole paragraph, because findings are built
     * here too and a finding that needs a measured number in the
     * middle of a sentence is the normal case. */
    static char slots[8][512];
    static int next;

    char *slot = slots[next];
    va_list args;

    next = (next + 1) % (int)(sizeof slots / sizeof slots[0]);

    va_start(args, format);
    vsnprintf(slot, sizeof slots[0], format, args);
    va_end(args);

    return slot;
}
/* }}} */

/* {{{ scene_seed() */
unsigned scene_seed(void)
{
    return seed_value;
}
/* }}} */

/* {{{ scene_pick() */
int scene_pick(int low, int high)
{
    /* A demo asking for an empty or backwards range is a demo with a
     * bug in its own setup, and silently returning the low end would
     * hide it behind plausible output. */
    if (high < low) {
        fprintf(stderr, "demo: scene_pick(%d, %d) has no values in it\n",
                low, high);
        exit(1);
    }
    return low + (int)(next_random() % (unsigned)(high - low + 1));
}
/* }}} */
