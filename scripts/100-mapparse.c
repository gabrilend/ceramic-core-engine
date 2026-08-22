/*
 * 100-mapparse.c — reading what a map says, believing none of it yet.
 *
 * What this is: the map file parser (issue 601). It turns text into
 * a description — stations, their box names and kinds, their input
 * overrides, their arrows, and the statics — and does no
 * construction of any sort. Whether any of it makes sense is answered
 * by the calls the description becomes; whether it is well-formed is
 * answered here, fatally, naming file, line, and expectation.
 *
 * **It is part of the compiler** (issue 311d). A running program does
 * not read descriptions: it hands them to the generator, which turns
 * them into the construction calls they describe, and those are
 * compiled and loaded like any other code. So nothing links this into
 * a program anybody runs, and the file moved out of the engine's
 * directory to say so — it had already stopped being linked in, since
 * the linker discards what nothing reaches.
 *
 * How it does it, in general terms: line-oriented, first word
 * dispatches. Three keywords (statics, in, out) and the station line
 * as the default. Indentation is for the reader and means nothing.
 * A `#` starts a comment to end of line — the dump (issue 703)
 * writes derived facts as comments, so the format must be able to
 * carry them; the format document records the addition.
 */
#include "099-mapparse.h"

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

/* {{{ parse_depth_word() */
/*
 * A starting depth is written `x64` — the letter, then a count. It
 * reads as "sixty-four of them", the way a parts list writes a
 * quantity, and only the letter is accepted: a star was briefly a
 * second spelling and was withdrawn, because one way to say a thing
 * is the habit everywhere else in this engine.
 *
 * Returns 1 and fills *slots when the word is a depth, 0 when it is
 * something else entirely. A word that starts with 'x' and is not a
 * depth is not an error here — it is simply not a depth, and whatever
 * reads the source next will have its own opinion.
 */
static int parse_depth_word(const char *word, int *slots)
{
    if (word[0] != 'x' || !word[1])
        return 0;
    for (const char *p = word + 1; *p; p++)
        if (*p < '0' || *p > '9')
            return 0;
    return parse_number(word + 1, slots);
}
/* }}} */

/* {{{ handle_in() */
static void handle_in(parse_state_t *st, const char *rest)
{
    if (!st->current)
        die_parse(st->path, st->line, "an 'in' line before any station line");

    char in_port_word[64], ref[128];
    const char *after_port = next_word(rest, in_port_word, sizeof in_port_word);
    const char *after_ref = next_word(after_port, ref, sizeof ref);

    int port;
    if (!parse_number(in_port_word, &port))
        die_parse(st->path, st->line, "expected a port number after 'in'");
    if (!ref[0])
        die_parse(st->path, st->line,
                  "expected '$entry', '= value', or '-' after the port number");

    desc_input_t *in = need(calloc(1, sizeof *in), st->path, st->line);
    in->port = port;
    in->line = st->line;

    /*
     * An optional starting depth comes first, because the inline
     * value form runs to the end of the line and nothing can follow
     * it. One rule for all three source forms is worth more than a
     * rule with an exception in it (issue 210b).
     */
    if (parse_depth_word(ref, &in->depth)) {
        if (in->depth <= 0)
            die_parse(st->path, st->line,
                      "a starting depth must be at least one slot");
        after_port = after_ref;
        after_ref = next_word(after_port, ref, sizeof ref);
        /* Nothing after the depth is legal and means **a buffer this
         * deep**: the source is the default, so the only thing the
         * line says is how deep to start. It used to be refused,
         * which left the dump with no way to write a deepened buffer
         * except `x64 -` — and that reads back as a port with no
         * source, so the program could not be reloaded. */
    }

    /* Three forms, and the first character tells them apart.
     *
     * '= value' carries the value on this line. It is what the dump
     * writes, because a dump has values on ports and no entry numbers
     * to point at; the '=' matches the statics section's own 'N =
     * value', so the two places a value can be written spell it the
     * same way. The text runs to the end of the line, because a struct
     * value has spaces in it.
     *
     * '$entry' points at the statics section, which is notation
     * resolved while the file is read (issue 401).
     *
     * '-' is a port with no source at all: a state rather than a
     * value, in which the station simply never becomes ready. No
     * value in this format is ever a lone dash, so it cannot be read
     * as one, and the dash already means "wire" on an out line — so
     * this reads as a wire that is not there yet (issue 210b).
     *
     * A bare station name used to be a fourth form, meaning "gather
     * from that station". Nothing is gathered now (issue 210), so it
     * is refused rather than quietly reinterpreted — the forms differ
     * by one character, and reading an old file as a new one would run
     * a program nobody wrote. */
    if (ref[0] == '=') {
        const char *value = after_port;
        while (*value == ' ' || *value == '\t')
            value++;
        value++;                                  /* past the '=' */
        while (*value == ' ' || *value == '\t')
            value++;
        if (!*value)
            die_parse(st->path, st->line, "an input with no value after '='");
        in->is_static = 0;
        in->text = copy_string(value, st->path, st->line);
    } else if (ref[0] == '$') {
        char extra[8];
        next_word(after_ref, extra, sizeof extra);
        if (extra[0])
            die_parse(st->path, st->line,
                      "unexpected trailing words on an 'in' line");
        in->is_static = 1;
        if (!parse_number(ref + 1, &in->static_id))
            die_parse(st->path, st->line, "expected a number after '$'");
    } else if (ref[0] == '-' && !ref[1]) {
        char extra[8];
        next_word(after_ref, extra, sizeof extra);
        if (extra[0])
            die_parse(st->path, st->line,
                      "unexpected trailing words on an 'in' line — a dash "
                      "means this port has no source, so nothing follows it");
        in->is_none = 1;
    } else if (!ref[0] && in->depth > 0) {
        /*
         * A depth and nothing else: **a buffer this deep**, fed by
         * arrows like any buffer. The port's source is the default, so
         * the only thing this line is saying is how deep to start.
         *
         * It exists because the dump needed to write one. A deepened
         * buffer used to come out as `x64 -`, and a bare dash means a
         * port with *no source* — so such a program could be written
         * down and not read back, and an arrow into that port was
         * refused on the way in. Two different things were spelled the
         * same way, which is the one thing a round trip cannot
         * survive.
         */
        in->is_none = 0;
        in->is_static = 0;
        in->text = NULL;
    } else {
        die_parse(st->path, st->line,
                  "expected '$entry', '= value', '-', or a depth alone; a "
                  "bare station name here meant 'gather from that station', "
                  "and there is no pull path any more");
    }

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
                  "expected the destination as station.port, like printer.0");
    *dot = 0;

    desc_output_t *out = need(calloc(1, sizeof *out), st->path, st->line);
    out->port = port;
    out->line = st->line;
    out->dest_station = copy_string(dest, st->path, st->line);
    if (!parse_number(dot + 1, &out->dest_port))
        die_parse(st->path, st->line, "expected a port number after the dot");

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
     * type, which arrives when a port binds it. */
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
    char box[128], kind_word[8], door_word[16], extra[8];
    rest = next_word(rest, box, sizeof box);
    rest = next_word(rest, kind_word, sizeof kind_word);
    rest = next_word(rest, door_word, sizeof door_word);
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

    /*
     * An optional fourth word says this station is one of the
     * program's doors (issues 209, 213): `entry` for where the
     * outside delivers, `result` for where results come from.
     *
     * Words rather than `in` and `out`, which already mean a port on
     * the lines beneath a station. A file where one word means a port
     * in one place and a whole station in another is the kind of
     * ambiguity that reads fine and round-trips wrong.
     */
    int door = DOOR_NONE;
    if (door_word[0]) {
        if (strcmp(door_word, "entry") == 0)       door = DOOR_IN;
        else if (strcmp(door_word, "result") == 0) door = DOOR_OUT;
        else
            die_parse(st->path, st->line,
                      "after the kind, only 'entry' (the outside delivers "
                      "here) or 'result' (results come from here)");
    }

    for (desc_station_t *s = st->d->stations; s; s = s->next)
        if (strcmp(s->name, name) == 0)
            die_parse(st->path, st->line, "a station with this name already exists");

    desc_station_t *s = need(calloc(1, sizeof *s), st->path, st->line);
    s->name = copy_string(name, st->path, st->line);
    s->box = copy_string(box, st->path, st->line);
    s->kind = kind;
    s->door = door;
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

        /*
         * **The keyword dispatch, and every line kind announces
         * itself** (issue 607).
         *
         * A station line used to be *what remains* — anything whose
         * first word was not one of three keywords. That is a
         * negative definition, and a negative definition can only
         * narrow: every keyword the format ever gains takes another
         * name away from every map anybody has already written,
         * silently, with the failure appearing as a parse error about
         * something else. A station called `in` was told there was an
         * input line before any station.
         *
         * So the station line is announced like the others. The first
         * word of a line is *always* a keyword and the second is
         * *always* a name, so no word is ever both — a station called
         * `in` is `station in keep p`, one called `station` is
         * `station station keep p`, and neither is a special case.
         *
         * The word costs one field on the least numerous line kind: a
         * map has more input and output lines than station lines, so
         * the marker lands on the cheaper half.
         */
        if (strcmp(first, "in") == 0) {
            handle_in(&st, rest);
        } else if (strcmp(first, "out") == 0) {
            handle_out(&st, rest);
        } else if (strcmp(first, "statics") == 0) {
            st.in_statics = 1;
            st.current = NULL;
        } else if (st.in_statics && isdigit((unsigned char)first[0])) {
            handle_static_entry(&st, line);
        } else if (strcmp(first, "station") == 0) {
            char name[128];
            rest = next_word(rest, name, sizeof name);
            if (!name[0])
                die_parse(path, st.line,
                          "a station line needs a name after 'station'");
            handle_station(&st, name, rest);
        } else {
            /* Names all four rather than saying what this is not,
             * because a reader who wrote something wrong wants the
             * list of what is right. */
            die_parse(path, st.line,
                      "a line starts with 'station', 'in', 'out' or "
                      "'statics'");
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
            /* Null unless the line carried its value inline; free
             * copes either way. */
            free(in->text);
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
