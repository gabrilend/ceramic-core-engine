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
        
        in->text = copy_string(value, st->path, st->line);
    } else if (ref[0] == '-' && !ref[1]) {
        /*
         * **A dash with a source after it is a wire; a dash alone is
         * an arrow from nothing** (issue 601a). The dash has always
         * been the arrow and the keyword has always said which way it
         * points — away on an `out` line, toward on an `in` line — so
         * the two readings are one form with and without its far end.
         */
        char source[192], extra[8];
        const char *after_source = next_word(after_ref, source,
                                             sizeof source);
        next_word(after_source, extra, sizeof extra);

        size_t len = strlen(source);
        if (!source[0]) {
            in->is_none = 1;
        } else if (len > 1 && source[len - 1] == '$') {
            /*
             * **`0$` says this port is the map's argument 0** (issue
             * 601b), and it sits after the dash because that is what
             * the dash is for: everything after it is where this
             * port's values come from, and this one says they come
             * from outside.
             *
             * The number is the argument's identity, chosen by the
             * author rather than derived from where the line sits.
             */
            source[len - 1] = 0;
            in->is_argument = 1;
            if (!parse_number(source, &in->argument) || in->argument < 0)
                die_parse(st->path, st->line,
                          "expected an argument number before '$', as "
                          "'in 0 - 0$'");
        } else if (extra[0]) {
            die_parse(st->path, st->line,
                      "unexpected trailing words on an 'in' line — a dash "
                      "takes one source, written station.port, or nothing "
                      "at all");
        } else {
            char *dot = strrchr(source, '.');
            if (!dot || dot == source || !dot[1])
                die_parse(st->path, st->line,
                          "expected the source as station.port, like "
                          "feed.0 — or nothing after the dash, meaning "
                          "this port has no source yet");
            *dot = 0;
            in->is_source = 1;
            in->source_station = copy_string(source, st->path, st->line);
            if (!parse_number(dot + 1, &in->source_port))
                die_parse(st->path, st->line,
                          "expected a port number after the dot");
        }
    } else if (ref[0] == '[') {
        /*
         * **Values waiting in the buffer** (issue 712), the form that
         * says what a program holds rather than what it is shaped
         * like. Everything up to the closing bracket is kept whole:
         * the values are comma separated and a struct value has
         * commas inside it, so only something that reads one value at
         * a time can tell an outer comma from an inner one, and that
         * is not this.
         */
        const char *open_at = strchr(after_port, '[');
        const char *close_at = open_at ? strrchr(open_at, ']') : NULL;
        if (!close_at)
            die_parse(st->path, st->line,
                      "a list of waiting values opened with '[' and never "
                      "closed");
        for (const char *after = close_at + 1; *after; after++)
            if (*after != ' ' && *after != '\t' && *after != '#')
                die_parse(st->path, st->line,
                          "unexpected text after the waiting values");
            else if (*after == '#')
                break;

        size_t len = (size_t)(close_at - open_at - 1);
        char *held = malloc(len + 1);
        if (!held)
            die_parse(st->path, st->line, "out of memory");
        memcpy(held, open_at + 1, len);
        held[len] = 0;

        in->is_waiting = 1;
        
        in->text = held;
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
        
        in->text = NULL;
    } else {
        die_parse(st->path, st->line,
                  "expected '$entry', '= value', '[values]', '-', or a "
                  "depth alone; a "
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

    /*
     * **`$N` says this port is the map's result N** (issue 601b), the
     * mirror of the same mark on an input line. The `in` or `out`
     * keyword carries the direction, so one notation covers both ends
     * and there is no second form to learn.
     */
    if (strcmp(dash, "-") != 0)
        die_parse(st->path, st->line,
                  "expected '-' between the port and where its values go");

    size_t dest_len = strlen(dest);
    if (dest_len > 1 && dest[dest_len - 1] == '$') {
        /* **`0$` says this port is the map's result 0** (issue 601b),
         * the mirror of the same mark on an input line and in the same
         * place: after the dash, which is where a port's values go. */
        if (extra[0])
            die_parse(st->path, st->line,
                      "unexpected trailing words after a result number");
        dest[dest_len - 1] = 0;
        desc_output_t *mark = need(calloc(1, sizeof *mark), st->path,
                                   st->line);
        mark->port = port;
        mark->line = st->line;
        mark->is_result = 1;
        if (!parse_number(dest, &mark->result) || mark->result < 0)
            die_parse(st->path, st->line,
                      "expected a result number before '$', as "
                      "'out 0 - 0$'");
        desc_output_t **at = &st->current->outputs;
        while (*at)
            at = &(*at)->next;
        *at = mark;
        return;
    }

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

/* {{{ handle_station() */
static void handle_station(parse_state_t *st, const char *name,
                           const char *rest)
{
    char box[128], kind_word[8], door_word[16], extra[16], beyond[8];
    rest = next_word(rest, box, sizeof box);
    rest = next_word(rest, kind_word, sizeof kind_word);
    rest = next_word(rest, door_word, sizeof door_word);
    rest = next_word(rest, extra, sizeof extra);
    next_word(rest, beyond, sizeof beyond);

    if (!box[0] || !kind_word[0])
        die_parse(st->path, st->line,
                  "a station line is three words: name, box function, kind (p/c/i)");
    /* Two optional words may follow the kind — a door and an
     * iterator's position — so a third is one too many. */
    if (beyond[0])
        die_parse(st->path, st->line, "unexpected trailing words on a station line");

    /* The kind is written rather than inferred: forgetting a
     * threshold line must be an error, never a silent demotion to a
     * plain box that routes everything one way (issue 601). One
     * letter, dispatched. */
    int kind;
    if (strcmp(kind_word, "p") == 0)      kind = CERA_STATION_PLAIN;
    else if (strcmp(kind_word, "c") == 0) kind = CERA_STATION_COMPARATOR;
    else if (strcmp(kind_word, "i") == 0) kind = CERA_STATION_ITERATOR;
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
    /*
     * **And an optional `@N`, which is where an iterator is pointing**
     * (issue 712) — the one memory a station keeps. Every other word
     * on this line says what the station *is*; this says where it had
     * got to, which is what makes a written-down program an image
     * rather than only a schematic.
     *
     * Either order, because there is no reading of `result @2` that
     * differs from `@2 result` and making somebody remember which
     * comes first buys nothing.
     */
    int cursor = 0;
    const char *trailing[2] = { door_word, extra };
    for (int t = 0; t < 2; t++) {
        const char *word = trailing[t];
        if (!word[0])
            continue;
        if (word[0] == '@') {
            if (cursor)
                die_parse(st->path, st->line,
                          "a station line says '@' twice, and a station has "
                          "one place it had got to");
            if (kind != CERA_STATION_ITERATOR)
                die_parse(st->path, st->line,
                          "'@' says where an iterator is pointing, and this "
                          "station is not one — only kind 'i' takes its "
                          "exits in turn");
            if (!parse_number(word + 1, &cursor) || cursor < 0)
                die_parse(st->path, st->line,
                          "expected an exit number after '@'");
            /* A cursor of zero is where an iterator starts, so the
             * dump never writes it and a file saying so has said
             * nothing. Recorded as one anyway rather than refused:
             * writing it down is not wrong, only redundant. */
            continue;
        }
        /*
         * **A door is a port now** (issues 213a, 601b), so a station
         * line no longer carries one. The two words that used to sit
         * here are named in the refusal because a file written before
         * the change will have them, and "unexpected word" would send
         * somebody looking for a typo they did not make.
         */
        if (strcmp(word, "entry") == 0 || strcmp(word, "result") == 0)
            die_parse(st->path, st->line,
                      "'entry' and 'result' marked a whole station, and a "
                      "door is a port now — write '$0' on the port itself, "
                      "as 'in 0 $0' or 'out 0 $0'");
        die_parse(st->path, st->line,
                  "after the kind, only '@N' (where an iterator had got "
                  "to)");
    }

    for (desc_station_t *s = st->d->stations; s; s = s->next)
        if (strcmp(s->name, name) == 0)
            die_parse(st->path, st->line, "a station with this name already exists");

    desc_station_t *s = need(calloc(1, sizeof *s), st->path, st->line);
    s->name = copy_string(name, st->path, st->line);
    s->box = copy_string(box, st->path, st->line);
    s->kind = kind;
    s->cursor = cursor;
    s->line = st->line;

    *st->station_tail = s;
    st->station_tail = &s->next;
    st->d->n_stations++;
    st->current = s;
}
/* }}} */

/* {{{ line_scan() */
/*
 * **One walk over a physical line that answers both questions the
 * reader has**: where a comment starts, and how much the braces
 * opened or closed. Returns the net brace change; writes the comment's
 * offset into `comment_at`, or -1 when there is none.
 *
 * One walk rather than two because both questions have the same
 * awkward case — **a quoted string, where a `#` is a character and a
 * `{` is a character**. Finding the comment by searching for the first
 * `#` anywhere was wrong before this and quietly truncated any value
 * containing one; counting braces the same way would break every
 * struct holding text with a brace in it. Tracking the quote state
 * once answers both correctly and cannot drift between them.
 *
 * A backslash inside a string hides whatever follows it, so a quote
 * that was escaped does not end the string.
 */
static int line_scan(const char *line, int *comment_at)
{
    int depth = 0;
    int inside = 0;
    *comment_at = -1;

    for (int i = 0; line[i]; i++) {
        char c = line[i];
        if (inside) {
            if (c == '\\' && line[i + 1])
                i++;
            else if (c == '"')
                inside = 0;
            continue;
        }
        if (c == '"')       inside = 1;
        else if (c == '{')  depth++;
        else if (c == '}')  depth--;
        else if (c == '#') { *comment_at = i; break; }
    }
    return depth;
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

    parse_state_t st = { 0 };
    st.d = d;
    st.station_tail = &d->stations;
    st.path = path;

    /*
     * **A logical line, assembled from as many physical ones as its
     * braces need.**
     *
     * `physical` is one line as the file has it; `logical` is what the
     * parser sees. They are usually the same, and differ exactly when a
     * struct value is spread over several lines to be readable.
     *
     * This does not weaken *the first word of a line is always a
     * keyword* (issue 607). A continuation is not a new line — it is
     * the same logical line still being assembled — and the keyword
     * rule was always about logical lines.
     */
    char physical[1024];
    char logical[8192];
    while (fgets(physical, sizeof physical, f)) {
        st.line++;
        int opened_at = st.line;

        size_t used = 0;
        int depth = 0;
        for (;;) {
            /*
             * **A physical line that does not end is refused rather
             * than split.** Filling the buffer with no newline means
             * the reader is about to hand the parser half a line and
             * then treat the rest as a fresh one — every line number
             * after it wrong, and whether anything is noticed at all
             * depending on where the cut happened to land. With
             * continuations in place this only fires on a single token
             * longer than the buffer, which is a file nobody meant.
             */
            size_t got = strlen(physical);
            if (got == sizeof physical - 1 && physical[got - 1] != '\n'
                && !feof(f))
                die_parse(path, st.line,
                          "this line is longer than the reader's 1023-byte "
                          "limit; a value that needs more room than that "
                          "belongs behind a box that reads it");

            int comment_at = -1;
            depth += line_scan(physical, &comment_at);
            if (comment_at >= 0)
                physical[comment_at] = 0;

            size_t len = strlen(physical);
            while (len > 0 && (physical[len - 1] == '\n'
                               || physical[len - 1] == '\r'
                               || physical[len - 1] == ' '
                               || physical[len - 1] == '\t'))
                physical[--len] = 0;

            /* Leading whitespace goes on a continuation, so a value
             * indented to line up with the one above reads as one
             * value rather than as text with gaps in it. The first
             * physical line keeps its own, since indentation there has
             * never meant anything either way. */
            const char *add = physical;
            if (used > 0)
                while (*add == ' ' || *add == '\t')
                    add++;

            size_t adding = strlen(add);
            if (used + adding >= sizeof logical)
                die_parse(path, opened_at,
                          "this value needs more room than one line can "
                          "hold, even continued");
            memcpy(logical + used, add, adding);
            used += adding;
            logical[used] = 0;

            if (depth <= 0)
                break;
            if (!fgets(physical, sizeof physical, f))
                die_parse(path, opened_at,
                          "a '{' opened here and the file ended before "
                          "anything closed it");
            st.line++;
        }

        char first[128];
        const char *rest = next_word(logical, first, sizeof first);
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
            /*
             * **The statics section is gone** (issue 601b). It let a
             * value be written once and pointed at by number, which
             * was a second spelling of a constant — and the dump
             * never wrote one, so a hand-written file and a dumped
             * one differed by notation that meant nothing.
             *
             * Named in the refusal rather than reported as an unknown
             * word, because a file written before the change will
             * have one and its author wants to be told what replaced
             * it, not sent hunting for a typo.
             */
            die_parse(path, st.line,
                      "the 'statics' section is gone — write each value on "
                      "the port that reads it, as 'in 1 = 5'");
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
                      "a line starts with 'station', 'in' or 'out'");
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
            /* Both null unless the line carried a value inline or
             * named a source; free copes either way. */
            free(in->text);
            free(in->source_station);
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
    free(d->path);
    free(d);
}
/* }}} */
