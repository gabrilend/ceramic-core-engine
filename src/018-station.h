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
#include <stdatomic.h>
#include <stdint.h>

#include "011-pool.h"

/*
 * The slot kinds (issue 202). The tag is stored, never inferred:
 * asking "is my upstream input-less?" on every readiness check would
 * chase an index to answer a question that cannot change while the
 * program runs.
 *
 * There were three. The gatherer — a slot whose value was produced by
 * running its upstream box inline at the moment a task was assembled
 * — is gone, and the reasoning is kept in
 * docs/implementation-notes/056-no-pull-path.md rather than repeated
 * here. What it bought was a value fresh at the moment of use; what
 * it cost was user code running on a thread that was in the middle of
 * assembling someone else's task, and a queue depth nothing kept in
 * step with the station's other ports. Writing a static now runs the
 * ordinary readiness check on its station, which is what replaced it
 * (issue 210).
 *
 * The numbering closed up rather than leaving a hole where the
 * gatherer was, because nothing outside this file ever saw these as
 * numbers — the map file spells a slot's kind as text, and the dump
 * writes text back.
 *
 * SLOT_NONE is not a third kind of value; it is the absence of a
 * decision (issue 210b). A port in it has been given no source, and a
 * station holding one can never be ready — which is what lets a
 * program be assembled from nothing, a station coming into existence
 * with every port unset and becoming runnable as its ports are given
 * sources one at a time. No null is invented and nothing is ever
 * handed to a box; the readiness walk simply answers no forever.
 */
enum slot_kind {
    SLOT_RING    = 0,
    SLOT_STATIC  = 1,
    SLOT_NONE    = 2,
    SLOT_KIND_COUNT
};

/*
 * Every port's ring buffer starts this deep, in cells of that port's
 * own element size — so a port carrying four-byte integers starts at
 * forty bytes and one carrying a two-hundred-byte struct at two
 * thousand (issue 210b).
 *
 * Ten is a magic number and is meant to be one. It barely matters: a
 * buffer that starts too small grows to whatever depth the program
 * actually demands and then stops, so the cost of guessing low is a
 * slower startup, which is the cheapest time in a program's life to be
 * slow. A port that knows better can say so through
 * map_slot_start_depth.
 *
 * One cell is left spare so head-equals-tail can mean empty, which is
 * why the usable depth is one less than this. That spare disappears
 * with the head-and-tail scheme itself in issue 210c, where a cell's
 * own state says whether it is occupied.
 */
#define SLOT_DEFAULT_CAPACITY 10

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
 * One input slot. Which fields are *in effect* depends on the kind: a
 * ring buffer reads storage/capacity/head/tail, a static reads
 * static_id, and an unconfigured port reads neither. elem_size
 * matters to all three — cells are exactly the size of the parameter
 * this slot feeds, which is what makes a write a memcpy with no
 * allocation on the hot path.
 *
 * The `source` field went with the gatherer (issue 210): it held the
 * upstream station a slot pulled from, and nothing pulls now.
 *
 * **The cells are allocated at instantiation and are never freed
 * until the map is** (issue 210b), whatever the tag currently says.
 * A port that is a static for the whole life of a program carries
 * cells it never uses, and that is the price: it is paid once, at
 * startup, in the cheapest moment a program has. What it buys is that
 * changing a port's source is a field write rather than an allocation
 * dance — there is never a moment when the storage a tag needs is
 * absent — and that values already waiting in a port survive it being
 * turned into something else and back (issue 210f).
 *
 * The storage a *static* needs is still the table entry number rather
 * than the bytes themselves. Moving the bytes onto the port here is
 * blocked on issue 401, which deletes the table they currently live
 * in; until then this record has room for one storage and an index to
 * the other, which is the honest shape of a half-finished move.
 */
typedef struct slot {
    unsigned char kind;
    int   elem_size;
    void *storage;
    int   capacity;
    int   head;
    int   tail;
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

    /* Phase 7's counters (issue 702). The counts are atomics updated
     * where the work already is, costing nearly nothing, and are
     * always on. The times are only ever written when SORA_STATS is
     * compiled in — the fields stay so the struct never changes
     * shape, but every clock read compiles out. */
    _Atomic long runs;           /* tasks of this station completed */
    _Atomic long produced;       /* tasks its outputs made due elsewhere */
    _Atomic long box_ns;         /* time inside the box function */
    _Atomic long mutex_wait_ns;  /* time deliverers waited on the mutex */
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

    /* How many stations the seed sweep enqueued (issue 605). Zero
     * on hand-built maps that seed by delivering. */
    int seeded;

    /* Station names, retained from the map file (null on hand-built
     * maps). The engine itself never reads them — every wire is an
     * index — but the dump (issue 703) must write a file that reads
     * back, and a person watching a live view deserves names. The
     * loader's throwaway lookup table and this are different things:
     * that one resolved arrows and died; this one is for speaking. */
    char **station_names;

    /* The rewiring lock (issue 704): edge validation and list
     * mutation are one operation under it, never two. */
    pthread_mutex_t rewire_mutex;

    /* The observer (issue 701): a small reporting thread, not a
     * worker, pushing nothing. */
    pthread_t observer;
    int       observer_running;
    char     *observer_path;
    int       observer_interval_ms;
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

/* {{{ map_slot_start_depth() — issue 210b */
/*
 * Tell one port how deep its ring buffer should start, in cells.
 *
 * It is a hint rather than a setting: growth covers being wrong, so
 * nobody has to be right. A port never told anything starts at
 * SLOT_DEFAULT_CAPACITY, and a program that guesses low pays a slower
 * startup and nothing else. What this exists for is the case where an
 * author already knows one input side outruns its siblings — the
 * situation phase 7's buffer report shouts about — and would rather
 * not watch it grow thirteen times to find out.
 *
 * Refuses a port that already holds values, because a *starting*
 * depth set after the start is a different and much harder operation:
 * it would have to move values that other threads may be reading.
 *
 * Issue 210b asked for this as an argument on the call that creates
 * the station. It landed as its own call instead, for two reasons.
 * The array of capacities would have been null at roughly eighty
 * existing call sites across the tests and the phase demos, which is
 * a lot of churn to say "no opinion"; and issue 210g is about to
 * collapse placement and port configuration into one surface, where
 * this is a case of configuring a port rather than a parameter of
 * building a station. Naming a station, a port, and what that port
 * should be is the shape that surface already has.
 */
void map_slot_start_depth(map_t *m, int station, int slot, int cells);
/* }}} */

/* {{{ map_slot_convert() — issues 210b, 210f */
/*
 * Change what one port is: name the station, the port, and the tag it
 * is becoming.
 *
 * **Conversion is a field write.** Storage does not move. The cells
 * stay exactly as they are on every path through this — not freed,
 * not cleared, not drained — so whatever a producer had already
 * handed over and nobody had claimed is still waiting if the port
 * becomes a ring buffer again. Discarding it would throw away values
 * a producer already handed over, invisibly, which is worse than
 * serving them slightly late.
 *
 * That "slightly late" is the honest cost: a port turned into
 * something else and back may deliver a value that arrived before the
 * conversion after values that arrived during it. Arrival order is
 * not promised (issue 210d), so this costs nothing that was still
 * being offered — but it is a second reason for the same
 * non-guarantee, and rollback is not the only thing that opens gaps.
 *
 * Becoming a static is refused here and goes through map_slot_static,
 * which needs an entry number this call has no room for. That is the
 * half of issue 210f blocked on issue 401 — once a static's value
 * lives on the port rather than in a table, what a port needs to
 * become one is a *value*, and this call grows a way to carry it.
 *
 * A port that has been a static and is converted away keeps its
 * binding, so a port that goes static, ring, static reads the same
 * entry it read before. That is the same rule as the cells: nothing
 * on any path through here is destroyed. It is also what keeps
 * SLOT_NONE meaning one thing — "nobody has said yet" — rather than
 * also meaning "somebody said, then said something else."
 */
void map_slot_convert(map_t *m, int station, int slot, int kind);
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
 * into a map from outside. Returns whether a task became due, which
 * the statistics read as "the deliverer produced one".
 */
int map_deliver_value(map_t *m, int station, int slot, const void *value);
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
/* Internal joints between the engine's files. Not part of the       */
/* surface a map author touches.                                      */
/* ------------------------------------------------------------------ */

/* {{{ slot_kind_name() — issue 210b */
/*
 * What a port's tag is called, in the words a person would use. Every
 * refusal that turns somebody away from a port has to say which of the
 * three it found, because "not a buffer" describes two different
 * situations with two different fixes: a static already holds a value
 * and has no room to queue another, while an unconfigured port is one
 * nobody has finished wiring. One table, so a fourth tag would be a
 * row rather than three edits nobody finds.
 */
const char *slot_kind_name(unsigned char kind);
/* }}} */

/* {{{ station_port() */
/* The port at an index, or null if never wired — which delivery
 * reads as "discard". */
port_t *station_port(station_t *s, int index);
/* }}} */

/* {{{ static_claim() — task-build resolution */
/* Called by task construction, outside the station's mutex: a locked
 * copy from the statics table, so that table's lock never nests
 * inside a station's. It is the only kind still resolved here now
 * that nothing is gathered; ring values were already claimed under
 * the mutex before the task was built. */
void static_claim(map_t *m, const slot_t *sl, void *into);
/* }}} */

/* {{{ map_statics_free() — teardown joint */
void map_statics_free(map_t *m);
/* }}} */

/* {{{ task_build() — the one way a task comes into existence */
/*
 * Exposed so the seed sweep (issue 605) creates its first tasks
 * through the same path delivery uses — one way, not two. The
 * claimed buffer feeds ring slots only and may be null for a
 * station that has none, which is the only kind the seed touches.
 */
task_t *task_build(map_t *m, int station_index,
                   const unsigned char *claimed, int port);
/* }}} */

/* {{{ sora_active_map — the one live map, for box-reachable calls */
extern map_t *sora_active_map;
/* }}} */

#endif
