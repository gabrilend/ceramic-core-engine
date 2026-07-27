/*
 * 033-statics.c — the values that are simply always there.
 *
 * What this is: the statics table — numbered constants a map's slots
 * can bind instead of being fed. Thresholds, file paths,
 * configuration. A static slot is always full, never consumed, and
 * never affects whether a station is ready; this file owns the table,
 * the text-to-bytes reader that fills it, and the one mutex that
 * makes altering it mid-run safe.
 *
 * How it does it, in general terms: an entry starts as the text the
 * map wrote. The first slot to bind it parses that text into bytes
 * shaped by the slot's own type — a number for an int slot, a brace
 * walk over the generated field table for a struct slot, the
 * characters themselves for a string slot. From then on a claim is a
 * locked memcpy and a runtime write is a locked, size-checked
 * overwrite. Anything malformed is fatal at bind time, naming the
 * entry and the field, because a static that half-parses is silent
 * corruption wearing a default.
 */
#include "018-station.h"
#include "026-registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The map the bare-name write reaches — how a box, which receives
 * only values, can touch the table at all. One live map per process
 * is the assumption; the first-pass report discusses the cost. */
map_t *sora_active_map;

/* {{{ die_static() */
static void die_static(int id, const char *what)
{
    fprintf(stderr, "statics: entry %d: %s\n", id, what);
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
static void write_integer(long long v, unsigned char *out, int size, int id)
{
    /* Each width is written through its own type so sign extension
     * and truncation are the compiler's, not arithmetic here. */
    switch (size) {
    case 1: { signed char x = (signed char)v;  memcpy(out, &x, 1); break; }
    case 2: { short x = (short)v;              memcpy(out, &x, 2); break; }
    case 4: { int x = (int)v;                  memcpy(out, &x, 4); break; }
    case 8: { long long x = v;                 memcpy(out, &x, 8); break; }
    default: die_static(id, "an integer field of a width the reader does not know");
    }
}

static void write_unsigned(unsigned long long v, unsigned char *out, int size, int id)
{
    switch (size) {
    case 1: { unsigned char x = (unsigned char)v;   memcpy(out, &x, 1); break; }
    case 2: { unsigned short x = (unsigned short)v; memcpy(out, &x, 2); break; }
    case 4: { unsigned x = (unsigned)v;             memcpy(out, &x, 4); break; }
    case 8: { unsigned long long x = v;             memcpy(out, &x, 8); break; }
    default: die_static(id, "an unsigned field of a width the reader does not know");
    }
}

static void write_float(double v, unsigned char *out, int size, int id)
{
    if (size == 4) { float x = (float)v; memcpy(out, &x, 4); }
    else if (size == 8) { memcpy(out, &v, 8); }
    else die_static(id, "a floating field of a width the reader does not know");
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
 * too few, or the wrong shape are all fatal here, naming entry and
 * field, because they are silent corruption if caught any later.
 */
static const char *parse_struct_text(const struct_info_t *si, const char *p,
                                     unsigned char *out, int id)
{
    p = skip_ws(p);
    if (*p != '{')
        die_static(id, "expected '{' to open a struct value");
    p = skip_ws(p + 1);

    for (int f = 0; f < si->n_fields; f++) {
        const field_info_t *fl = &si->fields[f];
        char where[128];

        switch (fl->kind) {
        case FIELD_INT: {
            char *end;
            long long v = strtoll(p, &end, 0);
            if (end == p) {
                snprintf(where, sizeof where, "field '%s' wants a number", fl->name);
                die_static(id, where);
            }
            write_integer(v, out + fl->offset, fl->size, id);
            p = end;
            break;
        }
        case FIELD_UINT: {
            char *end;
            unsigned long long v = strtoull(p, &end, 0);
            if (end == p) {
                snprintf(where, sizeof where, "field '%s' wants a number", fl->name);
                die_static(id, where);
            }
            write_unsigned(v, out + fl->offset, fl->size, id);
            p = end;
            break;
        }
        case FIELD_FLOAT: {
            char *end;
            double v = strtod(p, &end);
            if (end == p) {
                snprintf(where, sizeof where, "field '%s' wants a number", fl->name);
                die_static(id, where);
            }
            write_float(v, out + fl->offset, fl->size, id);
            p = end;
            break;
        }
        case FIELD_STRING: {
            if (*p != '"') {
                snprintf(where, sizeof where, "field '%s' wants a quoted string", fl->name);
                die_static(id, where);
            }
            const char *start = p + 1;
            const char *stop = strchr(start, '"');
            if (!stop) {
                snprintf(where, sizeof where, "field '%s': unterminated string", fl->name);
                die_static(id, where);
            }
            int len = (int)(stop - start);
            if (len > fl->array_len - 1) {
                snprintf(where, sizeof where,
                         "field '%s' holds %d characters; %d given",
                         fl->name, fl->array_len - 1, len);
                die_static(id, where);
            }
            memset(out + fl->offset, 0, (size_t)fl->size);
            memcpy(out + fl->offset, start, (size_t)len);
            p = stop + 1;
            break;
        }
        case FIELD_STRUCT:
            p = parse_struct_text(fl->nested, p, out + fl->offset, id);
            break;
        default:
            die_static(id, "a field kind the reader does not know");
        }

        p = skip_ws(p);
        if (f < si->n_fields - 1) {
            if (*p != ',') {
                snprintf(where, sizeof where,
                         "expected ',' after field '%s' — too few values?", fl->name);
                die_static(id, where);
            }
            p = skip_ws(p + 1);
        }
    }

    if (*p == ',')
        die_static(id, "too many values for this struct");
    if (*p != '}')
        die_static(id, "expected '}' to close the struct value");
    return p + 1;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The table itself.                                                  */
/* ------------------------------------------------------------------ */

/* {{{ map_statics_alloc() */
void map_statics_alloc(map_t *m, int n_entries)
{
    if (m->statics)
        die_static(0, "the statics table was already allocated");
    m->statics = calloc((size_t)n_entries, sizeof *m->statics);
    if (!m->statics)
        die_static(0, "out of memory for the statics table");
    m->n_statics = n_entries;
    pthread_mutex_init(&m->statics_mutex, NULL);
}
/* }}} */

/* {{{ map_static_set_text() */
void map_static_set_text(map_t *m, int id, const char *text)
{
    if (id < 0 || id >= m->n_statics)
        die_static(id, "no such entry in the statics table");
    static_entry_t *e = &m->statics[id];
    if (e->text)
        die_static(id, "entry given text twice");
    e->text = strdup(text);
    if (!e->text)
        die_static(id, "out of memory copying entry text");
}
/* }}} */

/* {{{ map_slot_static() */
void map_slot_static(map_t *m, int station, int slot, int static_id)
{
    if (station < 0 || station >= m->n_stations)
        die_static(static_id, "binding a static on a station outside the table");
    station_t *s = &m->stations[station];
    if (slot < 0 || slot >= s->n_slots)
        die_static(static_id, "binding a static on a slot the box does not have");
    slot_t *sl = &s->slots[slot];
    if (sl->kind != SLOT_RING)
        die_static(static_id, "the slot was already converted from a ring buffer");
    if (!sl->type_name)
        die_static(static_id,
                   "the slot has no registry type — statics need stations "
                   "placed by name, so the text knows what shape to become");
    if (static_id < 0 || static_id >= m->n_statics)
        die_static(static_id, "no such entry in the statics table");

    static_entry_t *e = &m->statics[static_id];
    if (!e->text)
        die_static(static_id, "the entry has no text to parse");

    if (e->bytes) {
        /* Someone already parsed this entry. Sharing is allowed when
         * the sizes agree; the docs' broader "each reads the text its
         * own way" is narrowed here — see the first-pass report. */
        if (e->size != sl->elem_size)
            die_static(static_id,
                       "two slots of different sizes reference this entry; "
                       "the first pass supports one parse per entry");
    } else {
        e->bytes = malloc((size_t)sl->elem_size);
        if (!e->bytes)
            die_static(static_id, "out of memory for entry bytes");
        e->size = sl->elem_size;

        const struct_info_t *si = NULL;
        switch (classify_type_name(sl->type_name, &si)) {
        case TN_INT: {
            char *end;
            long long v = strtoll(e->text, &end, 0);
            if (end == e->text)
                die_static(static_id, "an integer slot wants a number");
            write_integer(v, e->bytes, sl->elem_size, static_id);
            break;
        }
        case TN_UINT: {
            char *end;
            unsigned long long v = strtoull(e->text, &end, 0);
            if (end == e->text)
                die_static(static_id, "an unsigned slot wants a number");
            write_unsigned(v, e->bytes, sl->elem_size, static_id);
            break;
        }
        case TN_FLOAT: {
            char *end;
            double v = strtod(e->text, &end);
            if (end == e->text)
                die_static(static_id, "a floating slot wants a number");
            write_float(v, e->bytes, sl->elem_size, static_id);
            break;
        }
        case TN_STRING: {
            /* The claimed value is a pointer; the characters live in
             * the entry for the life of the program, which is what
             * makes handing out the pointer sound. */
            const char *text = e->text;
            const char *start = text;
            int len;
            if (*text == '"') {
                const char *stop = strchr(text + 1, '"');
                if (!stop)
                    die_static(static_id, "unterminated string");
                start = text + 1;
                len = (int)(stop - start);
            } else {
                len = (int)strlen(text);
            }
            e->string_storage = malloc((size_t)len + 1);
            if (!e->string_storage)
                die_static(static_id, "out of memory for string storage");
            memcpy(e->string_storage, start, (size_t)len);
            e->string_storage[len] = 0;
            const char *p = e->string_storage;
            if (sl->elem_size != (int)sizeof p)
                die_static(static_id, "a string slot that is not pointer-sized");
            memcpy(e->bytes, &p, sizeof p);
            break;
        }
        case TN_STRUCT: {
            if (si->size != sl->elem_size)
                die_static(static_id, "struct size disagrees with the slot");
            const char *after =
                parse_struct_text(si, e->text, e->bytes, static_id);
            if (*skip_ws(after) != 0)
                die_static(static_id, "trailing text after the struct value");
            break;
        }
        default:
            die_static(static_id, "the slot's type is not one the reader knows");
        }
    }

    /* The conversion itself: the ring storage goes, the tag flips.
     * From here the readiness walk answers "always full" and claims
     * resolve at task build through static_claim. */
    free(sl->storage);
    sl->storage = NULL;
    sl->capacity = 0;
    sl->head = 0;
    sl->tail = 0;
    sl->kind = SLOT_STATIC;
    sl->static_id = static_id;
}
/* }}} */

/* {{{ map_static_write() */
void map_static_write(map_t *m, int id, const void *bytes, int size)
{
    if (id < 0 || id >= m->n_statics)
        die_static(id, "writing to an entry that does not exist");
    static_entry_t *e = &m->statics[id];
    if (!e->bytes)
        die_static(id, "writing to an entry no slot has bound — its shape is unknown");
    if (size != e->size)
        die_static(id, "writing a value of the wrong size for this entry");

    /* Held for the length of one copy. A struct half-overwritten
     * while a claim is copying it would yield fields from two
     * different worlds; the mutex is the whole defense. */
    pthread_mutex_lock(&m->statics_mutex);
    memcpy(e->bytes, bytes, (size_t)size);
    pthread_mutex_unlock(&m->statics_mutex);
}
/* }}} */

/* {{{ sora_static_write() */
void sora_static_write(int id, const void *bytes, int size)
{
    if (!sora_active_map) {
        fprintf(stderr, "statics: no active map to write into\n");
        abort();
    }
    map_static_write(sora_active_map, id, bytes, size);
}
/* }}} */

/* {{{ static_claim() */
void static_claim(map_t *m, const slot_t *sl, void *into)
{
    static_entry_t *e = &m->statics[sl->static_id];
    pthread_mutex_lock(&m->statics_mutex);
    memcpy(into, e->bytes, (size_t)sl->elem_size);
    pthread_mutex_unlock(&m->statics_mutex);
}
/* }}} */

/* {{{ map_statics_free() */
/* Teardown, called by map_destroy. */
void map_statics_free(map_t *m)
{
    if (!m->statics)
        return;
    for (int i = 0; i < m->n_statics; i++) {
        free(m->statics[i].text);
        free(m->statics[i].bytes);
        free(m->statics[i].string_storage);
    }
    free(m->statics);
    m->statics = NULL;
    pthread_mutex_destroy(&m->statics_mutex);
}
/* }}} */
