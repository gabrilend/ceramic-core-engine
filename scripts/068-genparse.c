/*
 * 068-genparse.c — reading a box source, from inside.
 *
 * What this is: the half of the generator that turns C text into a
 * description. It recognizes three top-level shapes — a typedef'd
 * struct, a function, and a function whose name ends in __compare —
 * and refuses everything else where it stands.
 *
 * How it does it, in general terms: comments, string contents and
 * preprocessor lines are blanked to spaces first, keeping every
 * newline so byte positions still map to line numbers; a brace inside
 * a comment or a string would otherwise throw off everything after
 * it. Then one pass stops at every top-level '{' and classifies the
 * declaration that led to it, using the text since the last ';' —
 * because complete declarations before this one (externs, globals)
 * end with a semicolon, and only what follows the last one leads to
 * this brace.
 *
 * The refusals are the point. A parser that quietly skips what it
 * does not understand emits a registry with a hole in it, and the
 * hole surfaces much later as an error pointing at somebody's map
 * (issue 301).
 */
#include "067-genparse.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ gp_fail() */
void gp_fail(const char *file, int line, const char *fmt, ...)
{
    fprintf(stderr, "generator: %s:%d: ", file ? file : "(input)", line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(65);   /* malformed input (issue 106) */
}
/* }}} */

/* {{{ the primitive table */
/*
 * What a primitive fundamentally is: the field kind the statics
 * reader will use, keyed by canonical type name. A table rather than
 * a chain of comparisons, like everything else here — and the same
 * set the Lua generator carried, because these strings are the
 * registry's type names and changing one silently changes what a
 * wire is checked against.
 */
typedef struct { const char *name; tkind_t kind; } prim_t;

static const prim_t PRIMITIVES[] = {
    { "char",               TKIND_INT   },
    { "signed char",        TKIND_INT   },
    { "unsigned char",      TKIND_UINT  },
    { "short",              TKIND_INT   },
    { "unsigned short",     TKIND_UINT  },
    { "int",                TKIND_INT   },
    { "unsigned",           TKIND_UINT  },
    { "unsigned int",       TKIND_UINT  },
    { "long",               TKIND_INT   },
    { "unsigned long",      TKIND_UINT  },
    { "long long",          TKIND_INT   },
    { "unsigned long long", TKIND_UINT  },
    { "float",              TKIND_FLOAT },
    { "double",             TKIND_FLOAT },
    { "int32_t",            TKIND_INT   },
    { "int64_t",            TKIND_INT   },
    { "uint32_t",           TKIND_UINT  },
    { "uint64_t",           TKIND_UINT  },
    { "size_t",             TKIND_UINT  },
};

/* A borrowed string: the bytes live wherever the pointer points,
 * which for a static is the port's own character storage. */
static const char *STRING_TYPES[] = { "const char *", "char *" };
/* }}} */

/* {{{ gp_primitive_kind() */
int gp_primitive_kind(const char *type, tkind_t *out)
{
    for (size_t i = 0; i < sizeof PRIMITIVES / sizeof PRIMITIVES[0]; i++) {
        if (strcmp(PRIMITIVES[i].name, type) == 0) {
            if (out)
                *out = PRIMITIVES[i].kind;
            return 1;
        }
    }
    return 0;
}
/* }}} */

/* {{{ static int is_string_type() */
static int is_string_type(const char *type)
{
    for (size_t i = 0; i < sizeof STRING_TYPES / sizeof STRING_TYPES[0]; i++)
        if (strcmp(STRING_TYPES[i], type) == 0)
            return 1;
    return 0;
}
/* }}} */

/* {{{ gp_init() / gp_free() */
void gp_init(description_t *d)
{
    d->arena = arena_new();
    vec_init(&d->structs,  sizeof(sdef_t));
    vec_init(&d->boxes,    sizeof(box_t));
    vec_init(&d->compares, sizeof(cmp_t));
}

void gp_free(description_t *d)
{
    vec_free(&d->structs);
    vec_free(&d->boxes);
    vec_free(&d->compares);
    arena_free(d->arena);
    d->arena = NULL;
}
/* }}} */

/* {{{ gp_struct_named() */
sdef_t *gp_struct_named(const description_t *d, const char *name)
{
    for (int i = 0; i < d->structs.n; i++) {
        sdef_t *s = vec_at(&d->structs, i);
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}
/* }}} */

/* {{{ gp_compare_for() */
const cmp_t *gp_compare_for(const description_t *d, const char *type)
{
    for (int i = 0; i < d->compares.n; i++) {
        const cmp_t *c = vec_at(&d->compares, i);
        if (strcmp(c->type, type) == 0)
            return c;
    }
    return NULL;
}
/* }}} */

/* {{{ gp_read_file() */
/*
 * One file into the arena, whole. **Exported rather than static
 * because the emitter wants the same bytes** (issue 311c): a box
 * source is read here to be parsed, and read again to be written out
 * as text the compiled program carries. Two readers with one error
 * message beats two readers with two.
 */
char *gp_read_file(arena_t *a, const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "generator: cannot open %s\n", path);
        exit(65);
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "generator: cannot measure %s\n", path);
        exit(65);
    }
    long size = ftell(f);
    if (size < 0) {
        fprintf(stderr, "generator: cannot measure %s\n", path);
        exit(65);
    }
    rewind(f);
    char *text = arena_alloc(a, (size_t)size + 1);
    size_t got = fread(text, 1, (size_t)size, f);
    fclose(f);
    text[got] = '\0';
    if (len_out)
        *len_out = got;
    return text;
}
/* }}} */

/* {{{ static void blank_noise() */
/*
 * Rewrites the text in place: comments, the insides of string and
 * character literals, and whole preprocessor lines become spaces,
 * with every newline kept so a byte offset still says which line it
 * is on.
 *
 * The quotes themselves survive while their contents do not, so a
 * declaration that mentions a string still looks like a declaration.
 * Preprocessor lines are blanked after the strings, so a '#' inside
 * text cannot be mistaken for a directive.
 */
static void blank_noise(char *text, size_t n, const char *path)
{
    size_t i = 0;
    while (i < n) {
        if (text[i] == '/' && i + 1 < n && text[i + 1] == '/') {
            while (i < n && text[i] != '\n')
                text[i++] = ' ';
        } else if (text[i] == '/' && i + 1 < n && text[i + 1] == '*') {
            size_t start = i;
            size_t j = i + 2;
            while (j + 1 < n && !(text[j] == '*' && text[j + 1] == '/'))
                j++;
            if (j + 1 >= n)
                gp_fail(path, gt_line_of(text, start),
                        "unterminated block comment");
            for (size_t k = start; k <= j + 1; k++)
                if (text[k] != '\n')
                    text[k] = ' ';
            i = j + 2;
        } else if (text[i] == '"' || text[i] == '\'') {
            char quote = text[i];
            size_t j = i + 1;
            while (j < n) {
                if (text[j] == '\\')
                    j += 2;
                else if (text[j] == quote)
                    break;
                else
                    j++;
            }
            /* Keep the quotes, blank what is between them. */
            for (size_t k = i + 1; k < j && k < n; k++)
                if (text[k] != '\n')
                    text[k] = ' ';
            i = (j < n) ? j + 1 : n;
        } else {
            i++;
        }
    }

    /* Whole preprocessor lines, now that no '#' can be inside a
     * string. A line counts as one when its first non-space
     * character is '#'. */
    size_t line_start = 0;
    for (size_t k = 0; k <= n; k++) {
        if (k == n || text[k] == '\n') {
            size_t p = line_start;
            while (p < k && (text[p] == ' ' || text[p] == '\t'))
                p++;
            if (p < k && text[p] == '#')
                for (size_t q = line_start; q < k; q++)
                    text[q] = ' ';
            line_start = k + 1;
        }
    }
}
/* }}} */

/* {{{ static long find_matching_brace() */
static long find_matching_brace(const char *text, size_t n, size_t open_pos)
{
    int depth = 0;
    for (size_t i = open_pos; i < n; i++) {
        if (text[i] == '{') {
            depth++;
        } else if (text[i] == '}') {
            depth--;
            if (depth == 0)
                return (long)i;
        }
    }
    return -1;
}
/* }}} */

/* {{{ static int trailing_ident() */
/*
 * The longest run of identifier characters ending at the end of a
 * span, returning its start or -1 when there is none. This is how a
 * name is separated from what precedes it, and it has to be done by
 * hand rather than by one pattern because the name may follow a star
 * with no space — "char *sneaky" — which no single split can say.
 */
static int trailing_ident(const char *s, size_t len)
{
    size_t end = len;
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t'
                       || s[end - 1] == '\n' || s[end - 1] == '\r'))
        end--;
    size_t start = end;
    while (start > 0 && gt_is_ident((unsigned char)s[start - 1]))
        start--;
    if (start == end)
        return -1;
    return (int)start;
}
/* }}} */

/* {{{ static int ident_end() */
/* Where the trailing identifier stops, ignoring trailing space. */
static size_t ident_end(const char *s, size_t len)
{
    size_t end = len;
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t'
                       || s[end - 1] == '\n' || s[end - 1] == '\r'))
        end--;
    return end;
}
/* }}} */

/* {{{ static int starts_with_word() */
/*
 * True when the span begins, after any whitespace, with the given
 * word followed by a non-identifier character. Sets *after to just
 * past the word.
 */
static int starts_with_word(const char *s, size_t len, const char *word,
                            size_t *after)
{
    size_t i = 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n'
                       || s[i] == '\r'))
        i++;
    size_t w = strlen(word);
    if (len - i < w || strncmp(s + i, word, w) != 0)
        return 0;
    if (i + w < len && gt_is_ident((unsigned char)s[i + w]))
        return 0;
    *after = i + w;
    return 1;
}
/* }}} */

/* {{{ static void parse_fields() */
/*
 * One field per declaration, split on semicolons. A comma is refused
 * rather than handled: "float x, y;" hides the field order this table
 * exists to state, and the offsets are the whole point.
 */
static void parse_fields(description_t *d, sdef_t *s, const char *body,
                         size_t body_len, const char *path, int line)
{
    for (size_t i = 0; i < body_len; i++)
        if (body[i] == '{')
            gp_fail(path, line,
                    "nested struct bodies are refused — define the inner "
                    "struct separately and refer to it by name");

    vec_t fields;
    vec_init(&fields, sizeof(field_t));

    size_t start = 0;
    for (size_t i = 0; i <= body_len; i++) {
        if (i != body_len && body[i] != ';')
            continue;
        const char *entry = body + start;
        size_t elen = i - start;
        start = i + 1;
        if (gt_all_space(entry, elen))
            continue;

        for (size_t k = 0; k < elen; k++)
            if (entry[k] == ',')
                gp_fail(path, line,
                        "one field per declaration — 'float x, y;' hides "
                        "the field order this table exists to state");

        /* An array suffix, if any: the digits inside a trailing
         * [ ... ]. The core is everything before it. */
        int array_len = 0;
        size_t core_len = elen;
        size_t e = ident_end(entry, elen);
        if (e > 0 && entry[e - 1] == ']') {
            size_t ob = e - 1;
            while (ob > 0 && entry[ob] != '[')
                ob--;
            if (entry[ob] == '[') {
                char digits[32];
                size_t dn = 0;
                for (size_t k = ob + 1; k < e - 1 && dn < sizeof digits - 1; k++)
                    if (entry[k] >= '0' && entry[k] <= '9')
                        digits[dn++] = entry[k];
                digits[dn] = '\0';
                array_len = dn ? atoi(digits) : 0;
                core_len = ob;
            }
        }

        int name_at = trailing_ident(entry, core_len);
        if (name_at < 0)
            gp_fail(path, line, "cannot read struct field: '%s'",
                    gt_trim(d->arena, entry, elen));
        size_t name_end = ident_end(entry, core_len);
        char *name = arena_strndup(d->arena, entry + name_at,
                                   name_end - (size_t)name_at);
        char *type = gt_normalize_type(d->arena, entry, (size_t)name_at);
        if (type[0] == '\0')
            gp_fail(path, line, "cannot read struct field: '%s'",
                    gt_trim(d->arena, entry, elen));

        field_t *f = vec_push(&fields);
        f->type = type;
        f->name = name;
        f->array_len = array_len;
    }

    /* Copied into the arena so the vector can be freed and the
     * addresses stay valid for the rest of the run. */
    s->n_fields = fields.n;
    s->fields = arena_alloc(d->arena, sizeof(field_t) * (size_t)(fields.n + 1));
    for (int i = 0; i < fields.n; i++)
        s->fields[i] = *(field_t *)vec_at(&fields, i);
    vec_free(&fields);
}
/* }}} */

/* {{{ static void parse_params() */
static void parse_params(description_t *d, box_t *b, const char *inner,
                         size_t len, const char *path, int line)
{
    char *trimmed = gt_trim(d->arena, inner, len);
    if (trimmed[0] == '\0' || strcmp(trimmed, "void") == 0) {
        b->n_params = 0;
        b->params = NULL;
        return;
    }

    vec_t params;
    vec_init(&params, sizeof(param_t));

    size_t start = 0;
    size_t n = strlen(trimmed);
    for (size_t i = 0; i <= n; i++) {
        if (i != n && trimmed[i] != ',')
            continue;
        const char *piece = trimmed + start;
        size_t plen = i - start;
        start = i + 1;

        int name_at = trailing_ident(piece, plen);
        size_t name_end = ident_end(piece, plen);
        if (name_at <= 0 || gt_all_space(piece, (size_t)name_at))
            gp_fail(path, line,
                    "cannot read parameter: '%s' — parameters must be named",
                    gt_trim(d->arena, piece, plen));

        param_t *p = vec_push(&params);
        p->name = arena_strndup(d->arena, piece + name_at,
                                name_end - (size_t)name_at);
        p->type = gt_normalize_type(d->arena, piece, (size_t)name_at);
    }

    b->n_params = params.n;
    b->params = arena_alloc(d->arena, sizeof(param_t) * (size_t)(params.n + 1));
    for (int i = 0; i < params.n; i++)
        b->params[i] = *(param_t *)vec_at(&params, i);
    vec_free(&params);
}
/* }}} */

/* {{{ gp_parse_file() */
void gp_parse_file(description_t *d, const char *path)
{
    size_t n = 0;
    char *text = gp_read_file(d->arena, path, &n);
    char *file = arena_strdup(d->arena, path);
    blank_noise(text, n, file);

    size_t pos = 0;
    size_t segment_start = 0;

    while (pos < n) {
        const char *brace = memchr(text + pos, '{', n - pos);
        if (!brace)
            break;
        size_t open = (size_t)(brace - text);

        /* Complete declarations before this one end with ';'. Only
         * what follows the last one leads to this brace. */
        const char *header = text + segment_start;
        size_t hlen = open - segment_start;
        size_t after = 0;
        for (size_t i = 0; i < hlen; i++)
            if (header[i] == ';')
                after = i + 1;
        const char *decl = header + after;
        size_t dlen = hlen - after;

        int line = gt_line_of(text, open);
        long close = find_matching_brace(text, n, open);
        if (close < 0)
            gp_fail(file, line, "unbalanced braces from here to end of file");

        /* --- typedef struct [tag] { ... } name; ------------------ */
        size_t p = 0;
        if (starts_with_word(decl, dlen, "typedef", &p)) {
            size_t q = 0;
            if (!starts_with_word(decl + p, dlen - p, "struct", &q))
                gp_fail(file, line,
                        "a typedef before a brace must be 'typedef struct "
                        "{ ... } name;'");
            /* Whatever follows may only be an optional tag. */
            char *rest = gt_trim(d->arena, decl + p + q, dlen - p - q);
            for (char *c = rest; *c; c++)
                if (!gt_is_ident((unsigned char)*c))
                    gp_fail(file, line,
                            "unrecognized declaration before this brace: "
                            "'%s' — box sources hold typedef structs and "
                            "functions only",
                            gt_trim(d->arena, decl, dlen));

            /* The name sits between the closing brace and the ';'. */
            size_t after_close = (size_t)close + 1;
            size_t nm = after_close;
            while (nm < n && (text[nm] == ' ' || text[nm] == '\n'
                              || text[nm] == '\t' || text[nm] == '\r'))
                nm++;
            size_t nme = nm;
            while (nme < n && gt_is_ident((unsigned char)text[nme]))
                nme++;
            size_t semi = nme;
            while (semi < n && (text[semi] == ' ' || text[semi] == '\n'
                                || text[semi] == '\t' || text[semi] == '\r'))
                semi++;
            if (nme == nm || semi >= n || text[semi] != ';')
                gp_fail(file, line,
                        "typedef struct with no name after the closing brace");

            sdef_t *s = vec_push(&d->structs);
            s->name = arena_strndup(d->arena, text + nm, nme - nm);
            s->file = file;
            s->line = line;
            parse_fields(d, s, text + open + 1, (size_t)close - open - 1,
                         file, line);

            pos = semi + 1;
            segment_start = pos;
            continue;
        }

        /* --- plain struct tag { ... } ---------------------------- */
        if (starts_with_word(decl, dlen, "struct", &p)) {
            char *tag = gt_trim(d->arena, decl + p, dlen - p);
            int all_ident = tag[0] != '\0';
            for (char *c = tag; *c; c++)
                if (!gt_is_ident((unsigned char)*c))
                    all_ident = 0;
            if (all_ident)
                gp_fail(file, line,
                        "plain 'struct %s' is refused — box value types must "
                        "be 'typedef struct { ... } name;' so maps can name "
                        "them", tag);
        }

        /* --- a function ------------------------------------------ */
        /*
         * The header ends with a balanced parenthesis group, and the
         * name is the identifier just before it. Found by scanning
         * back from the last ')' rather than by one pattern, because
         * the name may follow a star with no space.
         */
        size_t de = ident_end(decl, dlen);
        if (de > 0 && decl[de - 1] == ')') {
            int depth = 0;
            long op = -1;
            for (size_t i = de; i-- > 0; ) {
                if (decl[i] == ')') {
                    depth++;
                } else if (decl[i] == '(') {
                    depth--;
                    if (depth == 0) { op = (long)i; break; }
                }
            }
            if (op >= 0) {
                int name_at = trailing_ident(decl, (size_t)op);
                if (name_at > 0) {
                    char before = decl[name_at - 1];
                    if (before == '*' || before == ' ' || before == '\t'
                        || before == '\n' || before == '\r') {
                        size_t name_end = ident_end(decl, (size_t)op);
                        char *fn_name = arena_strndup(d->arena, decl + name_at,
                                                      name_end - (size_t)name_at);
                        char *ret = gt_normalize_type(d->arena, decl,
                                                      (size_t)name_at);

                        /* static and inline are qualifiers on the
                         * declaration, not part of the type. */
                        int is_static = 0;
                        size_t skip = 0;
                        for (;;) {
                            size_t q = 0;
                            if (starts_with_word(ret + skip, strlen(ret + skip),
                                                 "static", &q)) {
                                is_static = 1;
                                skip += q;
                            } else if (starts_with_word(ret + skip,
                                                        strlen(ret + skip),
                                                        "inline", &q)) {
                                skip += q;
                            } else {
                                break;
                            }
                        }
                        ret = gt_normalize_type(d->arena, ret + skip,
                                                strlen(ret + skip));

                        const char *inner = decl + op + 1;
                        size_t ilen = (size_t)(de - 1) - (size_t)(op + 1);

                        /* type__compare is that type's ordering and is
                         * never a box. */
                        size_t fl = strlen(fn_name);
                        const char *suffix = "__compare";
                        size_t sl = strlen(suffix);
                        if (fl > sl && strcmp(fn_name + fl - sl, suffix) == 0) {
                            char *ctype = arena_strndup(d->arena, fn_name,
                                                        fl - sl);
                            box_t probe;
                            memset(&probe, 0, sizeof probe);
                            parse_params(d, &probe, inner, ilen, file, line);
                            if (strcmp(ret, "int") != 0 || probe.n_params != 2
                                || strcmp(probe.params[0].type, ctype) != 0
                                || strcmp(probe.params[1].type, ctype) != 0)
                                gp_fail(file, line,
                                        "%s must be: int %s__compare(%s a, "
                                        "%s b)", fn_name, ctype, ctype, ctype);
                            cmp_t *c = vec_push(&d->compares);
                            c->type = ctype;
                            c->file = file;
                            c->line = line;
                        } else if (!is_static) {
                            box_t *b = vec_push(&d->boxes);
                            b->name = fn_name;
                            b->file = file;
                            b->line = line;
                            b->ret  = ret;
                            parse_params(d, b, inner, ilen, file, line);
                        }
                        /* A static function is a private helper:
                         * visible to its boxes, invisible to maps, and
                         * deliberately not an error. */

                        pos = (size_t)close + 1;
                        segment_start = pos;
                        continue;
                    }
                }
            }
        }

        gp_fail(file, line,
                "unrecognized declaration before this brace: '%s' — box "
                "sources hold typedef structs and functions only",
                gt_trim(d->arena, decl, dlen > 60 ? 60 : dlen));
    }
}
/* }}} */

/* {{{ static tkind_t classify_type() */
/*
 * What a type is to the engine: a primitive kind, a known struct, a
 * borrowed string, or a mistake.
 */
static tkind_t classify_type(const description_t *d, const char *t,
                             sdef_t **nested_out)
{
    if (nested_out)
        *nested_out = NULL;
    tkind_t kind;
    if (gp_primitive_kind(t, &kind))
        return kind;
    if (is_string_type(t))
        return TKIND_STRING_PTR;
    sdef_t *s = gp_struct_named(d, t);
    if (s) {
        if (nested_out)
            *nested_out = s;
        return TKIND_STRUCT;
    }
    return TKIND_UNKNOWN;
}
/* }}} */

/* {{{ gp_validate() */
void gp_validate(description_t *d)
{
    for (int i = 0; i < d->structs.n; i++) {
        sdef_t *s = vec_at(&d->structs, i);
        for (int j = 0; j < i; j++) {
            sdef_t *other = vec_at(&d->structs, j);
            if (strcmp(s->name, other->name) == 0)
                gp_fail(s->file, s->line, "struct '%s' defined twice", s->name);
        }
    }

    for (int i = 0; i < d->structs.n; i++) {
        sdef_t *s = vec_at(&d->structs, i);
        for (int j = 0; j < s->n_fields; j++) {
            field_t *f = &s->fields[j];
            if (f->array_len) {
                if (strcmp(f->type, "char") != 0)
                    gp_fail(s->file, s->line,
                            "field '%s': only char arrays (fixed strings) are "
                            "supported as array fields", f->name);
                f->kind = TKIND_STRING;
            } else {
                sdef_t *nested = NULL;
                tkind_t kind = classify_type(d, f->type, &nested);
                if (kind == TKIND_UNKNOWN || kind == TKIND_STRING_PTR)
                    gp_fail(s->file, s->line,
                            "field '%s' has type '%s', which the statics "
                            "reader cannot fill — use a primitive, a known "
                            "struct, or a char array", f->name, f->type);
                f->kind = kind;
                f->nested = nested;
            }
        }
    }

    for (int i = 0; i < d->boxes.n; i++) {
        box_t *b = vec_at(&d->boxes, i);
        for (int j = 0; j < i; j++) {
            box_t *other = vec_at(&d->boxes, j);
            if (strcmp(b->name, other->name) == 0)
                gp_fail(b->file, b->line, "box '%s' defined twice", b->name);
        }
        for (int j = 0; j < b->n_params; j++) {
            param_t *p = &b->params[j];
            tkind_t kind = classify_type(d, p->type, NULL);
            if (kind == TKIND_UNKNOWN)
                gp_fail(b->file, b->line,
                        "box '%s' parameter '%s' has unknown type '%s'",
                        b->name, p->name, p->type);
            p->kind = kind;
        }
        if (strcmp(b->ret, "void") != 0) {
            tkind_t kind = classify_type(d, b->ret, NULL);
            if (kind == TKIND_UNKNOWN)
                gp_fail(b->file, b->line,
                        "box '%s' returns unknown type '%s'", b->name, b->ret);
            if (kind == TKIND_STRING_PTR)
                gp_fail(b->file, b->line,
                        "box '%s' returns a string pointer — returning "
                        "borrowed memory through a wire has no owner; return "
                        "a struct with a char array instead", b->name);
            b->ret_kind = kind;
        }
    }
}
/* }}} */

/* {{{ gp_describe() */
void gp_describe(const description_t *d)
{
    for (int i = 0; i < d->structs.n; i++) {
        const sdef_t *s = vec_at(&d->structs, i);
        printf("struct %s  (%s:%d)\n", s->name, s->file, s->line);
        for (int j = 0; j < s->n_fields; j++) {
            const field_t *f = &s->fields[j];
            if (f->array_len)
                printf("  %-12s %s[%d]\n", f->name, f->type, f->array_len);
            else
                printf("  %-12s %s\n", f->name, f->type);
        }
    }
    for (int i = 0; i < d->boxes.n; i++) {
        const box_t *b = vec_at(&d->boxes, i);
        printf("box %s(", b->name);
        for (int j = 0; j < b->n_params; j++)
            printf("%s%s %s", j ? ", " : "", b->params[j].type,
                   b->params[j].name);
        printf(") -> %s  (%s:%d)\n", b->ret, b->file, b->line);
    }
    for (int i = 0; i < d->compares.n; i++) {
        const cmp_t *c = vec_at(&d->compares, i);
        printf("compare for %s  (%s:%d)\n", c->type, c->file, c->line);
    }
}
/* }}} */
