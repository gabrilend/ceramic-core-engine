/*
 * 033-statics.c — the values that are simply always there.
 *
 * What this is: a constant living on the input port that reads it.
 * Thresholds, file paths, configuration. A static port is always
 * full, never consumed, and never affects whether a station is ready;
 * this file owns the text-to-bytes reader that fills one, the writer
 * that turns one back into text, and the runtime write that changes
 * one while the program runs.
 *
 * **There was a table here.** Numbered entries on the map, shared by
 * every port that named one, behind a mutex of their own. What that
 * cost was out of proportion to what it bought: it was map-level
 * mutable state, so a process could hold only one running program; it
 * was a second lock nested inside the station's on every claim; and
 * an entry's bytes were shaped by whichever port bound it first, so
 * two ports of different types could read the same bytes each their
 * own way. Issue 401 moved the value onto the port and the table
 * stopped having anything to hold.
 *
 * A station is one instantiation of a box, wired its own way. Its
 * input ports are its own, and what one of them holds is a property
 * of that port on that station and of nothing else. Sharing, when it
 * is wanted, is drawn: one station holds the value and everyone who
 * needs it has an arrow from it, which costs a station and gains a
 * wire somebody can see.
 *
 * How it does it, in general terms: a port is given text and parses
 * it, at its own registry type, into its own storage — a number for
 * an int port, a brace walk over the generated field table for a
 * struct port, the characters themselves for a string port. From then
 * on a claim is a memcpy under the station's mutex, beside the ring
 * pops, and a runtime write is a size-checked overwrite under the
 * same mutex. Anything malformed is fatal at the moment it is given,
 * naming the station, the port, and the field, because a static that
 * half-parses is silent corruption wearing a default.
 *
 * The reader has a mirror at the bottom of this file, turning bytes
 * back into text. Nothing needed one until the table went: the table
 * kept the original string a file gave it and the dump echoed that
 * string, and with no text retained anywhere the bytes have to be
 * spoken. The two walk the same field table in opposite directions
 * and belong beside each other for exactly that reason.
 */
#include "018-station.h"
#include "026-registry.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Where an error happened, threaded through the recursive walk. It is
 * a station and a port because that is an address somebody can go and
 * look at; the messages used to name an entry number, which named a
 * row in a table rather than anything in the program.
 */
typedef struct where {
    int station;
    int slot;
} where_t;

/* {{{ die_static() */
static void die_static(const where_t *w, const char *what)
{
    fprintf(stderr, "statics: station %d port %d: %s\n",
            w->station, w->slot, what);
    abort();
}
/* }}} */

/* ------------------------------------------------------------------ */
/* What a type name fundamentally is, engine-side. This mirrors the  */
/* generator's own classification — two lists that must agree, which  */
/* the first-pass report flags as duplicated knowledge for the second */
/* pass to unify.                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    TN_INT, TN_UINT, TN_FLOAT, TN_STRING, TN_STRUCT, TN_UNKNOWN
} type_class_t;

/* {{{ classify_type_name() */
static type_class_t classify_type_name(const char *tn,
                                       const struct_info_t **out_struct)
{
    static const char *const ints[] = {
        "char", "signed char", "short", "int", "long", "long long",
        "int32_t", "int64_t", NULL
    };
    static const char *const uints[] = {
        "unsigned char", "unsigned short", "unsigned", "unsigned int",
        "unsigned long", "unsigned long long", "uint32_t", "uint64_t",
        "size_t", NULL
    };
    static const char *const floats[] = { "float", "double", NULL };
    static const char *const strings[] = { "const char *", "char *", NULL };

    for (int i = 0; ints[i]; i++)
        if (strcmp(tn, ints[i]) == 0) return TN_INT;
    for (int i = 0; uints[i]; i++)
        if (strcmp(tn, uints[i]) == 0) return TN_UINT;
    for (int i = 0; floats[i]; i++)
        if (strcmp(tn, floats[i]) == 0) return TN_FLOAT;
    for (int i = 0; strings[i]; i++)
        if (strcmp(tn, strings[i]) == 0) return TN_STRING;

    const struct_info_t *si = struct_find(tn);
    if (si) {
        if (out_struct) *out_struct = si;
        return TN_STRUCT;
    }
    return TN_UNKNOWN;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Number and string writing, width by width.                         */
/* ------------------------------------------------------------------ */

/* {{{ write_integer() / write_unsigned() / write_float() */
static void write_integer(long long v, unsigned char *out, int size,
                          const where_t *w)
{
    /* Each width is written through its own type so sign extension
     * and truncation are the compiler's, not arithmetic here. */
    switch (size) {
    case 1: { signed char x = (signed char)v;  memcpy(out, &x, 1); break; }
    case 2: { short x = (short)v;              memcpy(out, &x, 2); break; }
    case 4: { int x = (int)v;                  memcpy(out, &x, 4); break; }
    case 8: { long long x = v;                 memcpy(out, &x, 8); break; }
    default: die_static(w, "an integer field of a width the reader does not know");
    }
}

static void write_unsigned(unsigned long long v, unsigned char *out, int size,
                           const where_t *w)
{
    switch (size) {
    case 1: { unsigned char x = (unsigned char)v;   memcpy(out, &x, 1); break; }
    case 2: { unsigned short x = (unsigned short)v; memcpy(out, &x, 2); break; }
    case 4: { unsigned x = (unsigned)v;             memcpy(out, &x, 4); break; }
    case 8: { unsigned long long x = v;             memcpy(out, &x, 8); break; }
    default: die_static(w, "an unsigned field of a width the reader does not know");
    }
}

static void write_float(double v, unsigned char *out, int size,
                        const where_t *w)
{
    if (size == 4) { float x = (float)v; memcpy(out, &x, 4); }
    else if (size == 8) { memcpy(out, &v, 8); }
    else die_static(w, "a floating field of a width the reader does not know");
}
/* }}} */

/* {{{ read_integer() / read_unsigned() / read_float() */
/*
 * The other direction, for turning a value back into text. Each width
 * is read through its own type for the same reason it is written
 * through one: sign extension is the compiler's job, and doing it by
 * hand is how a negative number becomes a very large positive one.
 */
static long long read_integer(const unsigned char *p, int size,
                              const where_t *w)
{
    switch (size) {
    case 1: { signed char x;  memcpy(&x, p, 1); return x; }
    case 2: { short x;        memcpy(&x, p, 2); return x; }
    case 4: { int x;          memcpy(&x, p, 4); return x; }
    case 8: { long long x;    memcpy(&x, p, 8); return x; }
    default: die_static(w, "an integer field of a width the writer does not know");
    }
    return 0;
}

static unsigned long long read_unsigned(const unsigned char *p, int size,
                                        const where_t *w)
{
    switch (size) {
    case 1: { unsigned char x;      memcpy(&x, p, 1); return x; }
    case 2: { unsigned short x;     memcpy(&x, p, 2); return x; }
    case 4: { unsigned x;           memcpy(&x, p, 4); return x; }
    case 8: { unsigned long long x; memcpy(&x, p, 8); return x; }
    default: die_static(w, "an unsigned field of a width the writer does not know");
    }
    return 0;
}

static double read_float(const unsigned char *p, int size, const where_t *w)
{
    if (size == 4) { float x;  memcpy(&x, p, 4); return x; }
    if (size == 8) { double x; memcpy(&x, p, 8); return x; }
    die_static(w, "a floating field of a width the writer does not know");
    return 0;
}
/* }}} */

/* {{{ skip_ws() */
static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}
/* }}} */

/* {{{ parse_struct_text() */
/*
 * One generalized reader walking a field table and brace text
 * together (issue 402) — instead of a parser emitted per struct.
 * Grammar: '{' value (',' value)* '}', where a value is a number, a
 * quoted string into a char array, or a nested brace group recursing
 * into the nested field table. Counts must match exactly: too many,
 * too few, or the wrong shape are all fatal here, naming station,
 * port and field, because they are silent corruption if caught any
 * later.
 */
static const char *parse_struct_text(const struct_info_t *si, const char *p,
                                     unsigned char *out, const where_t *w)
{
    p = skip_ws(p);
    if (*p != '{')
        die_static(w, "expected '{' to open a struct value");
    p = skip_ws(p + 1);

    for (int f = 0; f < si->n_fields; f++) {
        const field_info_t *fl = &si->fields[f];
        char note[128];

        switch (fl->kind) {
        case FIELD_INT: {
            char *end;
            long long v = strtoll(p, &end, 0);
            if (end == p) {
                snprintf(note, sizeof note, "field '%s' wants a number", fl->name);
                die_static(w, note);
            }
            write_integer(v, out + fl->offset, fl->size, w);
            p = end;
            break;
        }
        case FIELD_UINT: {
            char *end;
            unsigned long long v = strtoull(p, &end, 0);
            if (end == p) {
                snprintf(note, sizeof note, "field '%s' wants a number", fl->name);
                die_static(w, note);
            }
            write_unsigned(v, out + fl->offset, fl->size, w);
            p = end;
            break;
        }
        case FIELD_FLOAT: {
            char *end;
            double v = strtod(p, &end);
            if (end == p) {
                snprintf(note, sizeof note, "field '%s' wants a number", fl->name);
                die_static(w, note);
            }
            write_float(v, out + fl->offset, fl->size, w);
            p = end;
            break;
        }
        case FIELD_STRING: {
            if (*p != '"') {
                snprintf(note, sizeof note, "field '%s' wants a quoted string", fl->name);
                die_static(w, note);
            }
            const char *start = p + 1;
            const char *stop = strchr(start, '"');
            if (!stop) {
                snprintf(note, sizeof note, "field '%s': unterminated string", fl->name);
                die_static(w, note);
            }
            int len = (int)(stop - start);
            if (len > fl->array_len - 1) {
                snprintf(note, sizeof note,
                         "field '%s' holds %d characters; %d given",
                         fl->name, fl->array_len - 1, len);
                die_static(w, note);
            }
            memset(out + fl->offset, 0, (size_t)fl->size);
            memcpy(out + fl->offset, start, (size_t)len);
            p = stop + 1;
            break;
        }
        case FIELD_STRUCT:
            p = parse_struct_text(fl->nested, p, out + fl->offset, w);
            break;
        default:
            die_static(w, "a field kind the reader does not know");
        }

        p = skip_ws(p);
        if (f < si->n_fields - 1) {
            if (*p != ',') {
                snprintf(note, sizeof note,
                         "expected ',' after field '%s' — too few values?", fl->name);
                die_static(w, note);
            }
            p = skip_ws(p + 1);
        }
    }

    if (*p == ',')
        die_static(w, "too many values for this struct");
    if (*p != '}')
        die_static(w, "expected '}' to close the struct value");
    return p + 1;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Turning a value back into words (issue 401). The exact mirror of   */
/* the reader above, walking the same field table the other way.      */
/* ------------------------------------------------------------------ */

/* {{{ struct textbuf / tb_addf() */
/*
 * A growing piece of text that never overflows and always reports how
 * much it wanted. `used` counts characters the caller asked for, which
 * may exceed the room available — so a caller that cares can tell it
 * was cut short and ask again with a bigger buffer, the same contract
 * snprintf offers.
 */
typedef struct textbuf {
    char *out;
    int   room;
    int   used;
} textbuf_t;

static void tb_addf(textbuf_t *tb, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    /* Where this write starts, and how much of the buffer is left for
     * it. Once `used` has passed `room` there is no space at all, and
     * the write is counted rather than made. */
    int left = tb->used < tb->room ? tb->room - tb->used : 0;
    char *at = tb->out + (tb->used < tb->room ? tb->used : tb->room);
    int wanted = vsnprintf(left > 0 ? at : NULL, (size_t)left, fmt, args);

    va_end(args);
    if (wanted > 0)
        tb->used += wanted;
}
/* }}} */

/* {{{ float_text() */
/*
 * Enough digits that reading the text back gives the same value.
 * Seventeen significant digits round-trip a double and nine round-trip
 * a float; fewer would make a dump that loads into a program slightly
 * different from the one dumped, which is exactly the failure the
 * round trip exists to catch.
 *
 * A whole number comes out without a decimal point — 2 rather than
 * 2.0 — and that is fine, because what reads it back is strtod, which
 * does not care. The map file format has never carried types.
 */
static void float_text(textbuf_t *tb, double v, int size)
{
    if (size == 4)
        tb_addf(tb, "%.9g", v);
    else
        tb_addf(tb, "%.17g", v);
}
/* }}} */

/* {{{ format_struct_text() */
/*
 * The mirror of parse_struct_text: '{' value (',' value)* '}', walking
 * the same field table in the same order, so what comes out is what
 * would go back in.
 */
static void format_struct_text(const struct_info_t *si,
                               const unsigned char *bytes,
                               textbuf_t *tb, const where_t *w)
{
    tb_addf(tb, "{ ");
    for (int f = 0; f < si->n_fields; f++) {
        const field_info_t *fl = &si->fields[f];
        if (f)
            tb_addf(tb, ", ");

        switch (fl->kind) {
        case FIELD_INT:
            tb_addf(tb, "%lld", read_integer(bytes + fl->offset, fl->size, w));
            break;
        case FIELD_UINT:
            tb_addf(tb, "%llu", read_unsigned(bytes + fl->offset, fl->size, w));
            break;
        case FIELD_FLOAT:
            float_text(tb, read_float(bytes + fl->offset, fl->size, w), fl->size);
            break;
        case FIELD_STRING: {
            /* A char array inside the struct, written back quoted. The
             * length is bounded by the array rather than trusted to a
             * terminator, because a field filled exactly to its width
             * has no room for one. */
            const char *chars = (const char *)(bytes + fl->offset);
            int len = 0;
            while (len < fl->array_len && chars[len])
                len++;
            tb_addf(tb, "\"%.*s\"", len, chars);
            break;
        }
        case FIELD_STRUCT:
            format_struct_text(fl->nested, bytes + fl->offset, tb, w);
            break;
        default:
            die_static(w, "a field kind the writer does not know");
        }
    }
    tb_addf(tb, " }");
}
/* }}} */

/* {{{ slot_constant_text() */
int slot_constant_text(const slot_t *sl, char *out, int room)
{
    where_t w = { -1, -1 };   /* the port is the caller's to name here */
    textbuf_t tb = { out, room, 0 };
    if (room > 0)
        out[0] = 0;

    if (!sl->constant_set) {
        tb_addf(&tb, "?");
    } else {
        const struct_info_t *si = NULL;
        switch (classify_type_name(sl->type_name ? sl->type_name : "", &si)) {
        case TN_INT:
            tb_addf(&tb, "%lld",
                    read_integer(sl->constant, sl->elem_size, &w));
            break;
        case TN_UINT:
            tb_addf(&tb, "%llu",
                    read_unsigned(sl->constant, sl->elem_size, &w));
            break;
        case TN_FLOAT:
            float_text(&tb, read_float(sl->constant, sl->elem_size, &w),
                       sl->elem_size);
            break;
        case TN_STRING: {
            /* The value is a pointer; the characters are the port's
             * own, which is what makes handing the pointer out sound. */
            const char *s = sl->constant_string ? sl->constant_string : "";
            tb_addf(&tb, "\"%s\"", s);
            break;
        }
        case TN_STRUCT:
            format_struct_text(si, sl->constant, &tb, &w);
            break;
        default:
            tb_addf(&tb, "?");
            break;
        }
    }

    /* vsnprintf terminates whatever it wrote; an empty buffer had
     * nowhere to be terminated and was handled above. */
    if (room > 0 && tb.used >= room)
        out[room - 1] = 0;
    return tb.used;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Giving a port a constant, and changing one.                        */
/* ------------------------------------------------------------------ */

/* {{{ slot_constant_free() */
void slot_constant_free(slot_t *sl)
{
    free(sl->constant);
    free(sl->constant_string);
    sl->constant = NULL;
    sl->constant_string = NULL;
    sl->constant_set = 0;
}
/* }}} */

/* {{{ map_slot_static_text() */
void map_slot_static_text(map_t *m, int station, int slot, const char *text)
{
    where_t w = { station, slot };

    if (station < 0 || station >= m->n_stations)
        die_static(&w, "giving a constant to a station outside the table");
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        die_static(&w, "giving a constant to a port the box does not have");
    slot_t *sl = &s->slots[slot];
    if (!sl->type_name)
        die_static(&w,
                   "the port has no registry type — a constant needs a station "
                   "placed by name, so the text knows what shape to become");
    if (!text || !*text)
        die_static(&w, "a constant with no text");

    /* Parsed into scratch first and copied in under the mutex, so a
     * malformed value never half-overwrites a working one and a claim
     * running concurrently never sees a value mid-parse. The parse is
     * the part that can fail; the install is the part that must not be
     * interrupted, and keeping them apart is what lets both be true. */
    unsigned char *fresh = calloc(1, (size_t)sl->elem_size);
    if (!fresh)
        die_static(&w, "out of memory parsing a constant");
    char *fresh_string = NULL;

    const struct_info_t *si = NULL;
    switch (classify_type_name(sl->type_name, &si)) {
    case TN_INT: {
        char *end;
        long long v = strtoll(text, &end, 0);
        if (end == text)
            die_static(&w, "an integer port wants a number");
        write_integer(v, fresh, sl->elem_size, &w);
        break;
    }
    case TN_UINT: {
        char *end;
        unsigned long long v = strtoull(text, &end, 0);
        if (end == text)
            die_static(&w, "an unsigned port wants a number");
        write_unsigned(v, fresh, sl->elem_size, &w);
        break;
    }
    case TN_FLOAT: {
        char *end;
        double v = strtod(text, &end);
        if (end == text)
            die_static(&w, "a floating port wants a number");
        write_float(v, fresh, sl->elem_size, &w);
        break;
    }
    case TN_STRING: {
        /* The claimed value is a pointer; the characters live on the
         * port for the life of the map, which is what makes handing
         * the pointer to a box sound. */
        const char *start = text;
        int len;
        if (*text == '"') {
            const char *stop = strchr(text + 1, '"');
            if (!stop)
                die_static(&w, "unterminated string");
            start = text + 1;
            len = (int)(stop - start);
        } else {
            len = (int)strlen(text);
        }
        fresh_string = malloc((size_t)len + 1);
        if (!fresh_string)
            die_static(&w, "out of memory for string storage");
        memcpy(fresh_string, start, (size_t)len);
        fresh_string[len] = 0;
        if (sl->elem_size != (int)sizeof(const char *))
            die_static(&w, "a string port that is not pointer-sized");
        memcpy(fresh, &fresh_string, sizeof fresh_string);
        break;
    }
    case TN_STRUCT: {
        if (si->size != sl->elem_size)
            die_static(&w, "struct size disagrees with the port");
        const char *after = parse_struct_text(si, text, fresh, &w);
        if (*skip_ws(after) != 0)
            die_static(&w, "trailing text after the struct value");
        break;
    }
    default:
        die_static(&w, "the port's type is not one the reader knows");
    }

    /* One of the four rare structural operations (issue 210): the
     * install and the tag together, under the station's mutex, so no
     * readiness walk and no claim sees a port mid-change. */
    pthread_mutex_lock(&s->mutex);
    char *old_string = sl->constant_string;
    memcpy(sl->constant, fresh, (size_t)sl->elem_size);
    sl->constant_string = fresh_string;
    sl->constant_set = 1;
    sl->kind = SLOT_STATIC;
    pthread_mutex_unlock(&s->mutex);

    free(fresh);
    free(old_string);

    /* A port that was the last one missing is no longer missing
     * (issue 210). This is what replaced the pull path, and it is one
     * addition rather than a subsystem: a chain of stations wired
     * through static ports becomes a recalculation graph, and
     * construction's own writes are what start a program.
     *
     * Only once the pool exists, because before that there is nowhere
     * to push and the loader is still assembling — the seed sweep is
     * what starts a freshly loaded map, deliberately and once. */
    if (m->pool)
        map_station_try_start(m, station);
}
/* }}} */

/* {{{ map_slot_static_write() */
void map_slot_static_write(map_t *m, int station, int slot,
                           const void *bytes, int size)
{
    where_t w = { station, slot };

    if (station < 0 || station >= m->n_stations)
        die_static(&w, "writing to a station outside the table");
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        die_static(&w, "writing to a port the box does not have");
    slot_t *sl = &s->slots[slot];
    if (!sl->constant_set)
        die_static(&w, "writing to a port that has never held a constant — "
                       "give it one as text first, so its shape is known");
    if (size != sl->elem_size)
        die_static(&w, "writing a value of the wrong size for this port");

    /* Held for the length of one copy, and it is the station's own
     * mutex — the one the claim already takes. A struct half-
     * overwritten while a claim is copying it would yield fields from
     * two different worlds, which for anything wider than a machine
     * word is not theoretical. */
    pthread_mutex_lock(&s->mutex);
    memcpy(sl->constant, bytes, (size_t)size);
    pthread_mutex_unlock(&s->mutex);

    /* Writing does not consume anything, so a station that could
     * already run runs again — which is how a value computed once
     * propagates through everything downstream of it. */
    if (m->pool)
        map_station_try_start(m, station);
}
/* }}} */
