/*
 * 018-station.h — stations, slots, and the map that holds them.
 *
 * What this is: the persistent half of the engine. A station is one
 * placement of a box in a map — it owns the buffers where values wait,
 * the mutex that guards them, and the list of places its output goes.
 * The map is one flat array of stations addressed by index, never by
 * pointer, so a wire written down today is valid forever.
 *
 * How it does it, in general terms: everything about a station that
 * varies in size hangs off a pointer, so the array stays a row of
 * identical records and a station never moves once placed. Values
 * move by being copied — into a slot's ring, out of it into a task,
 * never shared — which is the entire reason two invocations of one
 * station can run at the same moment without touching.
 *
 * Built across phase 2 (issues 201–207); grows slot kinds in phase 4
 * and routing kinds in phase 5, as marked.
 */
#ifndef SORA_STATION_H
#define SORA_STATION_H

#include <pthread.h>
#include <stdint.h>

#include "011-pool.h"

/*
 * The three slot kinds (issue 202). The tag is stored, never
 * inferred: asking "is my upstream input-less?" on every readiness
 * check would chase an index to answer a question that cannot change
 * while the program runs. Gatherers and statics activate in phase 4;
 * the rows exist from the start so adding them is a row, not a
 * restructure.
 */
enum slot_kind {
    SLOT_RING    = 0,
    SLOT_GATHER  = 1,
    SLOT_STATIC  = 2,
    SLOT_KIND_COUNT
};

/*
 * The three station kinds (issue 201, consulted only in phase 5's
 * routing dispatch). Identical in every respect except which output
 * port a returned value goes down.
 */
enum station_kind {
    STATION_PLAIN      = 0,
    STATION_COMPARATOR = 1,
    STATION_ITERATOR   = 2,
    STATION_KIND_COUNT
};

/* {{{ struct slot */
/*
 * One input slot. Which fields matter depends on the kind:
 * a ring buffer uses storage/capacity/head/tail, a gatherer uses
 * source, a static uses static_id. elem_size matters to all three —
 * cells are exactly the size of the parameter this slot feeds, which
 * is what makes a write a memcpy with no allocation on the hot path.
 */
typedef struct slot {
    unsigned char kind;
    int   elem_size;
    void *storage;
    int   capacity;
    int   head;
    int   tail;
    int   source;     /* gatherer only — upstream station index (phase 4) */
    int   static_id;  /* static only — statics table entry (phase 4) */

    /* The type this slot feeds, as text from the registry — what
     * lets a static entry's text become bytes of the right shape.
     * Null on hand-placed stations, which therefore cannot bind
     * statics; the loader always places by name (phase 4/6). */
    const char *type_name;

    /* The growth story, written by issue 203 and read by phase 7:
     * how many times this buffer has doubled, and the deepest the
     * backlog ever got. A growing slot is one input side outpacing
     * its siblings, with memory absorbing the imbalance. */
    int growths;
    int high_water;
} slot_t;
/* }}} */

/* {{{ struct destination / struct port */
/*
 * A port is one exit from a station; a destination is one place a
 * port delivers. Both numbers of a destination are needed: delivery
 * takes the destination station's mutex and examines all of its
 * slots, so it must name the station, not merely land inside it.
 */
typedef struct destination {
    int32_t station;
    int32_t slot;
    struct destination *next;
} destination_t;

typedef struct port {
    destination_t *destinations;
    struct port   *next;
} port_t;
/* }}} */

/*
 * Three-way comparison over raw bytes of two values of one type —
 * the sign of a minus b. Matches the registry's compare functions;
 * declared here generically so this header stays registry-free.
 */
typedef int (*station_compare_t)(const void *a, const void *b);

/* {{{ struct station */
/*
 * Fixed-size on purpose (issue 201): the array of these must stay
 * indexable, and growing a buffer must never move a station. The
 * kind and cursor were placed in phase 2 and came alive in phase 5,
 * exactly as planned — routing arrived as a change to delivery, not
 * to this shape.
 */
typedef struct station {
    pthread_mutex_t mutex;      /* guards the slots during delivery and readiness */
    task_call_t     call;       /* the shim; hand-written until phase 3 */
    unsigned char   kind;       /* plain, comparator, iterator */
    slot_t         *slots;
    int             n_slots;
    port_t         *ports;      /* linked list; one for plain, three for comparator */
    int             n_ports;
    int             cursor;     /* iterator's next port; the one memory a station keeps */
    int             out_size;   /* bytes of the box's return value; 0 means sink */

    /* Comparator only: the three-way compare for the box's return
     * type, resolved from the registry at placement so the delivery
     * path does a call rather than a lookup (issue 503). */
    station_compare_t compare;
} station_t;
/* }}} */

/* {{{ struct static_entry / struct map */
/*
 * One statics-table entry (issues 401, 402, 405). The text is what
 * the map said; the bytes are its parse, produced when the first
 * slot binds and sized to that slot's type. The first pass departs
 * from the docs here deliberately: entries hold parsed bytes rather
 * than being re-read from text at every claim, because runtime
 * mutation (issue 405) writes bytes, and a table that is sometimes
 * text and sometimes bytes is two tables wearing one name. The
 * "two slots read one entry each their own way" side effect is
 * narrowed to same-size types; the first-pass report carries the
 * reasoning.
 */
typedef struct static_entry {
    char          *text;            /* what the map said; owned here */
    unsigned char *bytes;           /* the parse; what claims copy */
    int            size;            /* bytes' length once parsed */
    char          *string_storage;  /* for string entries: the characters
                                     * the claimed pointer points at */
} static_entry_t;

typedef struct map {
    station_t *stations;
    int        n_stations;
    pool_t    *pool;            /* set by map_start; delivery pushes here */

    /* The statics table: numbered constants, alive for the life of
     * the program, guarded by one mutex the moment writes exist
     * (issue 405). Reads are a memcpy under it; contention is nil. */
    static_entry_t *statics;
    int             n_statics;
    pthread_mutex_t statics_mutex;

    /* The deepest gather chain seen while wiring — the worst-case
     * inline work a worker does assembling one task (issue 404). */
    int gather_depth;
} map_t;
/* }}} */

/* ------------------------------------------------------------------ */
/* Construction calls (issue 207).                                    */
/*                                                                    */
/* SCAFFOLDING, kept deliberately minimal enough to be irritating.    */
/* Phase 6's loader (issue 602) becomes the caller of these; a human  */
/* building maps this way in anger means phase 6 has quietly become   */
/* optional, which is a different and worse project.                  */
/* ------------------------------------------------------------------ */

/* {{{ map_create() — issue 201 */
/* One flat allocation of n identical station records, never resized. */
map_t *map_create(int n_stations);
/* }}} */

/* {{{ map_place() — issues 201, 202, 207 */
/*
 * Place a box at station index: its shim, its kind, one ring-buffer
 * slot per element size given, and the byte size of its return value
 * (zero for a sink). Element sizes are hand-supplied here; from
 * phase 3 they come from the registry, derived from the real C.
 */
void map_place(map_t *m, int station, task_call_t shim, int kind,
               int n_slots, const int *elem_sizes, int out_size);
/* }}} */

/* {{{ map_connect() — issues 201, 205, 207 */
/*
 * Wire: from a station's output port to a destination station's slot.
 * Ports are created on first use, in index order. Repeat with the
 * same port to fan out.
 */
void map_connect(map_t *m, int from_station, int port,
                 int to_station, int to_slot);
/* }}} */

/* {{{ map_start() / map_destroy() — issue 207 */
/*
 * map_start creates the pool with delivery as its finish hook.
 * Values injected before pool_release(m->pool) are the seed; the
 * formal seed sweep arrives in phase 6 (issue 605).
 */
void map_start(map_t *m, int n_workers);
void map_destroy(map_t *m);
/* }}} */

/* ------------------------------------------------------------------ */
/* The push path (issues 202–206).                                    */
/* ------------------------------------------------------------------ */

/* {{{ map_deliver_value() — issues 204, 205 */
/*
 * Deliver one value into one slot of one station: take the mutex,
 * write, run the readiness check, claim if complete, release, then
 * build and push a task if one became due. This is both the interior
 * of the delivery walk and the way a test or a seed drops a value
 * into a map from outside.
 */
void map_deliver_value(map_t *m, int station, int slot, const void *value);
/* }}} */

/* {{{ map_deliver() — issue 205 */
/*
 * The delivery walk: the pool's finish hook. Takes a finished task,
 * chooses the outgoing port by the station's kind, and walks that
 * port's destinations delivering the output value to each.
 */
void map_deliver(void *ctx, task_t *t);
/* }}} */

/* {{{ map_slot_depth() — issue 208 */
/* How many values are waiting in a slot right now. Takes the mutex.
 * Exists for demos and diagnostics, not for engine decisions. */
int map_slot_depth(map_t *m, int station, int slot);
/* }}} */

/* ------------------------------------------------------------------ */
/* The statics table (issues 401, 402, 405). Lives in 033-statics.c.  */
/* ------------------------------------------------------------------ */

/* {{{ map_statics_alloc() / map_static_set_text() */
/* Create the numbered table; give an entry its text. Text must be
 * set before any slot binds the entry. */
void map_statics_alloc(map_t *m, int n_entries);
void map_static_set_text(map_t *m, int id, const char *text);
/* }}} */

/* {{{ map_slot_static() */
/*
 * Convert a slot from ring buffer to static, binding it to an entry.
 * The entry's text is parsed here, into bytes shaped by the slot's
 * registry type — which is why only stations placed by name can bind
 * statics. Always full, never consumed, never affects readiness.
 */
void map_slot_static(map_t *m, int station, int slot, int static_id);
/* }}} */

/* {{{ map_static_write() / sora_static_write() */
/*
 * Alter an entry while the program runs (issue 405), size-checked
 * against the entry. The bare-name variant reaches the active map,
 * and exists so a box can call it — which is a back channel around
 * "a box cannot remember": shared mutable state, relocated, wearing
 * a table for a disguise. It works. Treat it with exactly the
 * suspicion a global variable deserves, and for the same reason.
 */
void map_static_write(map_t *m, int id, const void *bytes, int size);
void sora_static_write(int id, const void *bytes, int size);
/* }}} */

/* ------------------------------------------------------------------ */
/* The pull path (issues 403, 404). Lives in 034-gather.c.            */
/* ------------------------------------------------------------------ */

/* {{{ map_slot_gather() */
/*
 * Convert a slot to a gatherer: its value is produced on demand by
 * running the named upstream station inline at task assembly. The
 * upstream must have no ring-buffer slots (nothing could ever fill
 * them mid-gather), and the connection is refused if it would close
 * a gather cycle — a cycle here is a call that never returns,
 * surfacing as a bare segfault, so it is caught at wiring time when
 * it can still say two station numbers out loud.
 */
void map_slot_gather(map_t *m, int station, int slot, int source_station);
/* }}} */

/* ------------------------------------------------------------------ */
/* Internal joints between the engine's files. Not part of the       */
/* surface a map author touches.                                      */
/* ------------------------------------------------------------------ */

/* {{{ station_port() */
/* The port at an index, or null if never wired — which delivery
 * reads as "discard". */
port_t *station_port(station_t *s, int index);
/* }}} */

/* {{{ static_claim() / gather_claim() — task-build resolution */
/* Called by task construction, outside the station's mutex: a
 * static claim is a locked copy from the table; a gather claim runs
 * the upstream box inline on the assembling thread's own stack. */
void static_claim(map_t *m, const slot_t *sl, void *into);
void gather_claim(map_t *m, const slot_t *sl, void *into);
/* }}} */

/* {{{ map_statics_free() — teardown joint */
void map_statics_free(map_t *m);
/* }}} */

/* {{{ sora_active_map — the one live map, for box-reachable calls */
extern map_t *sora_active_map;
/* }}} */

#endif
