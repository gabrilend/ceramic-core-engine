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
 * it, at its own declared type, into its own storage — a number for
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
#include "026-emitted.h"

#include "091-stopping.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Where an error happened. Published as sora_where_t (issue 408),
 * because generated readers name the same place, and spelled `where_t`
 * here so the file that has always used the short name still reads
 * the way it did.
 */
typedef sora_where_t where_t;

/* {{{ die_static() */
static void die_static(const where_t *w, const char *what)
{
    /*
     * **Exit 70 rather than a core dump** (issue 106): text that will
     * not parse is a fault in the calling code, which is a caller can
     * correct and retry — and that is a different thing from running
     * out of memory, which no edit fixes. A shell script can now tell
     * them apart; before, both arrived as the same abnormal death.
     *
     * It still stops where the malformed value is rather than handing
     * a refusal back to be collected with others, and that is not an
     * omission. A value that will not parse is found while the
     * station holding it is being built, and every other fault found
     * at that moment stops there too — because continuing past a
     * station that could not be built means asking questions of
     * something that is not there. What accumulates is the
     * whole-program pass, which runs when everything exists and can
     * therefore report every fault at once.
     */
    char said[512];
    snprintf(said, sizeof said, "statics: station %d port %d: %s",
             w->station, w->port, what);
    sora_stop_now(NULL, SORA_EXIT_BAD_CALL, said);
}
/* }}} */

/* {{{ the escape table, and the two routines that share it — issue 408 */
/*
 * **One table, two directions**, so the writer and the reader cannot
 * disagree about what a backslash introduces.
 *
 * Before this, a string was written out raw and read back by
 * searching for the next quote. A value holding a quote ended its own
 * text early; a value holding a tab or a newline produced a map file
 * with a line break inside a line; a byte above 0x7F went out as
 * whatever the reader's locale made of it. All three are values the
 * engine will happily hold and could not write down — which makes it
 * a correctness hole rather than a matter of polish, because the dump
 * claims to round-trip.
 *
 * **Five named escapes and a hexadecimal form**, and the split is
 * deliberate. The five are the ones a person reading a map should see
 * spelled the way they already know them. Everything else
 * unprintable, and everything from 0x80 up, goes as `\xNN` — because
 * a byte with no agreed spelling is better shown as its number than
 * as a character somebody's terminal invented.
 *
 * **The hexadecimal form is exactly two digits, always.** C's own
 * `\x` consumes as many as it can find, so `"\x41" "2"` and
 * `"\x412"` mean different things and one of them is a compile
 * error — a footgun worth not inheriting. Two digits covers every
 * byte and never runs on into the next character.
 */
static const struct { char spelled; unsigned char is; } escapes[] = {
    { '"',  '"'  },
    { '\\', '\\' },
    { 'n',  '\n' },
    { 't',  '\t' },
    { 'r',  '\r' },
};

/* {{{ static const char *read_quoted() */
/*
 * The other direction, reading from `p` — which must be sitting on
 * the opening quote — into at most `room` bytes, and saying how many
 * arrived. Returns the position just past the closing quote.
 *
 * `what` names the thing being read, so a refusal can say which field
 * or which port was wrong rather than only that something was.
 */
static const char *read_quoted(const char *p, char *out, int room,
                               int *len_out, const char *what,
                               const where_t *w)
{
    char note[192];
    if (*p != '"') {
        snprintf(note, sizeof note, "%s wants a quoted string", what);
        die_static(w, note);
    }
    p++;

    int len = 0;
    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            if (!*p) {
                snprintf(note, sizeof note,
                         "%s: a backslash at the end of the text", what);
                die_static(w, note);
            }
            char spelled = *p++;
            int named = 0;
            for (size_t e = 0; e < sizeof escapes / sizeof *escapes; e++)
                if (spelled == escapes[e].spelled) {
                    c = escapes[e].is;
                    named = 1;
                    break;
                }
            if (!named) {
                if (spelled != 'x') {
                    snprintf(note, sizeof note,
                             "%s: '\\%c' is not an escape this format has",
                             what, spelled);
                    die_static(w, note);
                }
                int value = 0;
                for (int d = 0; d < 2; d++) {
                    char h = *p++;
                    int digit;
                    if (h >= '0' && h <= '9')      digit = h - '0';
                    else if (h >= 'a' && h <= 'f') digit = h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') digit = h - 'A' + 10;
                    else {
                        snprintf(note, sizeof note,
                                 "%s: '\\x' wants exactly two hexadecimal "
                                 "digits", what);
                        die_static(w, note);
                        return p;
                    }
                    value = value * 16 + digit;
                }
                c = (unsigned char)value;
            }
        }
        if (len >= room) {
            snprintf(note, sizeof note,
                     "%s holds %d characters and more were given",
                     what, room);
            die_static(w, note);
        }
        out[len++] = (char)c;
    }

    if (*p != '"') {
        snprintf(note, sizeof note, "%s: unterminated string", what);
        die_static(w, note);
    }
    *len_out = len;
    return p + 1;
}
/* }}} */
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

/* {{{ classify_port() */
/*
 * What kind of thing this port holds, and — when it is a struct —
 * where its reader and writer are.
 *
 * **The struct is not searched for.** The port was handed the address
 * of its pair at placement, because the placement function knew the
 * type concretely (issues 311b, 408); this used to scan every emitted
 * struct table looking for a matching name.
 *
 * The primitives are still told apart by their spelling, and that is
 * a different act from comparing two types: a wire is legal on
 * **width** alone, because two boxes may spell one shape differently
 * and mean the same data (issue 309). Nothing here compares one type
 * against another. It asks how to turn text into bytes, which needs
 * to know whether those bytes are a number, and which kind.
 */
static type_class_t classify_port(const in_port_t *sl,
                                  const struct_text_t **out_struct)
{
    const char *tn = sl->type_name ? sl->type_name : "";
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

    if (sl->text) {
        if (out_struct) *out_struct = sl->text;
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
typedef sora_textbuf_t textbuf_t;

static void tb_addf(textbuf_t *tb, const char *fmt, ...);

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

/* {{{ static void write_quoted() */
/*
 * `len` bytes, written as a quoted string with everything escaped
 * that has to be. The length is given rather than found, because a
 * value may legitimately contain a zero byte and a char array field
 * filled exactly to its width has no room for a terminator.
 */
static void write_quoted(textbuf_t *tb, const char *bytes, int len)
{
    tb_addf(tb, "\"");
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)bytes[i];
        int named = 0;
        for (size_t e = 0; e < sizeof escapes / sizeof *escapes; e++)
            if (c == escapes[e].is) {
                tb_addf(tb, "\\%c", escapes[e].spelled);
                named = 1;
                break;
            }
        if (named)
            continue;
        if (c < 0x20 || c >= 0x7F)
            tb_addf(tb, "\\x%02x", c);
        else
            tb_addf(tb, "%c", (char)c);
    }
    tb_addf(tb, "\"");
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


/* {{{ the helpers a generated reader and writer call — issue 408 */
/*
 * **One grammar, shared; one routine per struct, emitted.**
 *
 * A struct's reader and writer are generated now, reaching each field
 * by name with its size a `sizeof` at the point of use. What is *not*
 * generated is any of this: the escape rules, the number widths, the
 * refusals. Those are the same for every struct, so emitting them per
 * type would be one grammar written N times and N places for it to
 * drift.
 *
 * So the shape is the opposite of what it looks like at first glance:
 * the part that varies by type is generated, and the part that does
 * not is written once, here, and called.
 */
const char *sora_text_expect(const char *p, char c, const sora_where_t *w,
                             const char *what)
{
    p = skip_ws(p);
    if (*p != c) {
        char note[192];
        snprintf(note, sizeof note, "expected '%c' %s", c, what);
        die_static(w, note);
    }
    return skip_ws(p + 1);
}

const char *sora_text_signed(const char *p, void *out, int size,
                             const sora_where_t *w, const char *field)
{
    p = skip_ws(p);
    char *end;
    long long v = strtoll(p, &end, 0);
    if (end == p) {
        char note[192];
        snprintf(note, sizeof note, "field '%s' wants a number", field);
        die_static(w, note);
    }
    write_integer(v, (unsigned char *)out, size, w);
    return skip_ws(end);
}

const char *sora_text_unsigned(const char *p, void *out, int size,
                               const sora_where_t *w, const char *field)
{
    p = skip_ws(p);
    char *end;
    unsigned long long v = strtoull(p, &end, 0);
    if (end == p) {
        char note[192];
        snprintf(note, sizeof note, "field '%s' wants a number", field);
        die_static(w, note);
    }
    write_unsigned(v, (unsigned char *)out, size, w);
    return skip_ws(end);
}

const char *sora_text_floating(const char *p, void *out, int size,
                               const sora_where_t *w, const char *field)
{
    p = skip_ws(p);
    char *end;
    double v = strtod(p, &end);
    if (end == p) {
        char note[192];
        snprintf(note, sizeof note, "field '%s' wants a number", field);
        die_static(w, note);
    }
    write_float(v, (unsigned char *)out, size, w);
    return skip_ws(end);
}

const char *sora_text_chars(const char *p, char *out, int room,
                            const sora_where_t *w, const char *field)
{
    char note[192];
    snprintf(note, sizeof note, "field '%s'", field);

    p = skip_ws(p);
    int len = 0;
    p = read_quoted(p, out, room, &len, note, w);
    /* Zero-padded to the full width rather than only terminated, so
     * that two structs holding the same text are the same bytes and a
     * comparison over raw bytes means what it looks like it means. */
    for (int i = len; i < room; i++)
        out[i] = '\0';
    return skip_ws(p);
}

void sora_text_put(sora_textbuf_t *tb, const char *literal)
{
    tb_addf(tb, "%s", literal);
}

void sora_text_put_signed(sora_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    tb_addf(tb, "%lld", read_integer((const unsigned char *)bytes, size, &w));
}

void sora_text_put_unsigned(sora_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    tb_addf(tb, "%llu", read_unsigned((const unsigned char *)bytes, size, &w));
}

void sora_text_put_floating(sora_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    float_text(tb, read_float((const unsigned char *)bytes, size, &w), size);
}

void sora_text_put_chars(sora_textbuf_t *tb, const char *chars, int room)
{
    /* Bounded by the array rather than trusted to a terminator,
     * because a field filled exactly to its width has no room for
     * one. */
    int len = 0;
    while (len < room && chars[len])
        len++;
    write_quoted(tb, chars, len);
}
/* }}} */

/* {{{ in_port_constant_text() */
int in_port_constant_text(const in_port_t *sl, char *out, int room)
{
    where_t w = { -1, -1 };   /* the port is the caller's to name here */
    textbuf_t tb = { out, room, 0 };
    if (room > 0)
        out[0] = 0;

    if (!sl->constant_set) {
        tb_addf(&tb, "?");
    } else {
        const struct_text_t *si = NULL;
        switch (classify_port(sl, &si)) {
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
             * own, which is what makes handing the pointer out sound.
             * Escaped on the way out (issue 408), so a constant
             * holding a quote does not end its own text early. */
            const char *s = sl->constant_string ? sl->constant_string : "";
            write_quoted(&tb, s, (int)strlen(s));
            break;
        }
        case TN_STRUCT:
            si->write(sl->constant, &tb);
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

/* {{{ in_port_constant_free() */
void in_port_constant_free(in_port_t *sl)
{
    free(sl->constant);
    free(sl->constant_string);
    sl->constant = NULL;
    sl->constant_string = NULL;
    sl->constant_set = 0;
}
/* }}} */

/* {{{ port_text_to_bytes() */
/*
 * **Text into the bytes one port's type wants**, which is the one
 * thing this file knows how to do and the reason two very different
 * callers share it.
 *
 * A constant written in a map file and an argument typed on a command
 * line are the same problem: somebody wrote a value down as
 * characters, and the layout it has to become is a fact the compiler
 * computed and the generator recorded. Pointing this at an argument
 * list instead of at a statics line is the same code, the same
 * offsets, and the same messages naming the field that was wrong
 * (issue 213).
 *
 * `into` is `elem_size` bytes the caller owns. `owned_string` comes
 * back non-null when the port is a string port, holding characters
 * the caller must keep alive for as long as anything can read the
 * pointer that was written into `into` — because a string value *is*
 * that pointer, and freeing what it points at is freeing something a
 * box may still be looking at.
 */
static void port_text_to_bytes(const in_port_t *sl, const char *text,
                               unsigned char *into, char **owned_string,
                               const where_t *w)
{
    unsigned char *fresh = into;
    char *fresh_string = NULL;

    const struct_text_t *si = NULL;
    switch (classify_port(sl, &si)) {
    case TN_INT: {
        char *end;
        long long v = strtoll(text, &end, 0);
        if (end == text)
            die_static(w, "an integer port wants a number");
        write_integer(v, fresh, sl->elem_size, w);
        break;
    }
    case TN_UINT: {
        char *end;
        unsigned long long v = strtoull(text, &end, 0);
        if (end == text)
            die_static(w, "an unsigned port wants a number");
        write_unsigned(v, fresh, sl->elem_size, w);
        break;
    }
    case TN_FLOAT: {
        char *end;
        double v = strtod(text, &end);
        if (end == text)
            die_static(w, "a floating port wants a number");
        write_float(v, fresh, sl->elem_size, w);
        break;
    }
    case TN_STRING: {
        /* The claimed value is a pointer; the characters live on the
         * port for the life of the map, which is what makes handing
         * the pointer to a box sound.
         *
         * **Quoted text goes through the shared escape routines**
         * (issue 408). Unquoted text is taken as itself, which is what
         * lets somebody write `in 0 config.txt` without ceremony — and
         * is why a value that needs escaping has to be quoted, because
         * an unquoted backslash is a backslash. */
        int len;
        if (*text == '"') {
            int room = (int)strlen(text);
            fresh_string = malloc((size_t)room + 1);
            if (!fresh_string)
                die_static(w, "out of memory for string storage");
            read_quoted(text, fresh_string, room, &len, "a string constant", w);
        } else {
            len = (int)strlen(text);
            fresh_string = malloc((size_t)len + 1);
            if (!fresh_string)
                die_static(w, "out of memory for string storage");
            memcpy(fresh_string, text, (size_t)len);
        }
        fresh_string[len] = 0;
        if (sl->elem_size != (int)sizeof(const char *))
            die_static(w, "a string port that is not pointer-sized");
        memcpy(fresh, &fresh_string, sizeof fresh_string);
        break;
    }
    case TN_STRUCT: {
        if (si->size != sl->elem_size)
            die_static(w, "struct size disagrees with the port");
        const char *after = si->read(text, fresh, w);
        if (*skip_ws(after) != 0)
            die_static(w, "trailing text after the struct value");
        break;
    }
    default:
        die_static(w, "the port's type is not one the reader knows");
    }

    *owned_string = fresh_string;
}
/* }}} */

/* {{{ map_in_port_static_text() */
void map_in_port_static_text(map_t *m, int station, int port, const char *text)
{
    where_t w = { station, port };

    if (station < 0 || station >= m->n_stations)
        die_static(&w, "giving a constant to a station outside the table");
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        die_static(&w, "giving a constant to a port the box does not have");
    in_port_t *sl = &s->in_ports[port];
    if (!sl->type_name)
        die_static(&w,
                   "the port has no declared type — a constant needs a station "
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
    port_text_to_bytes(sl, text, fresh, &fresh_string, &w);

    /* One of the four rare structural operations (issue 210): the
     * install and the tag together, under the station's mutex, so no
     * readiness walk and no claim sees a port mid-change. */
    pthread_mutex_lock(&s->mutex);
    char *old_string = sl->constant_string;
    memcpy(sl->constant, fresh, (size_t)sl->elem_size);
    sl->constant_string = fresh_string;
    sl->constant_set = 1;
    sl->kind = IN_PORT_STATIC;
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

/* {{{ map_deliver_argument_text() */
/*
 * **An argument written as text**, turned into the bytes the port
 * wants and delivered through the ordinary door (issue 213).
 *
 * This is the constant reader pointed somewhere else. Somebody typing
 * `{ 5, 2.0, "hey" }` on a command line and somebody writing it in a
 * map file are doing the same thing, so struct arguments in brace
 * syntax come along for free, with the same compiler-computed offsets
 * and the same messages naming the field that was wrong.
 *
 * **A string argument leaks, deliberately.** The value delivered for
 * a string port *is* a pointer, and whatever it points at has to
 * outlive every box that might read it — which is the whole run.
 * Freeing it would be freeing something a box may still be looking
 * at. One allocation per argument, released when the process is, is
 * the honest shape: a command line lives as long as the program does.
 */
const char *map_deliver_argument_text(map_t *m, int station, int port,
                                      const char *text)
{
    static _Thread_local char said[256];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said, "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    in_port_t *sl = &s->in_ports[port];
    if (!sl->type_name) {
        snprintf(said, sizeof said,
                 "station %d port %d has no declared type, so text has no "
                 "shape to become", station, port);
        return said;
    }
    if (!text) {
        snprintf(said, sizeof said, "an argument with no text");
        return said;
    }

    where_t w = { station, port };
    unsigned char *bytes = calloc(1, (size_t)sl->elem_size);
    if (!bytes)
        return "out of memory parsing an argument";

    char *owned = NULL;
    port_text_to_bytes(sl, text, bytes, &owned, &w);

    const char *no = map_deliver_argument(m, station, port, bytes,
                                          sl->elem_size);
    free(bytes);
    /* `owned` is not freed; see above. */
    return no;
}
/* }}} */

/* {{{ map_deliver_command_line() */
/*
 * **The command line, delivered into a program's entrances** (issue
 * 213).
 *
 * A program's arguments are the input ports of the stations it
 * declared as entrances, taken in station order and then in port
 * order. A program with two entrances of two ports each takes four
 * arguments, and which is which is a fact about the program that a
 * person reading its map file can see.
 *
 * **It holds a standing promise while it delivers and drops it
 * afterwards**, which is the rule the pool has always had for
 * anything outside the workers: without it the program can decide it
 * has finished between two arguments. Dropping it afterwards is what
 * lets a program whose arguments are all in actually end.
 *
 * A count that does not match is refused rather than partly
 * delivered, and the refusal says how many the program wanted. Half a
 * command line is a program that waits forever for the rest, which is
 * a worse way to learn about a typo than being told.
 */
const char *map_deliver_command_line(map_t *m, int argc, char **argv)
{
    static _Thread_local char said[256];

    int wanted = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (s->call && s->door == DOOR_IN)
            wanted += s->n_in_ports;
    }

    int given = argc > 0 ? argc - 1 : 0;
    if (given != wanted) {
        snprintf(said, sizeof said,
                 "this program takes %d argument%s and was given %d",
                 wanted, wanted == 1 ? "" : "s", given);
        return said;
    }

    /*
     * **Too late is said out loud rather than achieved quietly.**
     *
     * A program that seeds nothing has an empty queue, and until
     * somebody holds a standing promise it also has nobody promising
     * anything — so between the workers being released and the first
     * argument arriving, the last sleeper correctly decides the
     * program is over. Everything delivered afterwards is a task that
     * will never run, and the symptom is a program that did nothing
     * for no visible reason.
     *
     * The rule that prevents it is the pool's own and has not
     * changed: make the promise before opening the gate. This is not
     * a second mechanism for it — a registration taken here could not
     * close a window that opened before this was called. It is how
     * somebody finds out they left it open.
     */
    if (m->pool && pool_finished(m->pool)) {
        snprintf(said, sizeof said,
                 "this program had already finished before its arguments "
                 "arrived — something outside has to hold a standing "
                 "promise from before the workers are released until the "
                 "last argument is in");
        return said;
    }

    int taken = 0;
    const char *no = NULL;
    for (int i = 0; i < m->n_stations && !no; i++) {
        station_t *s = map_station(m, i);
        if (!s->call || s->door != DOOR_IN)
            continue;
        for (int j = 0; j < s->n_in_ports && !no; j++)
            no = map_deliver_argument_text(m, i, j, argv[1 + taken++]);
    }

    return no;
}
/* }}} */

/* {{{ static_write_under_lock() */
/*
 * The copy itself, as something the delivery path can be asked to do
 * while it holds the station's mutex (issue 210d). It exists as a
 * separate function only because that is how the work is handed over.
 */
typedef struct {
    in_port_t  *port;
    const void *bytes;
    int         size;
} static_write_t;

static void static_write_under_lock(void *ctx)
{
    static_write_t *j = ctx;
    memcpy(j->port->constant, j->bytes, (size_t)j->size);
}
/* }}} */

/* {{{ map_in_port_static_write() */
void map_in_port_static_write(map_t *m, int station, int port,
                           const void *bytes, int size)
{
    where_t w = { station, port };

    if (station < 0 || station >= m->n_stations)
        die_static(&w, "writing to a station outside the table");
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        die_static(&w, "writing to a port the box does not have");
    in_port_t *sl = &s->in_ports[port];
    if (!sl->constant_set)
        die_static(&w, "writing to a port that has never held a constant — "
                       "give it one as text first, so its shape is known");
    if (size != sl->elem_size)
        die_static(&w, "writing a value of the wrong size for this port");

    /* The copy happens under the station's own mutex — the one the
     * claim already takes. A struct half-overwritten while a claim is
     * copying it would yield fields from two different worlds, which
     * for anything wider than a machine word is not theoretical.
     *
     * **And it happens inside the same hold as the readiness check**
     * (issue 210d). Writing does not consume anything, so a station
     * that could already run runs again — which is how a value
     * computed once propagates through everything downstream of it.
     * Doing the copy and the check as two acquisitions would leave a
     * gap between the value changing and the question being asked, so
     * the copy is handed to the check to perform.
     *
     * Without a pool there is nothing to start, and the copy still has
     * to happen — a map being built is written into before it runs. */
    static_write_t job = { sl, bytes, size };
    if (m->pool) {
        map_station_start_after(m, station, static_write_under_lock, &job);
    } else {
        pthread_mutex_lock(&s->mutex);
        static_write_under_lock(&job);
        pthread_mutex_unlock(&s->mutex);
    }
}
/* }}} */
