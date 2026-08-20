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
 * Every cell is usable. A spare one used to be held back so that
 * head-equals-tail could mean empty rather than full, and with a
 * state on every cell there is nothing left for it to disambiguate —
 * a full buffer is one where no cell answers empty, which is a
 * question that gets asked directly now (issue 210c).
 */
#define SLOT_DEFAULT_CAPACITY 10

/*
 * What is happening to one cell (issue 210c), and who is allowed to
 * touch it while it is happening.
 *
 * | state    | meaning                    | who may touch it     |
 * |----------|----------------------------|----------------------|
 * | empty    | nothing here               | a writer, by taking  |
 * | reserved | a writer is copying in     | that writer only     |
 * | ready    | the bytes have landed      | a reader, by taking  |
 * | claimed  | a reader is copying out    | that reader only     |
 *
 * **This is the mutual exclusion, per cell rather than per port.** A
 * writer must not write while anyone reads or writes; a reader must
 * not read while anyone writes; and the state says so. Every
 * transition is a single compare-and-swap, so two threads can never
 * own one cell — the loser of a race is told it lost and goes
 * elsewhere.
 *
 * A cell's occupancy used to be *implied* by the head and tail
 * indices, and that is why the station's mutex had to cover the copy:
 * the indices said a cell was occupied before its bytes had finished
 * landing, so nothing but exclusion could stop a reader arriving
 * early. A cell that says what is happening to it needs no such help.
 *
 * **Cells are not cleared when released.** Every write is a copy of
 * the port's full element size, so a stale value is always completely
 * covered and there is no such thing as a partial write into a cell.
 * The guarantee is not that a cell was cleaned but that its bytes are
 * never read unless its state says ready, which is this machine's
 * entire job. Zeroing on release would cost a full erase per claim
 * and buy nothing.
 *
 * Empty is zero so that a freshly allocated run of cells is a
 * freshly empty run of cells.
 */
enum cell_state {
    CELL_EMPTY    = 0,
    CELL_RESERVED = 1,
    CELL_READY    = 2,
    CELL_CLAIMED  = 3,
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
 * **Both storages are real** (issues 210b, 401): the cells, and the
 * bytes of a static. Exactly one is in effect and the other sits idle,
 * which is what makes changing what a port is a field write in both
 * directions rather than only one.
 */
typedef struct slot {
    unsigned char kind;
    int   elem_size;
    void *storage;
    int   capacity;
    /* Bytes from one cell to the next: the value's own size, plus its
     * state, rounded up so every value keeps the alignment its type
     * needs (issue 210c). Computed once at allocation, because the
     * rounding is the only arithmetic on the delivery path that is
     * not a single operation. */
    int   stride;

    /* Where to start looking, for a reader and for a writer (issue
     * 210d). These replaced a head and a tail, and the difference is
     * the whole of that issue: **a position must be exact and is
     * therefore computed; a hint may be wrong and therefore is not.**
     *
     * A head index had to be right, because it was what said which
     * cells were occupied — which meant maintaining it under
     * exclusion, which meant the lock. A hint says only "somebody
     * found a cell near here recently". A stale one costs a slightly
     * longer scan and nothing else, so nothing has to be excluded to
     * keep it true, because there is nothing about it that must be.
     *
     * They are ordinals into the port's cells rather than pointers,
     * because storage is the one thing in this design that gets
     * reallocated — a saved pointer means something else afterwards,
     * while an ordinal keeps meaning what it meant. Same reason a
     * wire is a pair of integers rather than an address.
     */
    int   read_hint;
    int   write_hint;

    /* How many cells are ready right now. Maintained rather than
     * counted, because readiness asks this question on every single
     * delivery and a scan to answer it would be the walk this design
     * is trying to get rid of. Atomic because it stops being read
     * under the mutex as the lock comes off the claim path. */
    _Atomic int held;

    /*
     * What a static needs, and it is the value itself now rather than
     * an index into a table everyone shared (issue 401).
     *
     * A station is one instantiation of a box, wired its own way. Its
     * input ports are its own: one may be fed by a wire, another may
     * hold a value that is simply always there. Which of those a port
     * is, and what it holds, is a property of that port on that
     * station and of nothing else — so there is nothing shared, and
     * therefore nothing to share a table for.
     *
     * Three things came out of the table with it. A process may hold
     * more than one running program, because the table was the map
     * state that forced the singleton. Claiming a static happens under
     * the station's own mutex beside the ring pop, so the static half
     * of an input set is as mutually consistent as the buffered half.
     * And two ports of different types can no longer reference one
     * entry and read the same bytes each their own way — that stops
     * being a rule to document and becomes a thing that cannot be
     * said.
     *
     * `constant` is elem_size bytes, allocated at placement like the
     * cells and kept for the life of the map. `constant_string` is
     * where a string static's characters live, since the value for
     * such a port is a pointer and it has to point at something the
     * port owns. `constant_set` is what stops a port being turned
     * into a static that has no value: the tag would be in effect and
     * the storage behind it would be nothing anybody wrote.
     */
    void *constant;
    char *constant_string;
    int   constant_set;

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
} destination_t;

/* {{{ dest_set_t */
/*
 * A port's destinations, as **one immutable array** (issue 214).
 *
 * Nothing ever edits one. Drawing or removing a wire builds a whole
 * new set and swaps the port's pointer in a single atomic write, so a
 * walker reads the pointer once and then walks something nobody will
 * ever modify. That is what takes the station's mutex off the delivery
 * walk: there is no lock, no copy onto the walker's stack, and no way
 * to see a half-edited set.
 *
 * It replaced a linked list whose nodes a rewire could free under a
 * walker's feet — which is why the walk used to copy every pair out
 * under the lock before visiting any of them, on every value the
 * engine moved.
 *
 * An array is also the better shape on its own terms: the destinations
 * are visited in order, immediately, one after another, and a
 * contiguous run of pairs is what a processor wants for that.
 */
typedef struct dest_set {
    struct dest_set *retired_next;  /* the scrapyard's link; see below */
    /*
     * Every worker's epoch at the moment this set was retired, and how
     * many workers there were. Null until it is retired.
     *
     * A worker whose epoch is now **even** is not inside a task, and a
     * worker whose epoch **differs from this snapshot** has finished
     * the task it was in. Either way it cannot still be holding this
     * set. When every worker passes, the set is freed.
     */
    uint64_t        *snapshot;
    int              n_snapshot;
    int              n;
    destination_t    items[];
} dest_set_t;
/* }}} */

typedef struct port {
    /*
     * Read without any lock on the hot path, written only while the
     * rewiring lock is held. Atomic because a reader and a writer
     * genuinely race here, and because the release on the write is
     * what makes the set's contents visible to whoever reads the
     * pointer afterwards.
     *
     * Null means a port wired nowhere, which discards — exactly what
     * an unwired comparator outcome should do.
     */
    _Atomic(dest_set_t *) dests;
    struct port          *next;
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

/* {{{ struct map */
/*
 * **There was a statics table here and it is gone** (issue 401). It
 * held numbered entries, each a piece of text from the map file and
 * the bytes that text parsed into, shared by every port that named
 * the entry and guarded by a mutex of its own.
 *
 * What it cost was out of proportion to what it was: a way to write a
 * value once in a file and point several ports at it. Being map-level
 * mutable state, it was the reason a process could hold only one
 * running program. Its mutex was a second lock a claim had to take,
 * nested inside the station's. And because an entry's bytes were
 * shaped by whichever port bound it first, two ports of different
 * types could name one entry and read the same bytes each their own
 * way — a footgun that had to be documented because it could not be
 * prevented.
 *
 * The `statics` section of a map file is now notation and nothing
 * else: a way to write a value down once while describing the map.
 * Reading it copies the value into each port that names it, and from
 * that moment the entry has done its job. Sharing, when it is wanted,
 * is drawn — one station holds the value and everyone who needs it
 * has an arrow from it, which costs a station and gains a wire
 * somebody can see.
 */
typedef struct map {
    station_t *stations;
    int        n_stations;
    pool_t    *pool;            /* set by map_start; delivery pushes here */

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

    /*
     * The scrapyard (issue 214): destination sets a rewire replaced,
     * kept until nothing can still be walking them.
     *
     * **It owns a lock, and not against tearing.** Nothing ever reads
     * a filed set's contents. The lock is against two hands freeing
     * the same set, and there are two touchers where only one is
     * obvious: rewiring sweeps, and teardown empties. Anything that
     * touches this takes the lock, confirms the set is still filed,
     * unfiles it, and frees it under that same hold — so a second
     * arrival simply does not find it.
     *
     * The lock is a **leaf**: nothing is acquired while it is held.
     * Said as a rule rather than left to be inferred, because a
     * lock-ordering cycle is exactly what somebody builds later
     * having had no way to know.
     *
     * It costs nothing this issue is trying to save. The lock being
     * removed is the one on the delivery walk; this one is touched
     * when wiring changes and when the program ends, never between.
     */
    pthread_mutex_t scrap_mutex;
    dest_set_t     *scrap_head;

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
/* {{{ port_dests() / dest_set_build() / dest_set_retire() — issue 214 */
/*
 * port_dests reads a port's current set. One atomic load, no lock,
 * and the pointer it returns is to something nobody will modify.
 * Null means the port is wired nowhere.
 *
 * dest_set_build makes a new set from an existing one plus or minus
 * one wire; it allocates and never edits what it was given.
 *
 * dest_set_retire files a replaced set in the map's scrapyard. It
 * takes the scrap lock and nothing else.
 */
dest_set_t *port_dests(const port_t *p);
/* Frees every filed set no worker can still be inside. Called by
 * rewiring before it retires another, so a program that rewires
 * forever does not grow forever. */
void        map_scrap_sweep(map_t *m);
/* How many sets are filed right now. For the tests and for a report
 * that wants to say whether the scrapyard is draining. */
int         map_scrap_count(map_t *m);
dest_set_t *dest_set_build(const dest_set_t *from, int add_station,
                           int add_slot, int drop_station, int drop_slot);
void        dest_set_retire(map_t *m, dest_set_t *old);
void        map_scrap_free_all(map_t *m);
/* }}} */

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

/* {{{ map_station_try_start() — issues 401, 605 */
/*
 * Ask a station whether it is ready, and if it is, claim one value
 * from every port, build a task, and push it. Returns whether one
 * became due.
 *
 * This is the interior of a delivery with the delivering taken out,
 * and it exists because two other things needed exactly that and were
 * each doing their own version. The seed sweep enqueued a station
 * without asking whether it was ready at all, which was safe only
 * while an unasked question happened to have the same answer. Writing
 * a static is supposed to run the ordinary readiness check on its
 * station — that is what replaced the pull path, and it is what makes
 * a chain of stations wired through static ports into a recalculation
 * graph — and it was not running one.
 *
 * A write cannot make something run that could not run anyway,
 * because the check it triggers is the ordinary one: an empty ring
 * port still answers no, and the engine will not invent a value for
 * it.
 */
int map_station_try_start(map_t *m, int station);
/* }}} */

/* ------------------------------------------------------------------ */
/* Statics, which live on the ports that read them (issues 401, 402,  */
/* 405). In 033-statics.c, which is now a reader and a writer of      */
/* values rather than the keeper of a table.                          */
/* ------------------------------------------------------------------ */

/* {{{ map_slot_static_text() */
/*
 * Give a port a constant, written as text, and make it a static.
 *
 * The text is parsed here into the port's own storage, shaped by the
 * port's registry type — which is why only stations placed by name
 * can hold statics: turning `{ 5, 2.0, { 0, 0, 0 }, "hey there", 2 }`
 * into bytes means knowing the field layout, and the type is where
 * that comes from.
 *
 * Nothing is retained afterwards. A map file's `statics` section is
 * notation: a way to write a value down once and point ports at it by
 * number while the file is being read. Two ports given the same
 * entry's text end up with two independent values, and writing one
 * does not disturb the other.
 *
 * A static is always full, never consumed, and never affects
 * readiness — but setting one runs the readiness check on its
 * station, because a port that was the last one missing is no longer
 * missing.
 */
void map_slot_static_text(map_t *m, int station, int slot, const char *text);
/* }}} */

/* {{{ map_slot_static_write() — issue 405 */
/*
 * Change a static while the program runs. It names a station and a
 * port, because that is where the value lives, and it is size-checked
 * against what that port holds.
 *
 * It takes the station's own mutex — the one the claim already takes
 * — so no claim can see a half-written value. That matters for
 * anything wider than a machine word: a struct half-overwritten while
 * a claim copies it yields fields that were never simultaneously
 * true, which is not theoretical and was demonstrated.
 *
 * Like setting one, writing one runs the readiness check on the
 * station. Writing does not *consume* anything, so a station that was
 * already able to run runs again — which is what makes a chain of
 * stations wired through static ports recalculate.
 *
 * **A box may no longer write a static, and that is the point of the
 * removal.** There used to be a bare-name variant taking an entry
 * number and reaching a process-wide "active map" pointer, because a
 * box receives only values and has no handle to anything. It worked,
 * and it was always described as deserving the suspicion a global
 * variable deserves — a box could stash a value and read it back on
 * its next run, with none of it visible in the wiring, so a map
 * showing no connection between two stations might still have them
 * talking.
 *
 * What was not obvious until it was traced is what it cost: reaching
 * a map from inside a box requires a process-wide map pointer, and a
 * process-wide map pointer means a process can only ever run one map.
 * A feature the design already distrusted was quietly charging the
 * whole engine its ability to compose.
 *
 * A box that needs to affect something later in the run does it the
 * way everything else does: it returns a value, and the value is
 * wired somewhere. Writers are otherwise all outside the graph — a
 * debugger, a control socket, a person turning a knob, or a parent
 * program configuring a child.
 */
void map_slot_static_write(map_t *m, int station, int slot,
                           const void *bytes, int size);
/* }}} */

/* ------------------------------------------------------------------ */
/* Internal joints between the engine's files. Not part of the       */
/* surface a map author touches.                                      */
/* ------------------------------------------------------------------ */

/* {{{ slot_cell() / slot_cell_move() — issue 210c */
/*
 * One cell, and the one way its state ever changes.
 *
 * slot_cell returns where cell `index`'s value bytes live. The value
 * comes first in a cell and its state sits after it, so that the
 * value keeps the alignment the allocator gave the array — a state
 * byte in front would push every value off by one, which on some
 * machines is a fault and on the rest is slow.
 *
 * slot_cell_move is the whole state machine: a compare-and-swap from
 * one named state to another, returning whether this caller won it.
 * There is one primitive rather than four named transitions because
 * the rule worth enforcing is *this exact state became that exact
 * state*, and naming the pair at the call site is what makes a
 * reader of the delivery path able to see the machine running. A
 * transition from a state a cell is not in simply fails, which is
 * what makes an illegal move impossible rather than merely
 * discouraged.
 *
 * Two callers race for one cell and exactly one of them wins. The
 * loser is not blocked and does not retry in place — it goes and
 * looks at another cell, which is the property the whole design is
 * for.
 */
void *slot_cell(const slot_t *sl, int index);
int   slot_cell_move(const slot_t *sl, int index, int from, int to);
/* }}} */

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

/* {{{ slot_constant_free() — teardown joint */
/* A port's constant and, for a string, the characters it points at.
 * Owned by the port and freed with the map. */
void slot_constant_free(slot_t *sl);
/* }}} */

/* {{{ slot_constant_text() — issue 401 */
/*
 * A port's constant, turned back into the text a map file would use.
 * Writes at most `room` bytes including the terminator, and returns
 * how many characters it wanted — so a caller can tell it was cut
 * short.
 *
 * This is the exact mirror of the reader that walks a field table
 * turning text into bytes, and it exists because the dump lost its
 * source of words. The statics table used to keep the original string
 * a file gave it, and the dump wrote that string back out; with the
 * value living on the port and no text retained anywhere, there is
 * nothing to echo and the bytes have to be spoken.
 *
 * It is one piece of work with more than one caller in waiting: the
 * dump, anything showing a value to a person, and eventually a
 * program's results — which are text for the same reason a static is,
 * because text resolves its layout when it is read and so survives a
 * rebuild that would silently change what raw bytes meant.
 */
int slot_constant_text(const slot_t *sl, char *out, int room);
/* }}} */

/* {{{ task_build() — the one way a task comes into existence */
/*
 * Exposed so the seed sweep (issue 605) creates its first tasks
 * through the same path delivery uses — one way, not two. The claimed
 * buffer carries one value per port, of every kind: statics are
 * claimed under the station's mutex beside the ring pops now (issue
 * 401), so nothing is left to resolve here.
 */
task_t *task_build(map_t *m, int station_index,
                   const unsigned char *claimed, int port);
/* }}} */

#endif
