/*
 * 026-registry.h — the joint between the two halves of a program.
 *
 * What this is: the shapes of what the generator emits. The C half of
 * a program is compiled and knows nothing about maps; a map is text
 * and knows nothing about code. The registry is the only place both
 * are described, and it is derived entirely from the C — which is
 * what actually runs, and therefore the only source of truth worth
 * having.
 *
 * How it does it, in general terms: the generator (a build-time Lua
 * script) reads the designated box sources and emits one C file
 * defining the arrays declared here — a shim per box, a record per
 * box carrying its name, types, and sizes, a field table per struct,
 * and a three-way compare per comparable type. Every size and offset
 * in the emitted file is a sizeof or offsetof expression, so the
 * compiler computes them and the generator never guesses.
 *
 * Types and lookups here are hand-written; the data is generated.
 */
#ifndef SORA_REGISTRY_H
#define SORA_REGISTRY_H

#include <stddef.h>
#include <stdio.h>

#include "011-pool.h"
#include "018-station.h"

/*
 * Three-way comparison over the raw bytes of two values of one type,
 * returning the sign of a minus b. Generic in signature so one table
 * can hold every type's; each generated body copies the bytes into
 * real typed variables first, because comparing raw bytes gives
 * wrong answers — a negative float reads as larger than a positive
 * one byte-wise (issue 305).
 */
typedef int (*compare_fn_t)(const void *a, const void *b);

/* What a struct field fundamentally is, for the statics reader. */
enum field_kind {
    FIELD_INT = 0,     /* signed integers of any width */
    FIELD_UINT,        /* unsigned integers of any width */
    FIELD_FLOAT,       /* float or double */
    FIELD_STRING,      /* a char array, fixed length, text inside */
    FIELD_STRUCT,      /* another struct, by its own field table */
};

/* {{{ struct field_info / struct_info */
typedef struct struct_info struct_info_t;

typedef struct field_info {
    const char           *name;
    int                   offset;     /* offsetof, compiler-computed */
    int                   size;       /* sizeof the member */
    unsigned char         kind;
    const struct_info_t  *nested;     /* FIELD_STRUCT only */
    int                   array_len;  /* FIELD_STRING only */
} field_info_t;

struct struct_info {
    const char          *name;
    int                  size;
    int                  n_fields;
    const field_info_t  *fields;
};
/* }}} */

/* {{{ struct box_param / box_info */
typedef struct box_param {
    const char *type_name;
    int         size;
} box_param_t;

typedef struct box_info {
    const char         *name;         /* as it appears in a map */
    task_call_t         shim;         /* the generated call site */
    int                 n_params;
    const box_param_t  *params;       /* in declaration order */
    const char         *return_type;  /* "void" for a sink */
    int                 return_size;  /* 0 for a sink */
    size_t              task_size;    /* exact allocation for one invocation */
    compare_fn_t        compare;      /* for the return type, or null */
} box_info_t;
/* }}} */

/* {{{ struct box_place — issue 311b */
/*
 * One box's **placement function**, and the two names it answers to.
 *
 * The generator emits a function per box that writes a station
 * directly — the shim, the slot sizes, the return size, the type
 * names, the comparison — with every number a `sizeof` the compiler
 * folds into an immediate. A placement function *is* hand placement,
 * written by the generator instead of by a person, which is why there
 * are not two doors into the engine: placing by name is only a way of
 * finding which generated hand-placement to call.
 *
 * **This table is temporary and says so.** Once the generator reads
 * maps itself it emits the calls, and a placement function is reached
 * by being called rather than by being found (issue 311d). Both names
 * are carried meanwhile: the bare function name, which is what map
 * files say today, and the file-and-function address, which is what
 * they will say.
 */
typedef struct box_place {
    const char *name;      /* the bare function name, as maps say today */
    const char *address;   /* file:function, as maps will say */
    void      (*place)(map_t *m, int station, int kind);
} box_place_t;

extern const box_place_t    registry_places[];
extern const int            registry_n_places;

/* Which placement function writes this box's station. Compiled-in
 * rows first, then anything compiled after the program started. */
const box_place_t *box_place_find(const char *name);
/* }}} */

/* The generated data. Defined in src/generated/registry.c, which the
 * generator rewrites on every build where a box source changed. */
extern const box_info_t     registry_boxes[];
extern const int            registry_n_boxes;
extern const struct_info_t  registry_structs[];
extern const int            registry_n_structs;

/* {{{ registry_find() */
/* The lookup a map name goes through. Null when absent — and the
 * caller's error message is the most common one a map author will
 * ever see, so it had better name the name. */
const box_info_t *registry_find(const char *name);
/* }}} */

/* {{{ struct_find() */
const struct_info_t *struct_find(const char *type_name);
/* }}} */

/* {{{ registry_print() */
/* Every box, its parameter types and sizes, its return, its task
 * size — so a build problem is diagnosed by reading what was
 * emitted, not by guessing (issue 303). */
void registry_print(FILE *out);
/* }}} */


/* {{{ map_place_box() */
/*
 * Place a box at a station by name, sizes drawn from the registry
 * instead of typed in by hand — the moment phase 2's hand-supplied
 * element sizes become correct by construction (issue 303). A
 * comparator gets its extra threshold port here, typed to the box's
 * return value (issue 502's shape, placed early so the loader needs
 * no special case).
 */
void map_place_box(map_t *m, int station, const char *box_name, int kind);
/* }}} */

#endif
