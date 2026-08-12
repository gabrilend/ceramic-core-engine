/*
 * 041-mapfile.c — reading what a map says, believing none of it yet.
 *
 * What this is: the map file parser (issue 601). It turns text into
 * a description — stations, their box names and kinds, their input
 * overrides, their arrows, and the statics — and does no
 * construction of any sort. Whether any of it makes sense is the
 * loader's question; whether it is well-formed is answered here,
 * fatally, naming file, line, and expectation.
 *
 * How it does it, in general terms: line-oriented, first word
 * dispatches. Three keywords (statics, in, out) and the station line
 * as the default. Indentation is for the reader and means nothing.
 * A `#` starts a comment to end of line — the dump (issue 703)
 * writes derived facts as comments, so the format must be able to
 * carry them; the format document records the addition.
 */
#include "040-mapfile.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ die_parse() */
static void die_parse(const char *path, int line, const char *what)
{
    fprintf(stderr, "map %s:%d: %s\n", path, line, what);
    abort();
}
/* }}} */

/* {{{ small allocation helpers */
static void *need(void *p, const char *path, int line)
{
    if (!p)
        die_parse(path, line, "out of memory reading the map");
    return p;
}

static char *copy_string(const char *s, const char *path, int line)
{
    return need(strdup(s), path, line);
}
/* }}} */

/* {{{ next_word() */
/* Advance past whitespace, copy the next run of non-space characters
 * into `out`, and return the position after it. Empty means end. */
static const char *next_word(const char *p, char *out, size_t out_len)
{
    while (*p == ' ' || *p == '\t')
        p++;
    size_t n = 0;
    while (*p && *p != ' ' && *p != '\t' && n + 1 < out_len)
        out[n++] = *p++;
    out[n] = 0;
    return p;
}
/* }}} */

/* {{{ parse_number() */
static int parse_number(const char *word, int *out)
{
    if (!*word)
        return 0;
    char *end;
    long v = strtol(word, &end, 10);
    if (*end != 0)
        return 0;
    *out = (int)v;
    return 1;
}
/* }}} */

/*
 * The line handlers. Each receives the text after its keyword and
 * the parse state; the dispatch below routes by first word. The
 * state is which station is open and whether the statics section is.
 */
typedef struct parse_state {
    map_description_t *d;
    desc_station_t    *current;      /* the station in lines attach to */
    desc_station_t   **station_tail;
    int                in_statics;
    const char        *path;
    int                line;
} parse_state_t;

/* {{{ handle_in() */
static void handle_in(parse_state_t *st, const char *rest)
{
    if (!st->current)
        die_parse(st->path, st->line, "an 'in' line before any station line");

    char slot_word[64], ref[128], extra[8];
    rest = next_word(rest, slot_word, sizeof slot_word);
    rest = next_word(rest, ref, sizeof ref);
    next_word(rest, extra, sizeof extra);

    int slot;
    if (!parse_number(slot_word, &slot))
        die_parse(st->path, st->line, "expected a slot number after 'in'");
    if (!ref[0])
        die_parse(st->path, st->line,
                  "expected '$entry' after the slot number");
    if (extra[0])
        die_parse(st->path, st->line, "unexpected trailing words on an 'in' line");

    /* An 'in' line used to take either form: '$entry' named a static,
     * and a bare station name named a gather source. Nothing is
     * gathered now (issue 210), so the bare name is refused rather
     * than quietly reinterpreted — an old map file saying it meant
     * something the engine no longer does, and reading it as anything
     * else would run a program nobody wrote. */
    if (ref[0] != '$')
        die_parse(st->path, st->line,
                  "expected '$entry'; a bare station name here meant "
                  "'gather from that station', and there is no pull path "
                  "any more");

    desc_input_t *in = need(calloc(1, sizeof *in), st->path, st->line);
    in->slot = slot;
    in->line = st->line;

    /* The dollar is technically unnecessary now that it is the only
     * form — but 'in 1 0' reading as "static entry zero" is not
     * something anyone will guess a year from now (issue 601), and
     * it is what tells an old map file apart from a new one. */
    in->is_static = 1;
    if (!parse_number(ref + 1, &in->static_id))
        die_parse(st->path, st->line, "expected a number after '$'");

    desc_input_t **tail = &st->current->inputs;
    while (*tail)
        tail = &(*tail)->next;
    *tail = in;
}
/* }}} */

/* {{{ handle_out() */
static void handle_out(parse_state_t *st, const char *rest)
{
    if (!st->current)
        die_parse(st->path, st->line, "an 'out' line before any station line");

    char port_word[64], dash[8], dest[192], extra[8];
    rest = next_word(rest, port_word, sizeof port_word);
    rest = next_word(rest, dash, sizeof dash);
    rest = next_word(rest, dest, sizeof dest);
    next_word(rest, extra, sizeof extra);

    int port;
    if (!parse_number(port_word, &port))
        die_parse(st->path, st->line, "expected a port number after 'out'");
    if (strcmp(dash, "-") != 0)
        die_parse(st->path, st->line, "expected '-' between port and destination");
    if (extra[0])
        die_parse(st->path, st->line, "unexpected trailing words on an 'out' line");

    char *dot = strrchr(dest, '.');
    if (!dot || dot == dest || !dot[1])
        die_parse(st->path, st->line,
                  "expected the destination as station.slot, like printer.0");
    *dot = 0;

    desc_output_t *out = need(calloc(1, sizeof *out), st->path, st->line);
    out->port = port;
    out->line = st->line;
    out->dest_station = copy_string(dest, st->path, st->line);
    if (!parse_number(dot + 1, &out->dest_slot))
        die_parse(st->path, st->line, "expected a slot number after the dot");

    desc_output_t **tail = &st->current->outputs;
    while (*tail)
        tail = &(*tail)->next;
    *tail = out;
}
/* }}} */

/* {{{ handle_static_entry() */
static void handle_static_entry(parse_state_t *st, const char *line_text)
{
    /* Inside the statics section: `N = value`, the value kept as
     * text to the end of the line — turning it into bytes needs a
     * type, which arrives when a slot binds it. */
    char id_word[64];
    const char *p = next_word(line_text, id_word, sizeof id_word);
    int id;
    if (!parse_number(id_word, &id))
        die_parse(st->path, st->line, "expected an entry number in the statics section");
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != '=')
        die_parse(st->path, st->line, "expected '=' after the entry number");
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    if (!*p)
        die_parse(st->path, st->line, "an entry with no value after '='");

    desc_static_t *entry = need(calloc(1, sizeof *entry), st->path, st->line);
    entry->id = id;
    entry->line = st->line;
    entry->text = copy_string(p, st->path, st->line);

    desc_static_t **tail = &st->d->statics;
    while (*tail) {
        if ((*tail)->id == id)
            die_parse(st->path, st->line, "this statics entry was already given");
        tail = &(*tail)->next;
    }
    *tail = entry;
    if (id > st->d->max_static_id)
        st->d->max_static_id = id;
}
/* }}} */

/* {{{ handle_station() */
static void handle_station(parse_state_t *st, const char *name,
                           const char *rest)
{
    char box[128], kind_word[8], extra[8];
    rest = next_word(rest, box, sizeof box);
    rest = next_word(rest, kind_word, sizeof kind_word);
    next_word(rest, extra, sizeof extra);

    if (!box[0] || !kind_word[0])
        die_parse(st->path, st->line,
                  "a station line is three words: name, box function, kind (p/c/i)");
    if (extra[0])
        die_parse(st->path, st->line, "unexpected trailing words on a station line");

    /* The kind is written rather than inferred: forgetting a
     * threshold line must be an error, never a silent demotion to a
     * plain box that routes everything one way (issue 601). One
     * letter, dispatched. */
    int kind;
    if (strcmp(kind_word, "p") == 0)      kind = STATION_PLAIN;
    else if (strcmp(kind_word, "c") == 0) kind = STATION_COMPARATOR;
    else if (strcmp(kind_word, "i") == 0) kind = STATION_ITERATOR;
    else {
        die_parse(st->path, st->line,
                  "the kind must be p (plain), c (comparator), or i (iterator)");
        return;
    }

    for (desc_station_t *s = st->d->stations; s; s = s->next)
        if (strcmp(s->name, name) == 0)
            die_parse(st->path, st->line, "a station with this name already exists");

    desc_station_t *s = need(calloc(1, sizeof *s), st->path, st->line);
    s->name = copy_string(name, st->path, st->line);
    s->box = copy_string(box, st->path, st->line);
    s->kind = kind;
    s->line = st->line;

    *st->station_tail = s;
    st->station_tail = &s->next;
    st->d->n_stations++;
    st->current = s;
    st->in_statics = 0;
}
/* }}} */

/* {{{ mapfile_parse() */
map_description_t *mapfile_parse(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "map %s: cannot open\n", path);
        abort();
    }

    map_description_t *d = need(calloc(1, sizeof *d), path, 0);
    d->path = copy_string(path, path, 0);
    d->max_static_id = -1;

    parse_state_t st = { 0 };
    st.d = d;
    st.station_tail = &d->stations;
    st.path = path;

    char line[1024];
    while (fgets(line, sizeof line, f)) {
        st.line++;

        /* Comments to end of line, then trailing whitespace. */
        char *hash = strchr(line, '#');
        if (hash)
            *hash = 0;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'
                           || line[len - 1] == ' ' || line[len - 1] == '\t'))
            line[--len] = 0;

        char first[128];
        const char *rest = next_word(line, first, sizeof first);
        if (!first[0])
            continue;

        /* The keyword dispatch (issue 601): three keywords, the
         * statics entry while that section is open, and the station
         * line as what remains. */
        if (strcmp(first, "in") == 0) {
            handle_in(&st, rest);
        } else if (strcmp(first, "out") == 0) {
            handle_out(&st, rest);
        } else if (strcmp(first, "statics") == 0) {
            st.in_statics = 1;
            st.current = NULL;
        } else if (st.in_statics && isdigit((unsigned char)first[0])) {
            handle_static_entry(&st, line);
        } else {
            handle_station(&st, first, rest);
        }
    }
    fclose(f);

    if (d->n_stations == 0)
        die_parse(path, st.line, "the map names no stations at all");

    return d;
}
/* }}} */

/* {{{ mapfile_free() */
void mapfile_free(map_description_t *d)
{
    desc_station_t *s = d->stations;
    while (s) {
        desc_input_t *in = s->inputs;
        while (in) {
            desc_input_t *next = in->next;
            free(in);
            in = next;
        }
        desc_output_t *out = s->outputs;
        while (out) {
            desc_output_t *next = out->next;
            free(out->dest_station);
            free(out);
            out = next;
        }
        desc_station_t *next = s->next;
        free(s->name);
        free(s->box);
        free(s);
        s = next;
    }
    desc_static_t *e = d->statics;
    while (e) {
        desc_static_t *next = e->next;
        free(e->text);
        free(e);
        e = next;
    }
    free(d->path);
    free(d);
}
/* }}} */
