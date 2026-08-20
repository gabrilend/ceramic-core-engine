/*
 * 067-genparse.h — what the generator found in the box sources.
 *
 * What this is: the description a parse produces and an emission
 * consumes — every struct, every box, and every author-written
 * comparison, with the file and line each came from so a later
 * complaint can name where the mistake actually is.
 *
 * How it does it, in general terms: the parser does not understand C.
 * It recognizes exactly three top-level shapes in files whose whole
 * purpose is to hold them, and stops loudly on anything else (issue
 * 301). Everything below is what those three shapes turn into.
 *
 * Every string in here lives in the description's arena and is valid
 * until the arena dies, which is when the program ends. Nothing owns
 * anything individually.
 */
#ifndef SORA_GENPARSE_H
#define SORA_GENPARSE_H

#include "065-gentext.h"

/* {{{ tkind_t */
/*
 * What a type fundamentally is, to the engine. This is the same set
 * the statics reader dispatches on, which is why the emitter can map
 * it straight onto the generated field kinds — with one exception:
 * TKIND_STRING_PTR is a borrowed `const char *`, legal as a box
 * parameter and never legal inside a struct, because a field table
 * describes bytes the reader must be able to fill and a pointer is
 * bytes pointing somewhere else.
 */
typedef enum {
    TKIND_UNKNOWN = 0,
    TKIND_INT,
    TKIND_UINT,
    TKIND_FLOAT,
    TKIND_STRING,       /* a fixed char array inside a struct */
    TKIND_STRUCT,
    TKIND_STRING_PTR    /* a borrowed pointer, parameters only */
} tkind_t;
/* }}} */

struct sdef;

/* {{{ field_t */
typedef struct field {
    const char   *type;
    const char   *name;
    int           array_len;   /* 0 when the field is not an array */
    tkind_t       kind;
    struct sdef  *nested;      /* TKIND_STRUCT only */
} field_t;
/* }}} */

/* {{{ sdef_t */
typedef struct sdef {
    const char *name;
    const char *file;
    int         line;
    field_t    *fields;
    int         n_fields;
} sdef_t;
/* }}} */

/* {{{ param_t */
typedef struct param {
    const char *type;
    const char *name;
    tkind_t     kind;
} param_t;
/* }}} */

/* {{{ box_t */
typedef struct box {
    const char *name;
    const char *file;
    int         line;
    const char *ret;        /* "void" for a sink */
    param_t    *params;
    int         n_params;
    tkind_t     ret_kind;
} box_t;
/* }}} */

/* {{{ cmp_t */
/* An author-written `type__compare`, which is never a box. */
typedef struct cmp {
    const char *type;
    const char *file;
    int         line;
} cmp_t;
/* }}} */

/* {{{ description_t */
/*
 * The vectors hold elements by value and are only appended to during
 * parsing, so by the time the emitter walks them the addresses are
 * stable. Nothing hands out a pointer into one before parsing ends,
 * with a single exception the parser handles carefully: a field's
 * `nested` points at an sdef, and it is filled in during validation,
 * after every struct has been pushed.
 */
typedef struct description {
    arena_t *arena;
    vec_t    structs;    /* sdef_t  */
    vec_t    boxes;      /* box_t   */
    vec_t    compares;   /* cmp_t   */
} description_t;
/* }}} */

/* {{{ interface */
void  gp_init(description_t *d);
void  gp_free(description_t *d);

/* Reads one box source and appends what it finds. Any problem ends
 * the program naming file and line: a parser that skips what it does
 * not understand produces a registry with a hole in it, which
 * surfaces much later pointing at the map instead of the real cause. */
void  gp_parse_file(description_t *d, const char *path);

/* Resolves types, links nested structs, and refuses duplicates and
 * anything the engine could not carry. Run once, after every file. */
void  gp_validate(description_t *d);

/* The parser's findings in readable form, for diagnosing a build by
 * looking at what was seen rather than at what was emitted. */
void  gp_describe(const description_t *d);

/* Looks a struct up by name; null when there is none. Valid after
 * parsing. */
sdef_t *gp_struct_named(const description_t *d, const char *name);

/* The author's comparison for a type, or null. */
const cmp_t *gp_compare_for(const description_t *d, const char *type);

/* True when a canonical type name is a primitive the reader knows,
 * and if so what kind it is. The emitter asks this to decide whether
 * to generate a comparison for a box's return type. */
int   gp_primitive_kind(const char *type, tkind_t *out);

/* Ends the program, naming where. Every refusal in the generator goes
 * through here so they all read the same way. */
void  gp_fail(const char *file, int line, const char *fmt, ...);
/* }}} */

#endif
