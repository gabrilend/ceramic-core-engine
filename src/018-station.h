/*
 * 018-station.h — stations, slots, and the map that holds them.
 *
 * What this is: the persistent half of the engine. A station is one
 * placement of a box in a map — it owns the buffers where values wait,
 * the mutex that guards them, and the list of places its output goes.
 * The map is a table of stations addressed by index, never by
 * pointer, so a wire written down today is valid forever.
 *
 * How it does it, in general terms: everything about a station that
 * varies in size hangs off a pointer, so the array stays a row of
 * identical records and a station never moves once placed. Values
 * move by being copied — into a port's ring, out of it into a task,
 * never shared — which is the entire reason two invocations of one
 * station can run at the same moment without touching.
 *
 * Built across phase 2 (issues 201–207); grows port kinds in phase 4
 * and routing kinds in phase 5, as marked.
 */
#ifndef SORA_STATION_H
#define SORA_STATION_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

#include "011-pool.h"

/*
 * The port kinds (issue 202). The tag is stored, never inferred:
 * asking "is my upstream input-less?" on every readiness check would
 * chase an index to answer a question that cannot change while the
 * program runs.
 *
 * There were three. The gatherer — a port whose value was produced by
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
 * numbers — the map file spells a port's kind as text, and the dump
 * writes text back.
 *
 * IN_PORT_NONE is not a third kind of value; it is the absence of a
 * decision (issue 210b). A port in it has been given no source, and a
 * station holding one can never be ready — which is what lets a
 * program be assembled from nothing, a station coming into existence
 * with every port unset and becoming runnable as its ports are given
 * sources one at a time. No null is invented and nothing is ever
 * handed to a box; the readiness walk simply answers no forever.
 */
enum in_port_kind {
    IN_PORT_RING    = 0,
    IN_PORT_STATIC  = 1,
    IN_PORT_NONE    = 2,
    IN_PORT_KIND_COUNT
};

/*
 * Every port's ring buffer starts this deep, in slots of that port's
 * own element size — so a port carrying four-byte integers starts at
 * forty bytes and one carrying a two-hundred-byte struct at two
 * thousand (issue 210b).
 *
 * Ten is a magic number and is meant to be one. It barely matters: a
 * buffer that starts too small grows to whatever depth the program
 * actually demands and then stops, so the cost of guessing low is a
 * slower startup, which is the cheapest time in a program's life to be
 * slow. A port that knows better can say so through
 * map_in_port_start_depth.
 *
 * Every slot is usable. A spare one used to be held back so that
 * head-equals-tail could mean empty rather than full, and with a
 * state on every slot there is nothing left for it to disambiguate —
 * a full buffer is one where no slot answers empty, which is a
 * question that gets asked directly now (issue 210c).
 */
#define IN_PORT_DEFAULT_CAPACITY 10

/*
 * What is happening to one slot (issue 210c), and who is allowed to
 * touch it while it is happening.
 *
 * | state    | meaning                    | who may touch it     |
 * |----------|----------------------------|----------------------|
 * | empty    | nothing here               | a writer, by taking  |
 * | reserved | a writer is copying in     | that writer only     |
 * | ready    | the bytes have landed      | a reader, by taking  |
 * | claimed  | a reader is copying out    | that reader only     |
 *
 * **This is the mutual exclusion, per slot rather than per port.** A
 * writer must not write while anyone reads or writes; a reader must
 * not read while anyone writes; and the state says so. Two threads can
 * never own one slot.
 *
 * **Only one transition is a compare-and-swap**, and knowing which is
 * worth more than assuming all of them are (issue 210d). Empty →
 * reserved is where two writers genuinely race for the same slot, so
 * the loser has to be told it lost. The other three have exactly one
 * possible mover: reserved → ready and claimed → empty are done by
 * the worker that owns the slot, and ready → claimed is done under
 * the station's mutex, which excludes the only other thing that could
 * want it. Those are an ordinary load and an ordinary store, with
 * acquire and release ordering so the bytes travel with the state.
 *
 * That distinction is not a micro-optimisation. The claim's *search*
 * asks this question of every candidate it walks past, so a
 * read-modify-write per candidate was measurable where a load is not.
 *
 * A slot's occupancy used to be *implied* by the head and tail
 * indices, and that is why the station's mutex had to cover the copy:
 * the indices said a slot was occupied before its bytes had finished
 * landing, so nothing but exclusion could stop a reader arriving
 * early. A slot that says what is happening to it needs no such help.
 *
 * **Slots are not cleared when released.** Every write is a copy of
 * the port's full element size, so a stale value is always completely
 * covered and there is no such thing as a partial write into a slot.
 * The guarantee is not that a slot was cleaned but that its bytes are
 * never read unless its state says ready, which is this machine's
 * entire job. Zeroing on release would cost a full erase per claim
 * and buy nothing.
 *
 * Empty is zero so that a freshly allocated run of slots is a
 * freshly empty run of slots.
 */
enum slot_state {
    SLOT_EMPTY    = 0,
    SLOT_RESERVED = 1,
    SLOT_READY    = 2,
    SLOT_CLAIMED  = 3,
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

/* {{{ struct in_port_page / struct in_port */
/*
 * One input port. Which fields are *in effect* depends on the kind: a
 * ring buffer reads the pages, the page size, the capacity and the two
 * hints; a static reads its constant; an unconfigured port reads
 * neither. elem_size matters to all three — slots are exactly the size
 * of the parameter this port feeds, which is what makes a write a
 * memcpy with no allocation on the hot path.
 *
 * The `source` field went with the gatherer (issue 210): it held the
 * upstream station a port pulled from, and nothing pulls now.
 *
 * **The slots are allocated at instantiation and are never freed
 * until the map is** (issue 210b), whatever the tag currently says.
 * A port that is a static for the whole life of a program carries
 * slots it never uses, and that is the price: it is paid once, at
 * startup, in the cheapest moment a program has. What it buys is that
 * changing a port's source is a field write rather than an allocation
 * dance — there is never a moment when the storage a tag needs is
 * absent — and that values already waiting in a port survive it being
 * turned into something else and back (issue 210f).
 *
 * **Both storages are real** (issues 210b, 401): the slots, and the
 * bytes of a static. Exactly one is in effect and the other sits idle,
 * which is what makes changing what a port is a field write in both
 * directions rather than only one.
 */
/*
 * One page of a port's ring buffer (issue 210e).
 *
 * A buffer grows by adding one of these to the end of a short list,
 * never by copying, so **no slot that already exists ever moves**.
 * That is not a tidiness argument. A worker copying bytes out of a
 * slot it has claimed holds no lock — a claimed slot belongs to it
 * alone and needs no exclusion from anybody — and relocating that
 * slot underneath it is precisely the thing that ownership does not
 * protect against. Copy-and-unwrap growth was safe only while the
 * station's mutex covered the whole copy, and it stops being safe the
 * moment the value copies leave that lock (issue 210d).
 *
 * Every page holds the same number of slots, so turning a slot's
 * ordinal into a page and an offset is a divide and a remainder.
 * Pages that each doubled the last would have made that a walk down
 * the list comparing ranges; the scan does this on every step and
 * growth happens rarely, so the cheap operation belongs on the side
 * that repeats.
 */
typedef struct in_port_page {
    /*
     * Atomic because a delivering writer walks this list holding no
     * lock while a grower may be appending to it (issue 210d). Growth
     * publishes a page by storing it here with release, and bumps the
     * capacity only afterwards — so a scanner that sees the larger
     * capacity is guaranteed to find the page, and one that sees the
     * old capacity simply does not use the new slots yet. Neither is
     * wrong; the second is merely a sweep too early.
     */
    _Atomic(struct in_port_page *) next;
    /* page_slots × stride bytes: value, state, padding, repeating. */
    unsigned char                 slots[];
} in_port_page_t;

typedef struct in_port {
    /*
     * Atomic because a delivery reads it holding no lock while a
     * conversion may be writing it (issues 210d, 210f). Conversion
     * takes the station's mutex, so it does not race the readiness
     * walk or the claim — but the two guard checks at the top of a
     * delivery run before any lock is taken, and a plain byte read
     * against a plain byte write is a data race whatever the values
     * involved.
     *
     * The outcome of losing that race is benign, which is why this is
     * the only thing needed: a value written into a port that has just
     * become a static lands in slots that exist regardless of the tag,
     * and waits there until the port is a buffer again. That is the
     * same promise conversion already makes about values it finds.
     */
    _Atomic unsigned char kind;
    int   elem_size;
    /* The pages, oldest first. The first is allocated when the
     * station is placed; growth appends. Never reordered, never
     * freed until the map is. */
    in_port_page_t *pages;
    /* Slots per page — the same for every page of this port, and the
     * same number the first page was given, so a program that wants
     * deep buffers raises its starting depth and gets large pages
     * everywhere rather than a long chain of small ones. */
    int   page_slots;
    /* Total slots across every page. A sum rather than a single
     * allocation's size, which is what the buffer report speaks.
     *
     * Atomic, and written **after** the page it counts is linked, so
     * that seeing it is proof the slots exist. A scan snapshots it
     * once rather than re-reading it, which is what bounds the sweep:
     * a reader that kept re-reading a number another thread keeps
     * raising could be made to walk forever. */
    _Atomic int capacity;
    /* Bytes from one slot to the next: the value's own size, plus its
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
     * slots were occupied — which meant maintaining it under
     * exclusion, which meant the lock. A hint says only "somebody
     * found a slot near here recently". A stale one costs a slightly
     * longer scan and nothing else, so nothing has to be excluded to
     * keep it true, because there is nothing about it that must be.
     *
     * They are ordinals into the port's slots rather than pointers.
     * Under paging a pointer into a page would in fact stay valid,
     * since pages never move (issue 210e) — but an ordinal survives
     * being read while another thread appends a page, and it is the
     * same shape the scan already needs to bound itself by. Same
     * reason a wire is a pair of integers rather than an address.
     */
    int   read_hint;
    int   write_hint;

    /* How many slots are ready right now. Maintained rather than
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
     * slots and kept for the life of the map. `constant_string` is
     * where a string static's characters live, since the value for
     * such a port is a pointer and it has to point at something the
     * port owns. `constant_set` is what stops a port being turned
     * into a static that has no value: the tag would be in effect and
     * the storage behind it would be nothing anybody wrote.
     */
    void *constant;
    char *constant_string;
    int   constant_set;

    /*
     * The type this port feeds, as text — what lets a static's text
     * become bytes of the right *shape*. Null on hand-placed stations,
     * which therefore cannot bind statics.
     *
     * **This is not a precedent for carrying type names, and the
     * difference is worth stating because it is easy to get wrong.**
     * A wire is checked by width and never by name (issue 309): two
     * boxes may spell one shape differently and mean the same data, so
     * comparing names would refuse a sound connection. Nothing here is
     * ever compared against anything. It is used to *find a layout* —
     * which field sits at which offset — because turning
     * `{ 5, 2.0, "hey" }` into bytes needs more than a byte count.
     *
     * The *searching* half of that is gone already: the field table
     * below is handed over at placement. What is left here is the
     * spelling, which messages and the dump still read, and which goes
     * when the binary carries its own box sources (issue 311c).
     */
    const char *type_name;

    /*
     * Which fields this port's type has, and where each one sits —
     * the address of a generated table, written by the placement
     * function because it knows the type concretely (issue 311b).
     *
     * Null unless the type is a struct, and null on a hand-placed
     * station, which is why the reader still checks before following
     * it. It replaced a search of every emitted struct table for one
     * whose name matched, which was a lookup performed to answer a
     * question the placement already knew.
     *
     * Declared as an incomplete type because the field table belongs
     * to the build path and this header must not depend on it — the
     * dependency runs the other way.
     */
    const struct struct_info *fields;

    /* The growth story, written by issue 203 and read by phase 7:
     * how many times this buffer has doubled, and the deepest the
     * backlog ever got. A growing port is one input side outpacing
     * its siblings, with memory absorbing the imbalance. */
    int growths;
    int high_water;
} in_port_t;
/* }}} */

/* {{{ struct destination / struct out_port */
/*
 * A port is one exit from a station; a destination is one place a
 * port delivers. Both numbers of a destination are needed: delivery
 * takes the destination station's mutex and examines all of its
 * slots, so it must name the station, not merely land inside it.
 */
typedef struct destination {
    int32_t station;
    int32_t port;
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
    int           n;
    destination_t items[];
} dest_set_t;
/* }}} */

typedef struct out_port {
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
    struct out_port      *next;
} out_port_t;
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
    pthread_mutex_t mutex;      /* held across delivery and readiness */
    task_call_t     call;       /* the shim; hand-written until phase 3 */
    unsigned char   kind;       /* plain, comparator, iterator */
    in_port_t      *in_ports;
    int             n_in_ports;
    out_port_t     *out_ports;  /* list; one plain, three comparator */
    int             n_out_ports;
    int             cursor;     /* iterator's next port; the one memory a station keeps */

    /*
     * Set when this station has been removed and not yet reclaimed
     * (issue 216).
     *
     * **Its fields stay readable until the scrapyard frees them**,
     * and that is the whole trick. A task is built from a station's
     * port count, return size, and shim *after* the readiness check
     * released the mutex — so clearing those at the moment of removal
     * would leave a worker building a task out of a station that had
     * just been emptied underneath it. Instead the record stays
     * intact and this flag says not to start anything new from it.
     * The fields go when nobody can still be inside a task that
     * needs them, which is the same question the scrapyard already
     * answers.
     *
     * So a removed place is not immediately a free place: it becomes
     * one when the sweep clears the shim. That is correct rather than
     * inconvenient — you cannot reuse something while somebody might
     * still be using it.
     */
    _Atomic unsigned char removed;
    int             out_size;   /* bytes of the box's return value; 0 means sink */

    /*
     * The name this station was placed as, written by the generated
     * placement function as a literal (issue 311b).
     *
     * **This is not a lookup and never was worth being one.** The dump
     * used to find a station's box by scanning every box record for
     * one whose call site matched — the registry read backwards, which
     * is a linear search to answer a question the station could simply
     * have been told the answer to. The literal costs one pointer per
     * station, the string is read-only data the compiler was going to
     * emit anyway, nothing is allocated and nothing is freed.
     *
     * **Null for a station placed by hand with no name given**, which
     * is honest rather than awkward: nothing on disk describes such a
     * program either, so there is nothing for a station line to say.
     * The dump says so plainly instead of inventing something.
     *
     * It carries the bare function name today because that is what a
     * map file says. When the format learns to carry a box's file as
     * well (issue 311a), this literal becomes the full address and the
     * dump follows without changing.
     */
    const char     *box_name;

    /*
     * Whether this station has already been set going by a
     * bring-up (issue 212).
     *
     * The pass that starts a program is **repeatable**: a station
     * added to a running program is checked and started by the next
     * call, and one that was started before is not started twice.
     * Without the mark, bringing a grown program up again would give
     * every no-input station a second run for no reason anybody asked
     * for — which is the sort of thing that looks like a scheduling
     * bug for a week.
     */
    unsigned char   seeded;


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
/*
 * How many station records sit on one shelf (issue 211). A power of
 * two, so turning a station number into a shelf and a position within
 * it is one shift and one mask rather than a division.
 *
 * The number does not have to be guessed well, and that is the point.
 * Too small and the short array of shelf pointers grows a little more
 * often — and that array holds addresses, so growing it is safe and
 * fast. Too large and the last shelf holds some records nobody uses, a
 * few kilobytes at worst. Nothing is copied either way and no station
 * ever moves either way.
 */
#define STATIONS_PER_SHELF 64
#define STATION_SHELF_SHIFT 6
#define STATION_SHELF_MASK  (STATIONS_PER_SHELF - 1)

typedef struct map {
    /*
     * **The table is shelves, not one array** (issue 211).
     *
     * A flat array grows by reallocation, and reallocation moves the
     * mutexes — a thread parked on one would be waiting at an address
     * nobody unlocks. A table built out of shelves does not move
     * anything: growing means allocating one more shelf and writing
     * its pointer here. Every station already placed stays exactly
     * where it was, mutex included, so guarantee S1 is kept rather
     * than argued with.
     *
     * The alternative was lifting the mutex out of the station so the
     * record becomes movable. That trades a shift-and-mask on the
     * delivery path for a pointer chase on the delivery path, and
     * breaks the sentence in this header rather than keeping it. It is
     * written down so the choice reads as a choice.
     */
    station_t **shelves;
    int         n_shelves;

    /*
     * **Only ever grows, and is published last.** A thread reading a
     * stale, smaller count does not see the newest station, and that
     * is harmless: a station nothing is wired to yet cannot be reached
     * by delivery, and the wire that will reach it is drawn after the
     * station exists. The one ordering to enforce is that the station
     * is completely built before the count that reveals it is
     * published.
     */
    _Atomic int n_stations;
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
    /* How many of them the array has room for, which is not always
     * the station count: stations are added one at a time now, so the
     * names grow behind them (issue 212). */
    int    n_named;

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
    pthread_mutex_t   scrap_mutex;
    struct scrap_item *scrap_head;

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
/* N places reserved up front. The table grows a shelf at a time
 * afterwards (issue 211), so this is a convenience rather than a
 * commitment. */
/* {{{ map_station() — issue 211 */
/*
 * Station number n: one shift, one mask, one extra dereference where
 * a flat array had one add. On the delivery path, which is why the
 * cost is named rather than assumed away.
 */
static inline station_t *map_station(map_t *m, int n)
{
    return &m->shelves[n >> STATION_SHELF_SHIFT][n & STATION_SHELF_MASK];
}
/* }}} */

/* {{{ map_add_station() — issue 211 */
/*
 * Make room for one more station and return its index, adding a shelf
 * when the current ones are full.
 *
 * **A removed station's place is reused before the table grows.** A
 * freed position holds nothing stale, because removing a station is
 * what removes the wires to it (issue 216), so the next station placed
 * can simply take it. That makes a program which adds and removes
 * forever reach a steady size rather than climbing.
 *
 * Returns -1 if it cannot grow.
 */
int map_add_station(map_t *m);
/* }}} */

map_t *map_create(int n_stations);

/* {{{ map_create_empty() — issue 211 */
/*
 * A map with no stations at all, grown one at a time afterwards. This
 * is what reading a file does now, so that reading a map and adding a
 * station to a running program are the same act rather than two that
 * must agree. map_create is the same thing with N places reserved up
 * front, kept because a great many tests know exactly how many they
 * want.
 */
map_t *map_create_empty(void);
/* }}} */
/* }}} */

/* {{{ map_place() — issues 201, 202, 207 */
/*
 * Place a box at station index: its shim, its kind, one ring-buffer
 * port per element size given, and the byte size of its return value
 * (zero for a sink). Element sizes are hand-supplied here; from
 * phase 3 they come from the registry, derived from the real C.
 */
void map_place(map_t *m, int station, task_call_t shim, int kind,
               int n_in_ports, const int *elem_sizes, int out_size);
/* }}} */

/* {{{ map_in_port_start_depth() — issue 210b */
/*
 * Tell one port how deep its ring buffer should start, in slots.
 *
 * It is a hint rather than a setting: growth covers being wrong, so
 * nobody has to be right. A port never told anything starts at
 * IN_PORT_DEFAULT_CAPACITY, and a program that guesses low pays a slower
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
void map_in_port_start_depth(map_t *m, int station, int port, int slots);
/* }}} */

/* {{{ map_in_port_convert() — issues 210b, 210f */
/*
 * Change what one port is: name the station, the port, and the tag it
 * is becoming.
 *
 * **Conversion is a field write.** Storage does not move. The slots
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
 * non-guarantee, and the scan is not the only thing that opens gaps.
 *
 * Becoming a static **for the first time** is refused here and goes
 * through map_in_port_static_text, because what a port needs in order
 * to become a static is a *value*, and this call names only a tag.
 * Becoming a static **again** is what this does, and it works because
 * a constant survives being converted away exactly as the slots do.
 *
 * A port that has been a static and is converted away keeps its
 * binding, so a port that goes static, ring, static reads the same
 * value it read before. That is the same rule as the slots: nothing
 * on any path through here is destroyed. It is also what keeps
 * IN_PORT_NONE meaning one thing — "nobody has said yet" — rather than
 * also meaning "somebody said, then said something else."
 */
void map_in_port_convert(map_t *m, int station, int port, int kind);
/* }}} */

/* {{{ map_configure_port() — issue 210g */
/*
 * **Where a port's values come from**, as one operation: a station, a
 * port, a source, and — when the source is a value — the value
 * itself, written as text.
 *
 * `source` is one of the port kinds. Given text, a port becoming a
 * constant takes that value; given none, it returns to the value it
 * held before, which a constant surviving conversion is what makes
 * possible. A port with no source at all is *IN_PORT_NONE*, and a
 * station holding one can never be ready.
 *
 * **Returns NULL when it took, or a sentence saying why not.** The
 * refusal travels upward instead of stopping the program, so a caller
 * reading a file can collect every mistake in it and present them
 * together rather than one per run. The string is valid until this
 * thread's next refusal.
 *
 * Binding, converting and taking a source away were three calls with
 * three shapes; they are cases of this one now. There is one
 * description of what it means to give a port a source, and it is
 * executable — which is what lets reading a file be a sequence of
 * ordinary operations rather than a privileged path.
 */
const char *map_configure_port(map_t *m, int station, int port,
                               int source, const char *text);
/* }}} */

/* {{{ map_check_sources() — issue 210g */
/*
 * Every parameter that has nowhere to get a value, collected into one
 * sentence. NULL when every port on every station has a source.
 *
 * A port without one is the ordinary state of a station somebody has
 * not finished wiring, so this is not asked while a program is being
 * assembled — it is asked at the moment somebody says it is finished.
 * Asking then is what lets the complaint name the station and the
 * port while the person who mis-wired them is still there; a station
 * with an unsourced port otherwise just never runs, and a program
 * that quietly does less than it was asked to is a bad way to learn
 * about a typo.
 *
 * **No exceptions.** A parameter a box could do without was proposed
 * and refused (issue 210h), because it would have been the only
 * exemption to the rule that a station runs when every one of its
 * slots holds a value.
 */
const char *map_check_sources(map_t *m);
/* }}} */

/* {{{ map_bring_up() — issue 212 */
/*
 * **Declare a program finished, check the whole of it, and set going
 * whatever can run without waiting for an arrival.**
 *
 * This is the pass that used to live inside reading a file, which is
 * what made reading a file a privileged act: it validated and seeded
 * in a phase nothing else could enter, and there was a state called
 * *still loading* that only the loader could be in. There is no such
 * state now. A caller assembles a program by whatever route — reading
 * a file, calling the surface, or both — and then says it is
 * finished.
 *
 * **Repeatable.** A station added to a running program is checked and
 * started by the next call; a station already started is not started
 * again. That is what makes "add a station now, wire it in a moment,
 * bring it up" an ordinary sequence rather than a window of
 * invalidity.
 *
 * Returns NULL when the program was sound, or a collected complaint
 * naming every fault found. **Nothing is started when anything is
 * wrong**, because a program that runs half of what it was asked to
 * is worse than one that refuses.
 */
const char *map_bring_up(map_t *m);
/* }}} */

/* {{{ map_name_station() — issue 212 */
/*
 * What to call a station. NULL when taken, or a sentence saying why
 * not.
 *
 * The engine never reads these — every wire is an index. They are for
 * writing a program back out as a file that reads in again, and for a
 * person watching a live view. A program with no names runs perfectly
 * well; it just cannot be described on disk.
 */
const char *map_name_station(map_t *m, int station, const char *name);
/* }}} */

/* {{{ map_connect() — issues 201, 205, 207 */
/*
 * Wire: from a station's output port to a destination station's port.
 * Ports are created on first use, in index order. Repeat with the
 * same port to fan out.
 */
/* {{{ out_port_dests() / dest_set_build() / dest_set_retire() — issue 214 */
/*
 * out_port_dests reads a port's current set. One atomic load, no lock,
 * and the pointer it returns is to something nobody will modify.
 * Null means the port is wired nowhere.
 *
 * dest_set_build makes a new set from an existing one plus or minus
 * one wire; it allocates and never edits what it was given.
 *
 * dest_set_retire files a replaced set in the map's scrapyard. It
 * takes the scrap lock and nothing else.
 */
dest_set_t *out_port_dests(const out_port_t *p);
dest_set_t *dest_set_build(const dest_set_t *from, int add_station,
                           int add_port, int drop_station, int drop_port);

/*
 * **The scrapyard takes anything.** Hand it a pointer and the
 * function that frees it, and it holds on until no worker can still
 * be inside whatever was using it.
 *
 * It began as a place for destination sets a rewire replaced, and it
 * is general because the same question is asked about three different
 * things: a replaced destination set, a removed station's ports and
 * buffers (issue 216), and the compiled code of a box nobody places
 * any more (issue 310). One mechanism rather than three that drift.
 *
 * map_retire sweeps before filing, so a program that changes shape
 * forever reclaims as it goes rather than growing forever.
 */
void        map_retire(map_t *m, void *p, void (*free_fn)(void *));
void        map_scrap_sweep(map_t *m);
int         map_scrap_count(map_t *m);
void        map_scrap_free_all(map_t *m);
/* }}} */

/* {{{ map_remove_station() — issue 216 */
/*
 * Take a station out of a running program and free its place for the
 * next one.
 *
 * **Removing the wires that name it is the first thing it does**, and
 * that is what makes a version tag on every wire unnecessary. A wire
 * exists only as a destination record on some station's output port,
 * so walking every station, every output port, every destination
 * finds all of them — nothing else in the engine names a station.
 * With none left, nothing stale can survive to be followed, and the
 * place can be reused with no tag, no version, and no cost anywhere
 * on the delivery path.
 *
 * The guarantee this changes gets **sharper**, not weaker. It used to
 * say a wire written down today is valid forever, held by nothing
 * ever being removed. It now says a wire never names a station that
 * is not there, held by removal removing the wires to it.
 *
 * The station's ports and buffers go to the scrapyard rather than
 * being freed, because a worker may be running a task from this
 * station right now and will touch them when it finishes.
 *
 * Returns 0, or -1 with a reason on stderr. What it cannot see: a
 * caller **outside** the map holding on to this station's index. The
 * input station deliberately does not remember who delivered into it,
 * so there is nothing to walk. That is undefined rather than
 * defended — a program is reached through its input and output
 * stations, and holding anything else across a removal is your own
 * affair.
 */
int map_remove_station(map_t *m, int station);
/* }}} */

void map_connect(map_t *m, int from_station, int port,
                 int to_station, int to_port);
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
 * Deliver one value into one port of one station: take the mutex,
 * write, run the readiness check, claim if complete, release, then
 * build and push a task if one became due. This is both the interior
 * of the delivery walk and the way a test or a seed drops a value
 * into a map from outside. Returns whether a task became due, which
 * the statistics read as "the deliverer produced one".
 */
int map_deliver_value(map_t *m, int station, int port, const void *value);
/* }}} */

/* {{{ map_deliver() — issue 205 */
/*
 * The delivery walk: the pool's finish hook. Takes a finished task,
 * chooses the outgoing port by the station's kind, and walks that
 * port's destinations delivering the output value to each.
 */
void map_deliver(void *ctx, task_t *t);
/* }}} */

/* {{{ map_in_port_depth() — issue 208 */
/* How many values are waiting in a port right now. Takes the mutex.
 * Exists for demos and diagnostics, not for engine decisions. */
int map_in_port_depth(map_t *m, int station, int port);
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

/*
 * The same thing, with a piece of work done inside the station's hold
 * before the check runs (issue 210d).
 *
 * There is one caller and it is writing a static. A write and the
 * readiness check it triggers both want the station's mutex, and two
 * acquisitions would leave a gap between the value changing and the
 * question being asked. Handing the work in rather than exporting a
 * lock-already-held variant keeps every acquisition of a station's
 * mutex inside the delivery file, where the discipline is written
 * down once.
 */
int map_station_start_after(map_t *m, int station,
                            void (*while_locked)(void *), void *ctx);
/* }}} */

/* ------------------------------------------------------------------ */
/* Statics, which live on the ports that read them (issues 401, 402,  */
/* 405). In 033-statics.c, which is now a reader and a writer of      */
/* values rather than the keeper of a table.                          */
/* ------------------------------------------------------------------ */

/* {{{ map_in_port_static_text() */
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
void map_in_port_static_text(map_t *m, int station, int port,
                             const char *text);
/* }}} */

/* {{{ map_in_port_static_write() — issue 405 */
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
void map_in_port_static_write(map_t *m, int station, int port,
                           const void *bytes, int size);
/* }}} */

/* ------------------------------------------------------------------ */
/* Internal joints between the engine's files. Not part of the       */
/* surface a map author touches.                                      */
/* ------------------------------------------------------------------ */

/* {{{ in_port_slot() / in_port_slot_move() — issue 210c */
/*
 * One slot, and the one way its state ever changes.
 *
 * in_port_slot returns where slot `index`'s value bytes live. The value
 * comes first in a slot and its state sits after it, so that the
 * value keeps the alignment the allocator gave the array — a state
 * byte in front would push every value off by one, which on some
 * machines is a fault and on the rest is slow.
 *
 * in_port_slot_move is the whole state machine: a compare-and-swap from
 * one named state to another, returning whether this caller won it.
 * There is one primitive rather than four named transitions because
 * the rule worth enforcing is *this exact state became that exact
 * state*, and naming the pair at the call site is what makes a
 * reader of the delivery path able to see the machine running. A
 * transition from a state a slot is not in simply fails, which is
 * what makes an illegal move impossible rather than merely
 * discouraged.
 *
 * Two callers race for one slot and exactly one of them wins. The
 * loser is not blocked and does not retry in place — it goes and
 * looks at another slot, which is the property the whole design is
 * for.
 */
void *in_port_slot(const in_port_t *sl, int index);
int   in_port_slot_move(const in_port_t *sl, int index, int from, int to);

/*
 * The same transition on a slot the caller has already located.
 * The scan walks pages and therefore holds the address already; going
 * back through an ordinal would make it resolve a page per candidate,
 * which is a walk down the page list for every slot it looks at
 * (issue 210e).
 */
int   slot_move_at(void *slot, int elem_size, int from, int to);

/*
 * Reading a slot's state, and setting it without a compare-and-swap.
 *
 * Only for transitions whose mover is already unique: a claimer under
 * the station's mutex taking a *ready* slot (other claimers excluded
 * by the lock, and a writer never touches a ready one), or an owner
 * moving a slot it holds in *reserved* or *claimed*. Everywhere else —
 * which means a writer racing another writer for an empty slot — the
 * compare-and-swap above is what makes the loser go elsewhere.
 */
int   slot_state_at(const void *slot, int elem_size);
void  slot_set_at(void *slot, int elem_size, int to);
/* }}} */

/* {{{ in_port_add_page() / in_port_free_pages() — issue 210e */
/*
 * Growing a ring buffer, and the one act that gives it its first page
 * as well — they are the same thing, which is what paging buys.
 * `in_port_add_page` appends one page of `page_slots` slots, all
 * empty, and adds them to the capacity; nothing already there moves.
 * Callers hold the station's mutex, so two threads meeting a full
 * buffer add one page between them rather than one each.
 *
 * `in_port_free_pages` drops the whole list, for teardown and for the
 * one moment a port's page size legitimately changes — its starting
 * depth, which may only be set while the port is empty.
 */
in_port_page_t *in_port_add_page(in_port_t *sl);
void            in_port_free_pages(in_port_t *sl);
/* }}} */

/* {{{ in_port_kind_name() — issue 210b */
/*
 * What a port's tag is called, in the words a person would use. Every
 * refusal that turns somebody away from a port has to say which of the
 * three it found, because "not a buffer" describes two different
 * situations with two different fixes: a static already holds a value
 * and has no room to queue another, while an unconfigured port is one
 * nobody has finished wiring. One table, so a fourth tag would be a
 * row rather than three edits nobody finds.
 */
const char *in_port_kind_name(unsigned char kind);
/* }}} */

/* {{{ station_out_port() */
/* The port at an index, or null if never wired — which delivery
 * reads as "discard". */
out_port_t *station_out_port(station_t *s, int index);
/* }}} */

/* {{{ in_port_constant_free() — teardown joint */
/* A port's constant and, for a string, the characters it points at.
 * Owned by the port and freed with the map. */
void in_port_constant_free(in_port_t *sl);
/* }}} */

/* {{{ in_port_constant_text() — issue 401 */
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
int in_port_constant_text(const in_port_t *sl, char *out, int room);
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
