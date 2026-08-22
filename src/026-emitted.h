/*
 * 026-emitted.h — the joint between the two halves of a program.
 *
 * What this is: the shapes of what the generator emits. The C half of
 * a program is compiled and knows nothing about maps; a map is text
 * and knows nothing about code. What it emits is the only place both
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
#ifndef SORA_EMITTED_H
#define SORA_EMITTED_H

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

/* {{{ the box record, deleted — issue 311b */
/*
 * **There was a record per box here and it is gone.**
 *
 * It held a name, a shim pointer, a parameter count, an array of
 * parameter type names and sizes, a return type name and size, the
 * exact task allocation, and a comparison function. A station was
 * built by reading it once at placement and copying out the parts a
 * station keeps.
 *
 * The observation that removed it: **the record was read exactly
 * once, at placement, and never again.** Nothing in a running engine
 * consulted it — a station holds its own shim, its own slot sizes,
 * its own return size and its own comparison, and the station header
 * is deliberately free of any reference back. So the record existed
 * only to be read at the one moment generated code could just as well
 * do the writing, which is what a placement function does.
 *
 * Every number in it was a `sizeof` the compiler folded. They did not
 * become guesses by moving: they moved from a table into a function
 * and the compiler folds them into immediates, so they are not stored
 * anywhere at all. Guarantee C1 is untouched.
 *
 * The last three things that still read it went one at a time. The
 * two comparator refusals moved into the placement function, where
 * they were already duplicated and where they name the box by its
 * full address rather than by whatever bare word a map used. The wire
 * refusal stopped fetching a return type's spelling, because a
 * refusal now names both ends by position and size — a name is not
 * what makes a wire legal. And the tests that asked the record what
 * it held now ask a *station* what it got, which is the thing that
 * matters and the thing that is used.
 */
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

extern const box_place_t    box_places[];
extern const int            n_box_places;

/* Which placement function writes this box's station. Compiled-in
 * rows first, then anything compiled after the program started. */
const box_place_t *box_place_find(const char *name);

/*
 * Whether one row is what a name refers to. Three forms, one rule: a
 * bare function name, a basename and a function, or a path and a
 * function (issue 311a). Exposed because the compiled-in rows and the
 * rows that arrived while the program ran are searched separately and
 * must agree about what a name means.
 */
int box_place_matches(const box_place_t *row, const char *name);
/* }}} */

/* The generated data. Defined in src/generated/emitted.c, which the
 * generator rewrites on every build where a box source changed. */
extern const struct_info_t  struct_layouts[];
extern const int            n_struct_layouts;

/* {{{ writing a value down and reading it back — issue 408 */
/*
 * **Each struct gets a reader and a writer of its own, emitted.**
 *
 * There used to be one generalized walk in each direction, stepping a
 * field table and the text together: for every field, read an offset,
 * a size and a kind out of a table, switch on the kind, and do the
 * arithmetic. That is the one place left in the engine where a port
 * pointed at a table and something walked it, and the rest of the
 * engine stopped working that way when placement functions replaced
 * the box record (issue 311b).
 *
 * What replaces it is straight-line code per type, where every field
 * is reached **by name** and its size is a `sizeof` at the point of
 * use. No offsets are stored or computed anywhere: the compiler
 * places the fields and the generated code names them, which is a
 * stronger version of guarantee C1 than a table of offsets, because
 * there is no number to be wrong.
 *
 * A nested struct is read by calling that struct's own reader. So the
 * recursion is in the code rather than in a chain of table pointers.
 *
 * The grammar itself is not emitted per type — the helpers below are
 * shared, so the escape rules and the number widths exist once and
 * cannot drift between one struct's reader and another's.
 */

/*
 * Where a fault was, threaded through so a refusal can name somewhere
 * a person can go and look. A station and a port, because that is an
 * address; the messages used to name an entry number, which named a
 * row in a table rather than anything in the program.
 */
typedef struct sora_where {
    int station;
    int port;
} sora_where_t;

/*
 * A growing piece of text that never overflows and always reports how
 * much it wanted, so a caller that was cut short can tell, and ask
 * again with more room. The same contract snprintf offers.
 */
typedef struct sora_textbuf {
    char *out;
    int   room;
    int   used;
} sora_textbuf_t;

/* Reading. Each returns where it stopped; each refuses fatally,
 * naming the station, the port and the field. */
const char *sora_text_expect(const char *p, char c, const sora_where_t *w,
                             const char *what);
const char *sora_text_signed(const char *p, void *out, int size,
                             const sora_where_t *w, const char *field);
const char *sora_text_unsigned(const char *p, void *out, int size,
                               const sora_where_t *w, const char *field);
const char *sora_text_floating(const char *p, void *out, int size,
                               const sora_where_t *w, const char *field);
/* A char array, filled to `room` bytes and zero-padded. Bounded by
 * the array rather than by a terminator, because a field filled
 * exactly to its width has no room for one. */
const char *sora_text_chars(const char *p, char *out, int room,
                            const sora_where_t *w, const char *field);

/* Writing. What comes out is what would go back in. */
void sora_text_put(sora_textbuf_t *tb, const char *literal);
void sora_text_put_signed(sora_textbuf_t *tb, const void *bytes, int size);
void sora_text_put_unsigned(sora_textbuf_t *tb, const void *bytes, int size);
void sora_text_put_floating(sora_textbuf_t *tb, const void *bytes, int size);
void sora_text_put_chars(sora_textbuf_t *tb, const char *chars, int room);

/*
 * One struct's two directions. A port holding a struct constant is
 * handed this pair at placement, the same way it is handed everything
 * else it needs, so writing a constant down follows a pointer instead
 * of searching anything.
 */
typedef struct struct_text {
    const char *name;
    /*
     * How many bytes the reader will write. Carried so that a port
     * whose width disagrees with the type is refused before the write
     * rather than overrun by it — the one thing the pair cannot check
     * for itself, since it is handed a destination and told nothing
     * about how much room is there.
     */
    int         size;
    const char *(*read)(const char *p, void *out, const sora_where_t *w);
    void        (*write)(const void *bytes, sora_textbuf_t *tb);
} struct_text_t;

extern const struct_text_t struct_texts[];
extern const int           n_struct_texts;

/* One type's pair by name, or NULL. Wanted at placement and by a
 * caller building a port by hand; nothing on the delivery path asks. */
const struct_text_t *struct_text_find(const char *type_name);
/* }}} */

/* {{{ struct_find() */
const struct_info_t *struct_find(const char *type_name);
/* }}} */

/* {{{ emitted_print() */
/* Every box, its parameter types and sizes, its return, its task
 * size — so a build problem is diagnosed by reading what was
 * emitted, not by guessing (issue 303). */
void emitted_print(FILE *out);
/* }}} */


/* {{{ map_place_box() */
/*
 * Place a box at a station by name, sizes drawn from the emitted file
 * instead of typed in by hand — the moment phase 2's hand-supplied
 * element sizes become correct by construction (issue 303). A
 * comparator gets its extra threshold port here, typed to the box's
 * return value (issue 502's shape, placed early so the loader needs
 * no special case).
 */
void map_place_box(map_t *m, int station, const char *box_name, int kind);
/* }}} */

/* {{{ box sources, as text — issue 311c */
/*
 * **The C a program was made from, carried inside it.**
 *
 * The generated file already includes each box source whole so the
 * compiler can see the types and inline each box into its shim. This
 * is the same text emitted a second time as data, so a running
 * program can say what its boxes look like — and so a program handed
 * to somebody else is not a binary that needs a source tree beside it
 * before it can do anything with new code.
 *
 * The text is exactly what was compiled, which is the point of
 * carrying it: a source reported from here can never have changed on
 * disk since, because this copy did not come from disk.
 *
 * The path is shortened against the project root, so two machines
 * building the same tree emit the same file.
 */
typedef struct box_source {
    const char *path;
    const char *text;
} box_source_t;

extern const box_source_t sora_box_sources[];
extern const int          sora_n_box_sources;

/* The text of one box source, by the path the build knew it as, or
 * NULL. A bare basename matches too, because that is how a person
 * refers to a file they can see. */
const char *box_source_text(const char *path);
/* }}} */

/* {{{ maps compiled into code — issue 311d */
/*
 * **A map the build was told about, as the calls it describes.**
 *
 * The text was a thing a program parsed while it ran; it becomes a
 * blueprint for the compilation instead. Every box name in it was
 * resolved on the author's machine and became a direct call to that
 * box's placement function, so **no box name survives into the
 * running program** and a misspelled one fails the build rather than
 * somebody else's startup.
 *
 * Station names do survive, and that is not an inconsistency: a box
 * name was a question the engine had to answer at run time and no
 * longer is, while a station name is data the program carries about
 * itself so it can be written back out as a file that reads in again.
 *
 * The built function works on an empty program and on a crowded one,
 * because it records where each station landed rather than assuming
 * they are numbered from zero.
 */
typedef struct map_build {
    const char *path;
    /*
     * How many stations the description declares. Known when it was
     * compiled, so a caller that needs to record where they land can
     * size its table before building rather than guessing or building
     * twice.
     */
    int         n_stations;
    /*
     * Builds the description into `m` and returns how many stations it
     * declares. When `landed` is not null, where each of those
     * stations went is written into it, in the order the description
     * declared them, up to `cap`.
     *
     * **Where a station lands cannot be recovered by counting.**
     * Adding one hands back a freed place before it grows the table,
     * so a program that has had removals gets whatever holes exist in
     * whatever order. A parent bringing a description inside itself
     * needs this to find the copy's doors (issue 217); a caller
     * building a whole program passes null and ignores the count.
     */
    int       (*build)(map_t *m, int *landed, int cap);
} map_build_t;

extern const map_build_t sora_map_builds[];
extern const int         sora_n_map_builds;

/* The build function for one description, by the path the build knew
 * it as, or by the bare name somebody would type. NULL when this
 * program was not built with that map. */
const map_build_t *map_build_find(const char *path);
/* }}} */

#endif
