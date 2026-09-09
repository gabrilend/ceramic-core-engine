/*
 * 133-line-boxes.c — reading a file a line at a time, with no memory.
 *
 * What this is: the smallest honest answer to "how do I stream lines
 * through a map". A box may not remember anything between calls, so
 * the file's text and the reader's position cannot live in the box —
 * they live in the value that travels the wire, and the wire loops
 * back on itself so each call hands the next one where it got to.
 *
 * How it does it, in general terms: one box slurps the file once and
 * emits a reader; a second box takes one line off a reader and emits
 * the reader advanced; a comparator on that reader's "did I get a
 * line" field sends it round again or lets it fall off the end. The
 * cost is that the whole text is copied per line, which is what
 * carrying state in the value means and is worth seeing plainly.
 */
#include <stdio.h>
#include <string.h>

/* The reader, entire. Everything the next call needs to know, because
 * the next call will be a different invocation with nothing kept. */
typedef struct {
    char text[2048];   /* the file, slurped once */
    int  len;          /* bytes of it that are real */
    int  pos;          /* where the next line starts */
    char line[128];    /* the line this call took, if it took one */
    int  more;         /* 1 if line holds something, 0 at the end */
} lines;

/* {{{ open_lines() */
/*
 * **Slurp the file and hand back a reader positioned at the start.**
 *
 * Its only port holds a constant, and a constant port is never a ring
 * buffer, so bring-up seeds this station and it runs exactly once —
 * which is what reading a file at startup has to mean in an engine
 * where nothing polls.
 */
lines open_lines(const char *path)
{
    lines r;
    memset(&r, 0, sizeof r);

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "open_lines: cannot read %s\n", path);
        return r;
    }
    r.len = (int)fread(r.text, 1, sizeof r.text - 1, f);
    fclose(f);
    r.text[r.len] = 0;
    r.pos = 0;
    r.more = 1;   /* "there may be a line" — next_line decides */
    return r;
}
/* }}} */

/* {{{ next_line() */
/*
 * **Take one line and hand back where that left us.**
 *
 * Returns the reader by value with `more` set to 1 when it produced a
 * line and 0 when the text ran out. That field is what the comparator
 * downstream reads, so the decision to loop is drawn on the map
 * rather than written here — this function makes no choice about what
 * happens next and could not, having no idea it is in a loop.
 */
lines next_line(lines r)
{
    if (r.pos >= r.len) {
        r.more = 0;
        r.line[0] = 0;
        return r;
    }

    int end = r.pos;
    while (end < r.len && r.text[end] != '\n')
        end++;

    int room = end - r.pos;
    if (room > (int)sizeof r.line - 1)
        room = (int)sizeof r.line - 1;
    memcpy(r.line, r.text + r.pos, (size_t)room);
    r.line[room] = 0;

    r.pos = end < r.len ? end + 1 : r.len;
    r.more = 1;
    return r;
}
/* }}} */

/* {{{ say_line() */
/* Printing is a side effect on the world, not memory kept between
 * calls, which is the line this engine actually draws. */
void say_line(lines r)
{
    printf("%s\n", r.line);
    fflush(stdout);
}
/* }}} */

/* {{{ lines__compare() */
/*
 * **Order two readers by whether one has a line**, which is the whole
 * of what the loop needs to ask. A comparator compares its box's
 * return value against the value on its last port; giving that port a
 * reader whose `more` is zero makes "greater" mean "there was a line"
 * and "equal" mean "the file is finished".
 */
int lines__compare(lines a, lines b)
{
    if (a.more < b.more) return -1;
    if (a.more > b.more) return 1;
    return 0;
}
/* }}} */
