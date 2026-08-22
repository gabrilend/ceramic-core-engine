/*
 * 105-mapwrite.c — a description, back out as the text it came from.
 *
 * What this is: the parser's missing other half. Text becomes a
 * description and now a description becomes text, so the two can be
 * asked whether they agree — read a file, write it back, read that,
 * and the second description must equal the first.
 *
 * **The project already had a writer and it is not this one.** The
 * dump walks a *live station table* and says what the engine is
 * actually running. That needs a running program, which is exactly
 * what somebody drawing a map does not have — a canvas holds a
 * drawing, not a table. Discovering that is what this file is: issue
 * 801 planned to compile the existing writer for the browser and
 * there was nothing there to compile.
 *
 * So there are two writers on purpose, and they answer different
 * questions. The dump answers *what is running*; this answers *what
 * was written down*. A description has no capacities, no sizes, no
 * derived facts, and cannot invent them — which is what makes it the
 * right thing to produce from a drawing.
 *
 * How it does it, in general terms: one pass over the stations in file
 * order, writing exactly the lines that say something. A port at its
 * defaults produces no line at all, because the format writes
 * exceptions and a file restating every default is a file nobody
 * reads.
 */
#include "099-mapparse.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ struct out / add() */
/*
 * A growing piece of text. Grows by doubling rather than by asking for
 * exactly enough each time, because a description is written in many
 * small pieces and one allocation per piece is the shape that turns a
 * small file into a lot of work.
 */
typedef struct {
    char  *text;
    size_t len;
    size_t room;
} out_t;

static void add(out_t *o, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void add(out_t *o, const char *fmt, ...)
{
    va_list args;
    for (;;) {
        size_t left = o->room - o->len;
        va_start(args, fmt);
        int wanted = vsnprintf(o->text + o->len, left, fmt, args);
        va_end(args);
        if (wanted < 0) {
            fprintf(stderr, "mapwrite: cannot format\n");
            exit(71);
        }
        if ((size_t)wanted < left) {
            o->len += (size_t)wanted;
            return;
        }
        /* It did not fit, so grow and write it again. vsnprintf said
         * how much it wanted, so one growth is always enough. */
        size_t want = o->len + (size_t)wanted + 1;
        size_t room = o->room ? o->room : 256;
        while (room < want)
            room *= 2;
        char *bigger = realloc(o->text, room);
        if (!bigger) {
            fprintf(stderr, "mapwrite: out of memory\n");
            exit(71);
        }
        o->text = bigger;
        o->room = room;
    }
}
/* }}} */

/* {{{ static char kind_letter() */
static char kind_letter(int kind)
{
    if (kind == STATION_COMPARATOR) return 'c';
    if (kind == STATION_ITERATOR)   return 'i';
    return 'p';
}
/* }}} */

/* {{{ mapfile_write() */
char *mapfile_write(const map_description_t *d)
{
    out_t o = { NULL, 0, 0 };
    add(&o, "%s", "");   /* so an empty description is "" and not null */

    /*
     * The statics section first, because a port line may point into it
     * and a reader meeting `$3` should already have seen entry 3.
     * Written only when there is one: an empty section is a heading
     * with nothing under it.
     */
    if (d->statics) {
        add(&o, "statics\n");
        for (desc_static_t *e = d->statics; e; e = e->next)
            add(&o, "  %d = %s\n", e->id, e->text);
        add(&o, "\n");
    }

    for (desc_station_t *s = d->stations; s; s = s->next) {
        add(&o, "station %s %s %c", s->name, s->box, kind_letter(s->kind));
        if (s->door == DOOR_IN)       add(&o, " entry");
        else if (s->door == DOOR_OUT) add(&o, " result");
        /* Where an iterator had got to, written only when it says
         * something: zero is where one starts. */
        if (s->kind == STATION_ITERATOR && s->cursor > 0)
            add(&o, " @%d", s->cursor);
        add(&o, "\n");

        for (desc_input_t *in = s->inputs; in; in = in->next) {
            /*
             * A depth comes before the source, because the value forms
             * run to the end of the line and nothing can follow them.
             */
            char depth[24] = "";
            if (in->depth > 0)
                snprintf(depth, sizeof depth, "x%d ", in->depth);

            if (in->is_none)
                add(&o, "  in %d %s-\n", in->port, depth);
            else if (in->is_waiting)
                add(&o, "  in %d %s[%s]\n", in->port, depth, in->text);
            else if (in->is_static)
                add(&o, "  in %d %s$%d\n", in->port, depth, in->static_id);
            else if (in->text)
                add(&o, "  in %d %s= %s\n", in->port, depth, in->text);
            else
                /* A depth and nothing after it: a buffer this deep,
                 * fed by arrows like any buffer. Written without a
                 * dash, which would say the port has no source at
                 * all — two different things spelled one way is the
                 * one thing a round trip cannot survive. */
                add(&o, "  in %d %s\n", in->port, depth);
        }

        for (desc_output_t *out = s->outputs; out; out = out->next)
            add(&o, "  out %d - %s.%d\n",
                out->port, out->dest_station, out->dest_port);

        if (s->next)
            add(&o, "\n");
    }

    return o.text;
}
/* }}} */
