/*
 * cera.h — everything a program built with this engine may call.
 *
 * What this is: the engine's whole interface, in one file. Take this
 * and cera.c beside it and you have the engine; there is nothing
 * else to install and no include path to configure beyond the
 * directory these two sit in.
 *
 * How it is arranged: seven sections, in dependency order, each one
 * formerly a numbered header. The numbers are kept because they are
 * this project's reading order and the sections are still a story
 * meant to be read in sequence — the pool knows nothing of stations,
 * stations know nothing of the generator, and so on upward.
 *
 * What it does not yet do is distinguish what a consumer may call
 * from how the engine talks to itself. Every declaration here was in
 * a header because one engine file needed to reach another, and
 * narrowing that to a real public surface is issue 902.
 *
 * GENERATED ONCE from the numbered headers by scripts/110-amalgamate.lua
 * (issue 901) and edited by hand from then on. The script is removed
 * when issue 904 deletes the files it read.
 */
#ifndef CERA_H
#define CERA_H

/* ==================================================================
 *
 * 011 — the pool — worker threads and the task ring
 *
 * Was libs/011-pool.h. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * 011-pool.h — the thread pool's public face.
 *
 * What this is: a fixed set of worker threads fed by one first-in
 * first-out queue of task pointers. Work goes in one end; threads on
 * every core take it out the other. The pool knows nothing about
 * boxes, stations, or maps — it moves opaque task structs between
 * threads and calls the one function pointer each task carries.
 *
 * How it does it, in general terms: the queue is a ring that doubles
 * when full, so pushing never blocks on anything slower than a memory
 * copy. Idle workers sleep instead of spinning. The last worker to
 * fall asleep looks at the queue one final time, and if it is truly
 * empty declares the program finished — so the pool shuts itself down
 * the moment there is provably nothing left to do.
 *
 * The whole interface is declared here at once, annotated with the
 * issue that supplies each part, so the header reads as the module's
 * table of contents even while the implementation is still arriving.
 */



#include <stdint.h>

/*
 * A task is one invocation, made concrete (issue 206): created the
 * moment a station's inputs are all present, destroyed by the worker
 * that ran it. The pool reads exactly one field — `call` — and treats
 * the rest as freight; everything the fields *mean* lives on the
 * delivery path.
 *
 * The values behind `in` are copies, claimed under the station's
 * mutex, so a task is self-contained: once built it depends on
 * nothing another thread can change. The whole task — struct,
 * pointer array, value bytes — is one allocation, sized exactly for
 * its box, and freed with one call by the worker that ran it.
 *
 * Phase 1 tests still hang synthetic payloads off the end by
 * embedding this struct first in a larger one; the pool cannot tell
 * and does not care.
 */
typedef struct task cera_task_t;
typedef void (*cera_task_call_t)(cera_task_t *t);

struct task {
    cera_task_call_t call;     /* the shim to run */
    /*
     * **Which program this task belongs to** (issue 212), carried the
     * same way `station` and `port` are — ferried without being
     * interpreted, which is what keeps the pool ignorant of maps. A
     * void pointer for exactly that reason: the pool must not know
     * what this is.
     *
     * Without it a pool serves one program, because finishing a task
     * means resolving a station index, and an index means nothing
     * without the table it indexes. The finish hook therefore had to
     * carry the map, which bound the pool to it.
     *
     * With it, one pool can serve several programs — which is what
     * lets a program start another beside itself, sharing the workers
     * and nothing else.
     */
    void       *owner;
    int32_t     station;  /* which station produced it, so delivery knows where to look */
    int32_t     port;     /* an iterator's assigned exit; inert until phase 5 */
    int32_t     n_in;     /* how many input values ride along */
    void      **in;       /* one claimed value per input port, in parameter order */
    void       *out;      /* where the return value lands; null for a sink */

    /* How long the box took, when the engine is built with timing
     * compiled in. The pool never reads it — it is carried here the
     * same way `station` and `port` are, as something the pool ferries
     * without interpreting, which is what keeps the pool ignorant of
     * maps (issue 405).
     *
     * It rides on the task because the alternative was a process-wide
     * pointer to the running map, so that a shim — which receives only
     * a task — could find the station to charge. That pointer was what
     * made a process able to run only one map at a time, and one
     * optional measurement was the last thing holding it. */
    long        box_ns;
};

typedef struct pool cera_pool_t;

/*
 * The finish hook is delivery's reserved seat (issue 205). After a
 * worker runs a task and before it frees it, the hook is called with
 * the context given at creation. Passing a null hook means "just free
 * it", which is all phase 1 needs. The hook is how the pool stays
 * ignorant of stations while still carrying their values forward.
 */
typedef void (*cera_pool_finish_t)(void *ctx, cera_task_t *t);

/* {{{ cera_pool_create() — issues 101, 102 */
/*
 * n_workers <= 0 means: use the CERAMIC_WORKERS environment variable
 * if set, otherwise one worker per online processor. Workers are
 * spawned immediately but parked at a starting barrier; nothing runs
 * until cera_pool_release. That gap is where a map is seeded (issue 605).
 */
cera_pool_t *cera_pool_create(int n_workers, cera_pool_finish_t finish, void *finish_ctx);
/* }}} */

/* {{{ cera_pool_push() — issues 101, 103 */
/*
 * Enqueue one task. Grows the ring if it is full (a bounded memory
 * copy, never a wait on user code) and wakes every sleeping worker.
 * The task must have been allocated with malloc; the worker that runs
 * it frees it.
 */
void cera_pool_push(cera_pool_t *p, cera_task_t *t);
/* }}} */

/* {{{ cera_pool_pop() — issue 101 */
/*
 * Dequeue the oldest task, or return null when the queue is empty.
 * Never blocks: the decision to sleep on empty belongs to the worker
 * run loop (issue 103), not to the queue.
 */
cera_task_t *cera_pool_pop(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_release() — issue 102 */
/* Release the workers past the starting barrier. Called once. */
void cera_pool_release(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_join() — issue 104 */
/*
 * Wait until the pool has terminated itself — every task run, every
 * worker returned. Termination is decided by the last worker to fall
 * asleep re-checking the queue (issue 104), so this is a wait for
 * genuine completion, not a deadline.
 */
void cera_pool_join(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_destroy() — issue 101 */
/* Free the pool. If workers were started, join them first. */
void cera_pool_destroy(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_submitter_register() / cera_pool_submitter_unregister() — issue 104 */
/*
 * The termination rule assumes nothing outside the pool pushes tasks
 * after the workers are released. Anything that does — a trickle-feed
 * test, a control socket — must hold a registration for as long as it
 * might still push, or the pool can declare itself finished between
 * two of its pushes. Unregistering the last outside submitter nudges
 * the workers so a fully-asleep pool re-evaluates termination.
 */
void cera_pool_submitter_register(cera_pool_t *p);
void cera_pool_submitter_unregister(cera_pool_t *p);
/* }}} */

/* {{{ cera_pool_worker_index() — issue 102 */
/*
 * The index of the worker running the calling thread, or -1 when
 * called from a thread that is not a worker. Costs a thread-local
 * read; exists so phase 7 can attribute statistics per worker.
 */
int cera_pool_worker_index(void);
/* }}} */

/* {{{ cera_pool_worker_count() — issue 102 */
/* How many workers this pool actually has, after defaulting. */
int cera_pool_worker_count(cera_pool_t *p);

/* {{{ cera_pool_worker_epoch() — issue 214 */
/*
 * A worker's epoch: **odd while it is inside a task, even while it is
 * not.** Bumped once at the start of a whole task and once at the end,
 * on a cache line nobody else writes, so it costs one uncontended
 * write per task and no coordination at all.
 *
 * It exists so that somebody wanting to free something a worker might
 * be using — a destination set a rewire replaced, the compiled code of
 * a box nobody places any more — can find out without asking anyone to
 * stop. Snapshot every worker's epoch when you retire the thing; later,
 * a worker whose epoch is now **even**, or **differs from the
 * snapshot**, cannot still be in the task it was in, and therefore
 * cannot be holding it.
 *
 * Nothing waits and nothing spins. An idle worker is asleep and
 * therefore even, so it passes without ever having to move — which is
 * what would otherwise deadlock a sweep against a quiet pool.
 */
uint64_t cera_pool_worker_epoch(cera_pool_t *p, int worker);
/* }}} */

/* {{{ stopping on purpose — issue 106 */
/*
 * **cera_pool_signal_when_finished(pool, signal)** — raise this signal,
 * once, when the pool decides by itself that the work has run out.
 *
 * The thread that wants to know a program is over is usually also
 * waiting to be *told* to stop, and one waiting point woken for two
 * reasons is simpler than two waits that have to be combined. So the
 * end of the work arrives as one more signal, told apart from the
 * others by its number.
 *
 * It is raised at the process rather than at a thread, which needs no
 * thread identity to be recorded and lands wherever somebody is
 * waiting. Opt-in, and it has to be: most signals kill a process by
 * default, so a pool that raised one unasked would end every program
 * that did not expect it.
 *
 * **cera_pool_stop(pool)** — stop starting new things, which is what
 * halting honestly means. A worker inside a box finishes that box,
 * because there is no safe way to interrupt executing C; a worker
 * looking for work finds the flag and returns. Whatever is queued
 * stays queued and is never run, which tearing down afterwards
 * reports rather than hides.
 *
 * **cera_pool_queued(pool)** — how many tasks are waiting right now.
 *
 * **cera_pool_worker_station(pool, worker)** — which station that worker
 * is inside, or -1. A number the pool ferries and never interprets,
 * like the one on the task it came from, and **a number rather than
 * the task's address** on purpose: a report written while a lock is
 * held by something that will never release it must not dereference
 * anything. A stale integer is a wrong answer; a stale pointer is a
 * crash inside the thing that exists to explain a crash.
 */
void cera_pool_signal_when_finished(cera_pool_t *p, int signo);
void cera_pool_stop(cera_pool_t *p);
int  cera_pool_queued(cera_pool_t *p);
int  cera_pool_worker_station(cera_pool_t *p, int worker);

/*
 * **cera_pool_finished(pool)** — whether the pool has already decided the
 * work is over. For telling a caller that arrived too late, rather
 * than letting it push tasks nobody will ever run. The window it
 * detects is closed by making a standing promise before the gate
 * opens, which is issue 104's rule and has not changed.
 */
int  cera_pool_finished(cera_pool_t *p);
/* }}} */
/* }}} */

/* {{{ cera_pool_queue_stats() — issue 105 */
/*
 * The queue's current capacity, its high-water occupancy, and how
 * many times it has grown. Written for the phase 1 demo: every number
 * a demo reports must be measured, and these are the measurements.
 */
void cera_pool_queue_stats(cera_pool_t *p, int *capacity, int *high_water, int *growths);
/* }}} */



/* ==================================================================
 *
 * 018 — stations, ports, wires, and the construction surface
 *
 * Was src/018-station.h. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
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



#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>



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
 * CERA_IN_PORT_NONE is not a third kind of value; it is the absence of a
 * decision (issue 210b). A port in it has been given no source, and a
 * station holding one can never be ready — which is what lets a
 * program be assembled from nothing, a station coming into existence
 * with every port unset and becoming runnable as its ports are given
 * sources one at a time. No null is invented and nothing is ever
 * handed to a box; the readiness walk simply answers no forever.
 */
enum in_port_kind {
    CERA_IN_PORT_RING    = 0,
    CERA_IN_PORT_STATIC  = 1,
    CERA_IN_PORT_NONE    = 2,
    CERA_IN_PORT_KIND_COUNT
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
 * cera_map_in_port_start_depth.
 *
 * Every slot is usable. A spare one used to be held back so that
 * head-equals-tail could mean empty rather than full, and with a
 * state on every slot there is nothing left for it to disambiguate —
 * a full buffer is one where no slot answers empty, which is a
 * question that gets asked directly now (issue 210c).
 */
#define CERA_IN_PORT_DEFAULT_CAPACITY 10

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
    CERA_SLOT_EMPTY    = 0,
    CERA_SLOT_RESERVED = 1,
    CERA_SLOT_READY    = 2,
    CERA_SLOT_CLAIMED  = 3,
};

/*
 * The three station kinds (issue 201, consulted only in phase 5's
 * routing dispatch). Identical in every respect except which output
 * port a returned value goes down.
 */
/*
 * Which way a door faces (issues 209, 213). Not a station kind: a
 * door is an ordinary station of any kind, carrying a mark.
 */
enum station_door {
    CERA_DOOR_NONE = 0,   /* an interior station, which is most of them */
    CERA_DOOR_IN   = 1,   /* the outside may deliver here */
    CERA_DOOR_OUT  = 2,   /* results wait here to be taken */
};

enum station_kind {
    CERA_STATION_PLAIN      = 0,
    CERA_STATION_COMPARATOR = 1,
    CERA_STATION_ITERATOR   = 2,
    CERA_STATION_KIND_COUNT
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
} cera_in_port_page_t;

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
    cera_in_port_page_t *pages;
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
     * How a value of this port's type is written down and read back —
     * the address of a generated pair, written by the placement
     * function because it knows the type concretely (issues 311b, 408).
     *
     * Null unless the type is a struct, and null on a hand-placed
     * station, which is why the reader still checks before following
     * it. It replaced a search of every emitted struct table for one
     * whose name matched, which was a lookup performed to answer a
     * question the placement already knew.
     *
     * **It used to be a table of fields and is now two functions.**
     * The table said where each field sits and how wide it is, and a
     * generalized walk stepped it against the text. The generated pair
     * reaches each field by name instead, so there is no offset stored
     * anywhere and none computed — which is a stronger form of
     * guarantee C1 than a correct number that has to be carried
     * around.
     *
     * Declared as an incomplete type because the pair belongs to the
     * build path and this header must not depend on it — the
     * dependency runs the other way.
     */
    const struct struct_text *text;

    /* The growth story, written by issue 203 and read by phase 7:
     * how many times this buffer has doubled, and the deepest the
     * backlog ever got. A growing port is one input side outpacing
     * its siblings, with memory absorbing the imbalance. */
    int growths;
    int high_water;
} cera_in_port_t;
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
} cera_destination_t;

/* {{{ cera_dest_set_t */
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
    cera_destination_t items[];
} cera_dest_set_t;
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
    _Atomic(cera_dest_set_t *) dests;
    struct out_port      *next;
} cera_out_port_t;
/* }}} */

/*
 * Three-way comparison over raw bytes of two values of one type —
 * the sign of a minus b. Matches the emitted compare functions;
 * declared here generically so this header names nothing the generator emits.
 */
typedef int (*cera_station_compare_t)(const void *a, const void *b);

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
    cera_task_call_t     call;       /* the shim; hand-written until phase 3 */
    unsigned char   kind;       /* plain, comparator, iterator */
    cera_in_port_t      *in_ports;
    int             n_in_ports;
    cera_out_port_t     *out_ports;  /* list; one plain, three comparator */
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
     * one whose call site matched — the emitted table read backwards, which
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

    /*
     * **Whether this station is a door, and which way it faces**
     * (issues 209, 213).
     *
     * One mark rather than two flags, because the two are one design
     * seen from either side: a program's inputs are the ports the
     * outside is allowed to deliver to, and its outputs are the ports
     * a parent may wire from. A station is neither, or one, and being
     * both would mean a program whose entrance is its exit.
     *
     * A door is an **ordinary station** and runs whatever box it was
     * placed with — same shape, same readiness check. The mark adds
     * one rule on each side.
     *
     * Facing out: when its output port is wired nowhere, values are
     * **held** instead of discarded. Discarding is right for every
     * other port and wrong for this one — an unwired comparator
     * branch is the ordinary case, but a program that computed its
     * results and dropped them did nothing.
     *
     * Facing in: it is the only station the outside may deliver to.
     * That is what gives a program a surface rather than internals
     * that happen to be reachable, and it is why a parent can wire to
     * a sub-program without knowing what anything inside it is
     * called.
     */
    unsigned char   door;

    /*
     * The results waiting to be taken, when nobody is wired to this
     * station's output port. Guarded by the station's own mutex — the
     * one the claim already takes — because a worker finishing a box
     * and somebody outside draining results genuinely meet here.
     *
     * Grown by doubling. **A pile-up here is a third diagnosis and
     * not either of the other two**: a port backing up means uneven
     * inputs, the task ring backing up means consumers slower than
     * producers, and this backing up means *nobody is collecting the
     * program's results at all*. It is the loudest of the three by
     * design, because the other two are performance signals and this
     * one means the program is computing into somewhere nobody is
     * looking.
     */
    void           *held;
    int             n_held;
    int             held_room;
    int             held_growths;


    /* Comparator only: the three-way compare for the box's return
     * type, resolved by the placement function so the delivery
     * path does a call rather than a lookup (issue 503). */
    cera_station_compare_t compare;

    /* Phase 7's counters (issue 702). The counts are atomics updated
     * where the work already is, costing nearly nothing, and are
     * always on. The times are only ever written when CERA_STATS is
     * compiled in — the fields stay so the struct never changes
     * shape, but every clock read compiles out. */
    _Atomic long runs;           /* tasks of this station completed */
    _Atomic long produced;       /* tasks its outputs made due elsewhere */
    _Atomic long box_ns;         /* time inside the box function */
    _Atomic long mutex_wait_ns;  /* time deliverers waited on the mutex */
} cera_station_t;
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
#define CERA_STATIONS_PER_SHELF 64
#define CERA_STATION_SHELF_SHIFT 6
#define CERA_STATION_SHELF_MASK  (CERA_STATIONS_PER_SHELF - 1)

/*
 * **A station table belongs to one processor**, and so does the pool
 * that runs it.
 *
 * Not to one machine and not to one core: to the package, the thing
 * with its own memory controller. Every core inside it shares that
 * processor's fast memory, and a table living there is reachable by
 * all of them at the speed everything below assumes. Two processors
 * *can* share a table when the memory holding the stations is
 * reachable from both, and that is the slower path and the exception.
 *
 * Nothing here pins a table to a processor — no affinity, no
 * allocation on a particular node. What is written down is the intent,
 * because the one thing that would make it impossible later is a wire
 * that crosses tables, and there is now a standing reason not to have
 * one beyond the software reason there always was. A program spanning
 * two processors is two programs talking through their doors, not one
 * program with a long wire. See
 * docs/implementation-notes/090-one-table-per-processor.md.
 */
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
    cera_station_t **shelves;
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
    cera_pool_t    *pool;            /* set by cera_map_start; delivery pushes here */

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

    /*
     * **The outside door is shut** (issue 106).
     *
     * Set when a supervisor has politely asked the program to wind
     * down. Delivering an argument from outside is refused from that
     * moment, which is the whole of what "stop accepting new work"
     * can mean here — the entrance is the only way anything outside
     * puts work into a program, so closing it is what lets the queue
     * actually drain and the last-sleeper rule fire.
     *
     * Nothing else changes. No worker is told anything, no task is
     * discarded, no clock starts. The program ends exactly the way it
     * would have ended on its own, which is the argument for this
     * path: it adds no mechanism, only an early trigger for the one
     * that already exists.
     */
    _Atomic int closing;

    /*
     * True when this program was started beside another and shares
     * its workers (issue 212). It borrows the pool and must not
     * destroy it — the program that made it owns it, and tearing down
     * a pool other programs are still running on would take them with
     * it.
     */
    int    pool_is_borrowed;

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
} cera_map_t;
/* }}} */

/* ------------------------------------------------------------------ */
/* Construction calls (issue 207).                                    */
/*                                                                    */
/* SCAFFOLDING, kept deliberately minimal enough to be irritating.    */
/* Phase 6's loader (issue 602) becomes the caller of these; a human  */
/* building maps this way in anger means phase 6 has quietly become   */
/* optional, which is a different and worse project.                  */
/* ------------------------------------------------------------------ */

/* {{{ cera_map_create() — issue 201 */
/* N places reserved up front. The table grows a shelf at a time
 * afterwards (issue 211), so this is a convenience rather than a
 * commitment. */
/* {{{ cera_map_station() — issue 211 */
/*
 * Station number n: one shift, one mask, one extra dereference where
 * a flat array had one add. On the delivery path, which is why the
 * cost is named rather than assumed away.
 */
static inline cera_station_t *cera_map_station(cera_map_t *m, int n)
{
    return &m->shelves[n >> CERA_STATION_SHELF_SHIFT][n & CERA_STATION_SHELF_MASK];
}
/* }}} */

/* {{{ cera_map_add_station() — issue 211 */
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
int cera_map_add_station(cera_map_t *m);
/* }}} */

cera_map_t *cera_map_create(int n_stations);

/* {{{ cera_map_create_empty() — issue 211 */
/*
 * A map with no stations at all, grown one at a time afterwards. This
 * is what reading a file does now, so that reading a map and adding a
 * station to a running program are the same act rather than two that
 * must agree. cera_map_create is the same thing with N places reserved up
 * front, kept because a great many tests know exactly how many they
 * want.
 */
cera_map_t *cera_map_create_empty(void);
/* }}} */
/* }}} */

/* {{{ cera_map_place() — issues 201, 202, 207 */
/*
 * Place a box at station index: its shim, its kind, one ring-buffer
 * port per element size given, and the byte size of its return value
 * (zero for a sink). Element sizes are hand-supplied here; from
 * phase 3 they come from the emitted file, derived from the real C.
 */
void cera_map_place(cera_map_t *m, int station, cera_task_call_t shim, int kind,
               int n_in_ports, const int *elem_sizes, int out_size);
/* }}} */

/* {{{ cera_map_in_port_start_depth() — issue 210b */
/*
 * Tell one port how deep its ring buffer should start, in slots.
 *
 * It is a hint rather than a setting: growth covers being wrong, so
 * nobody has to be right. A port never told anything starts at
 * CERA_IN_PORT_DEFAULT_CAPACITY, and a program that guesses low pays a slower
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
 *
 * **Returns NULL when it took, or a sentence saying why not** (issue
 * 210g). It used to stop the program, and it was the last port
 * operation that did — which made a bad depth the one fault in a map
 * file capable of hiding every fault after it, since a caller
 * collecting mistakes cannot collect the one that killed it.
 */
const char *cera_map_in_port_start_depth(cera_map_t *m, int station, int port,
                                    int slots);
/* }}} */

/* {{{ cera_map_in_port_convert() — issues 210b, 210f */
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
 * through cera_map_in_port_static_text, because what a port needs in order
 * to become a static is a *value*, and this call names only a tag.
 * Becoming a static **again** is what this does, and it works because
 * a constant survives being converted away exactly as the slots do.
 *
 * A port that has been a static and is converted away keeps its
 * binding, so a port that goes static, ring, static reads the same
 * value it read before. That is the same rule as the slots: nothing
 * on any path through here is destroyed. It is also what keeps
 * CERA_IN_PORT_NONE meaning one thing — "nobody has said yet" — rather than
 * also meaning "somebody said, then said something else."
 */
void cera_map_in_port_convert(cera_map_t *m, int station, int port, int kind);
/* }}} */

/* {{{ cera_map_configure_port() — issue 210g */
/*
 * **Where a port's values come from**, as one operation: a station, a
 * port, a source, and — when the source is a value — the value
 * itself, written as text.
 *
 * `source` is one of the port kinds. Given text, a port becoming a
 * constant takes that value; given none, it returns to the value it
 * held before, which a constant surviving conversion is what makes
 * possible. A port with no source at all is *CERA_IN_PORT_NONE*, and a
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
const char *cera_map_configure_port(cera_map_t *m, int station, int port,
                               int source, const char *text);
/* }}} */

/* {{{ cera_map_check_sources() — issue 210g */
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
const char *cera_map_check_sources(cera_map_t *m);
/* }}} */

/* {{{ cera_map_bring_up() — issue 212 */
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
const char *cera_map_bring_up(cera_map_t *m);
/* }}} */

/* {{{ cera_map_designate_output() and collecting — issue 209 */
/*
 * **Where a program's results come from.**
 *
 * A designated station stays an ordinary station — same shape, same
 * readiness, running whatever box it was placed with or none. The
 * designation adds one rule: when its output port is wired nowhere,
 * values are **held** rather than discarded. Discarding is right for
 * every other port and wrong for this one, because a program that
 * computed its results and dropped them did nothing.
 *
 * A program may have several. A box returns one value, so a station
 * has one output port, so a program output is one station; several
 * results are several stations, and each gets the readiness check
 * that already exists rather than a new shape.
 *
 * `cera_map_output_waiting` says how many are held; `cera_map_output_take`
 * takes the oldest, returning 1 or 0. Both exist because a caller
 * asked to drain results cannot write the loop with only one of them.
 */
const char *cera_map_designate_output(cera_map_t *m, int station);

/* {{{ cera_map_station_set_cursor() — issue 712 */
/*
 * Put an iterator back where it had got to: which of its exits the
 * next value takes.
 *
 * This is the one memory a station keeps, so a program written down
 * mid-run and revived with its iterators reset would send the next
 * value to an exit it was never going to — right shape, wrong
 * behaviour, which is the worst way for a capture to be wrong.
 *
 * Refused on anything that is not an iterator, because there is
 * nothing for it to mean: a plain station has one exit and a
 * comparator chooses by comparing.
 */
const char *cera_map_station_set_cursor(cera_map_t *m, int station, int at);

/*
 * The other door: the station the outside is allowed to deliver to.
 *
 * Without it a caller reaches a program by naming one of its interior
 * stations, which means knowing what they are called — rename one and
 * every caller breaks. The mark is what turns internals that happen
 * to be reachable into a surface, and it is what lets a parent wire
 * to a sub-program without knowing anything inside it.
 *
 * A station cannot be both doors; being both would mean a program
 * whose entrance is its exit, and asking for it is almost certainly a
 * mis-named station.
 */
const char *cera_map_designate_input(cera_map_t *m, int station);

/* {{{ cera_map_start_beside() — issue 212 */
/*
 * **Start a fresh program beside this one, sharing its workers.**
 *
 * Composing and starting are two different acts and this is the
 * second. A program started beside another has its own station table,
 * its own rewiring lock, its own everything — except the pool.
 *
 * **You cannot wire to it**, and that is the point rather than a
 * limitation. A wire is a station index and a port index, and an
 * index only means anything inside one station table; two stations
 * can be wired together exactly when they sit in the same table.
 * Reaching a program started beside you is what its entrance is for.
 *
 * Which of the two you want follows from whether you want isolation.
 * A composed program is one graph and a parent wires across the seam
 * like it wires anything. A started one is separate, reached only
 * through its doors, and a program that runs other programs wants
 * that.
 */
/*
 * **This is the within-one-processor case.** Several programs, one
 * pool, one processor's cores, reaching each other only through their
 * doors. The across-processors case is the same shape with a pool
 * each, and it needs nothing added here — a door does not ask where
 * its caller is.
 */
cera_map_t *cera_map_start_beside(cera_map_t *parent);

/*
 * Deliver into a program from outside it. NULL when taken, or a
 * sentence saying why not.
 *
 * Underneath it is the ordinary delivery. What differs is who may use
 * it: this refuses any station that is not a declared entrance, and
 * that refusal is the whole of what gives a program a surface. The
 * size is checked here because a caller from outside is the one least
 * likely to be right about it — inside the graph a wire was checked
 * when it was drawn, and here there is no wire.
 */
const char *cera_map_deliver_argument(cera_map_t *m, int station, int port,
                                 const void *value, int size);
/* {{{ cera_map_deliver_argument_text() / cera_map_deliver_command_line() — issue 213 */
/*
 * **Arguments written as text.** The constant reader pointed at a
 * command line instead of at a map file — same code, same
 * compiler-computed offsets, same messages naming the field that was
 * wrong, and struct arguments in brace syntax for free.
 *
 * `cera_map_deliver_command_line` takes the whole of it: a program's
 * arguments are the input ports of the stations it declared as
 * entrances, in station order and then port order. It holds a
 * standing promise while it delivers and drops it afterwards, which
 * is what stops the program deciding it has finished between two
 * arguments and what lets it end once they are all in.
 *
 * A count that does not match is refused rather than partly
 * delivered: half a command line is a program waiting forever for the
 * rest, which is a worse way to learn about a typo than being told.
 *
 * **A string argument's characters are never freed**, deliberately.
 * The value delivered for a string port *is* a pointer, and what it
 * points at has to outlive every box that might read it — which is
 * the whole run. A command line lives as long as the program does.
 */
const char *cera_map_deliver_argument_text(cera_map_t *m, int station, int port,
                                      const char *text);
const char *cera_map_deliver_command_line(cera_map_t *m, int argc, char **argv);
/* }}} */

int cera_map_output_waiting(cera_map_t *m, int station);
int cera_map_output_take(cera_map_t *m, int station, void *into, int size);
/* }}} */

/* {{{ cera_map_name_station() — issue 212 */
/*
 * What to call a station. NULL when taken, or a sentence saying why
 * not.
 *
 * The engine never reads these — every wire is an index. They are for
 * writing a program back out as a file that reads in again, and for a
 * person watching a live view. A program with no names runs perfectly
 * well; it just cannot be described on disk.
 */
const char *cera_map_name_station(cera_map_t *m, int station, const char *name);
/* }}} */

/* {{{ cera_map_wire() — issue 212 */
/*
 * Draw a wire from a station's output port to another station's input
 * port, **at any moment** — while a program is being assembled or on
 * a running one with workers in flight. NULL when drawn, or a
 * sentence saying why not.
 *
 * There is one implementation and it applies every rule, because the
 * rules were never about *when*. Construction and live editing used
 * to have one each, and construction's was quietly the weaker: it
 * never asked whether the destination was a buffer and never compared
 * the widths, so a program could be built by hand that the same
 * program read from a file would have been refused.
 */
const char *cera_map_wire(cera_map_t *m, int from_station, int port,
                     int to_station, int to_port);
/* }}} */

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
/* }}} */

/* {{{ cera_map_remove_station() — issue 216 */
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
const char *cera_map_remove_station(cera_map_t *m, int station);
/* }}} */

void cera_map_connect(cera_map_t *m, int from_station, int port,
                 int to_station, int to_port);
/* }}} */

/* {{{ cera_map_start() / cera_map_destroy() — issue 207 */
/*
 * cera_map_start creates the pool with delivery as its finish hook.
 * Values injected before cera_pool_release(m->pool) are the seed; the
 * formal seed sweep arrives in phase 6 (issue 605).
 */
void cera_map_start(cera_map_t *m, int n_workers);
void cera_map_destroy(cera_map_t *m);
/* }}} */

/* ------------------------------------------------------------------ */
/* The push path (issues 202–206).                                    */
/* ------------------------------------------------------------------ */

/* {{{ cera_map_deliver_value() — issues 204, 205 */
/*
 * Deliver one value into one port of one station: take the mutex,
 * write, run the readiness check, claim if complete, release, then
 * build and push a task if one became due. This is both the interior
 * of the delivery walk and the way a test or a seed drops a value
 * into a map from outside. Returns whether a task became due, which
 * the statistics read as "the deliverer produced one".
 */
int cera_map_deliver_value(cera_map_t *m, int station, int port, const void *value);
/* }}} */

/* {{{ cera_map_in_port_depth() — issue 208 */
/* How many values are waiting in a port right now. Takes the mutex.
 * Exists for demos and diagnostics, not for engine decisions. */
int cera_map_in_port_depth(cera_map_t *m, int station, int port);
/* }}} */

/* {{{ cera_map_station_try_start() — issues 401, 605 */
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
int cera_map_station_try_start(cera_map_t *m, int station);

/* {{{ cera_map_station_start_while_ready() — issue 712 */
/*
 * Keep starting while the station stays ready, and say how many tasks
 * became due.
 *
 * Asking once is enough on the delivery path — one value arrives, at
 * most one task can become due. It is not enough when a port fills
 * *all at once* while another port has values stacked up in it: a
 * constant being bound, or a revived program putting a captured queue
 * back. The station is then ready several times over, and asking once
 * strands the rest.
 *
 * A station whose every port is a constant is ready forever, because
 * a constant is never consumed. Such a station is asked once and left
 * alone, which is right: nothing accumulates where there is no
 * buffer.
 */
int cera_map_station_start_while_ready(cera_map_t *m, int station);

/*
 * The same drain, for a caller that has already asked once — a
 * constant being written does the write and the first check inside one
 * lock hold, deliberately, so that there is no gap between the value
 * changing and the question being asked. This continues from there.
 *
 * A station with no buffer returns zero without asking anything,
 * because nothing accumulates where there is nothing to accumulate in,
 * and asking again would start it a second time for one change.
 */
int cera_map_station_keep_starting(cera_map_t *m, int station);
/* }}} */

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
int cera_map_station_start_after(cera_map_t *m, int station,
                            void (*while_locked)(void *), void *ctx);
/* }}} */

/* ------------------------------------------------------------------ */
/* Statics, which live on the ports that read them (issues 401, 402,  */
/* 405). In 033-statics.c, which is now a reader and a writer of      */
/* values rather than the keeper of a table.                          */
/* ------------------------------------------------------------------ */

/* {{{ cera_map_in_port_static_text() */
/*
 * Give a port a constant, written as text, and make it a static.
 *
 * The text is parsed here into the port's own storage, shaped by the
 * port's declared type — which is why only stations placed by name
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
void cera_map_in_port_static_text(cera_map_t *m, int station, int port,
                             const char *text);
/* }}} */

/* {{{ cera_map_in_port_static_write() — issue 405 */
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
 * **Writing the value the port already holds still counts**, and the
 * station is asked to run again. A write is a statement — the value
 * is now this — rather than a report of a difference, so whether the
 * bytes happen to match is a fact about the previous value, which the
 * caller said nothing about. Comparing would also be unreliable: a
 * struct arrives as raw bytes, padding included, so two writes meaning
 * one value can differ where nobody wrote anything.
 *
 * Like setting one, writing one runs the readiness check on the
 * station — and keeps asking while it stays ready, so a buffer with
 * work stacked up in it drains rather than releasing one task
 * (issue 712). Writing does not *consume* anything, so a station that
 * was already able to run runs again — which is what makes a chain of
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
void cera_map_in_port_static_write(cera_map_t *m, int station, int port,
                           const void *bytes, int size);
/* }}} */

/* ------------------------------------------------------------------ */
/* Internal joints between the engine's files. Not part of the       */
/* surface a map author touches.                                      */
/* ------------------------------------------------------------------ */

/* {{{ cera_map_in_port_queue_text() — issue 712 */
/*
 * Values put back into a buffer, from the text a capture wrote:
 * comma separated, read one at a time because a struct value has
 * commas inside it.
 *
 * Each goes in through the ordinary delivery, so the readiness check
 * runs and the tasks that form are the tasks that would have formed.
 * Nothing is reconstructed; the same door is used.
 *
 * Returns NULL, or a refusal naming what went wrong.
 */
const char *cera_map_in_port_queue_text(cera_map_t *m, int station, int port,
                                   const char *text);
/* }}} */



/* ==================================================================
 *
 * 026 — what the generator emits, and how a value becomes text
 *
 * Was src/026-emitted.h. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
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



#include <stddef.h>
#include <stdio.h>




/*
 * Three-way comparison over the raw bytes of two values of one type,
 * returning the sign of a minus b. Generic in signature so one table
 * can hold every type's; each generated body copies the bytes into
 * real typed variables first, because comparing raw bytes gives
 * wrong answers — a negative float reads as larger than a positive
 * one byte-wise (issue 305).
 */
typedef int (*cera_compare_fn_t)(const void *a, const void *b);

/* What a struct field fundamentally is, for the statics reader. */
enum field_kind {
    CERA_FIELD_INT = 0,     /* signed integers of any width */
    CERA_FIELD_UINT,        /* unsigned integers of any width */
    CERA_FIELD_FLOAT,       /* float or double */
    CERA_FIELD_STRING,      /* a char array, fixed length, text inside */
    CERA_FIELD_STRUCT,      /* another struct, by its own field table */
};

/* {{{ struct field_info / struct_info */
typedef struct struct_info cera_struct_info_t;

typedef struct field_info {
    const char           *name;
    int                   offset;     /* offsetof, compiler-computed */
    int                   size;       /* sizeof the member */
    unsigned char         kind;
    const cera_struct_info_t  *nested;     /* CERA_FIELD_STRUCT only */
    int                   array_len;  /* CERA_FIELD_STRING only */
} cera_field_info_t;

struct struct_info {
    const char          *name;
    int                  size;
    int                  n_fields;
    const cera_field_info_t  *fields;
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
    void      (*place)(cera_map_t *m, int station, int kind);
} cera_box_place_t;

extern const cera_box_place_t    box_places[];
extern const int            n_box_places;

/* Which placement function writes this box's station. Compiled-in
 * rows first, then anything compiled after the program started. */
const cera_box_place_t *cera_box_place_find(const char *name);

/* The generated data. Defined in src/generated/emitted.c, which the
 * generator rewrites on every build where a box source changed. */
extern const cera_struct_info_t  struct_layouts[];
extern const int            n_struct_layouts;
/* }}} */

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
typedef struct cera_where {
    int station;
    int port;
} cera_where_t;

/*
 * A growing piece of text that never overflows and always reports how
 * much it wanted, so a caller that was cut short can tell, and ask
 * again with more room. The same contract snprintf offers.
 */
typedef struct cera_textbuf {
    char *out;
    int   room;
    int   used;
} cera_textbuf_t;

/* Reading. Each returns where it stopped; each refuses fatally,
 * naming the station, the port and the field. */
const char *cera_text_expect(const char *p, char c, const cera_where_t *w,
                             const char *what);
const char *cera_text_signed(const char *p, void *out, int size,
                             const cera_where_t *w, const char *field);
const char *cera_text_unsigned(const char *p, void *out, int size,
                               const cera_where_t *w, const char *field);
const char *cera_text_floating(const char *p, void *out, int size,
                               const cera_where_t *w, const char *field);
/* A char array, filled to `room` bytes and zero-padded. Bounded by
 * the array rather than by a terminator, because a field filled
 * exactly to its width has no room for one. */
const char *cera_text_chars(const char *p, char *out, int room,
                            const cera_where_t *w, const char *field);

/* Writing. What comes out is what would go back in. */
void cera_text_put(cera_textbuf_t *tb, const char *literal);
void cera_text_put_signed(cera_textbuf_t *tb, const void *bytes, int size);
void cera_text_put_unsigned(cera_textbuf_t *tb, const void *bytes, int size);
void cera_text_put_floating(cera_textbuf_t *tb, const void *bytes, int size);
void cera_text_put_chars(cera_textbuf_t *tb, const char *chars, int room);

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
    const char *(*read)(const char *p, void *out, const cera_where_t *w);
    void        (*write)(const void *bytes, cera_textbuf_t *tb);
} cera_struct_text_t;

extern const cera_struct_text_t struct_texts[];
extern const int           n_struct_texts;

/* One type's pair by name, or NULL. Wanted at placement and by a
 * caller building a port by hand; nothing on the delivery path asks. */
const cera_struct_text_t *cera_struct_text_find(const char *type_name);
/* }}} */

/* {{{ cera_struct_find() */
const cera_struct_info_t *cera_struct_find(const char *type_name);
/* }}} */

/* {{{ cera_emitted_print() */
/* Every box, its parameter types and sizes, its return, its task
 * size — so a build problem is diagnosed by reading what was
 * emitted, not by guessing (issue 303). */
void cera_emitted_print(FILE *out);
/* }}} */


/* {{{ cera_map_place_box() */
/*
 * Place a box at a station by name, sizes drawn from the emitted file
 * instead of typed in by hand — the moment phase 2's hand-supplied
 * element sizes become correct by construction (issue 303). A
 * comparator gets its extra threshold port here, typed to the box's
 * return value (issue 502's shape, placed early so the loader needs
 * no special case).
 */
void cera_map_place_box(cera_map_t *m, int station, const char *box_name, int kind);
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
} cera_box_source_t;

extern const cera_box_source_t cera_box_sources[];
extern const int          cera_n_box_sources;

/* The text of one box source, by the path the build knew it as, or
 * NULL. A bare basename matches too, because that is how a person
 * refers to a file they can see. */
const char *cera_box_source_text(const char *path);
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
    int       (*build)(cera_map_t *m, int *landed, int cap);
} cera_map_build_t;

extern const cera_map_build_t cera_map_builds[];
extern const int         cera_n_map_builds;

/* The build function for one description, by the path the build knew
 * it as, or by the bare name somebody would type. NULL when this
 * program was not built with that map. */
const cera_map_build_t *cera_map_build_find(const char *path);
/* }}} */



/* ==================================================================
 *
 * 040 — loading a description, and a map placed inside a map
 *
 * Was src/040-mapfile.h. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * 040-mapfile.h — a description on disk becoming a running program.
 *
 * What this is: the surface of phase 6, and one operation seen from
 * three distances. Read a whole description into a fresh program;
 * bring one inside a program that already exists; add a part, which
 * is either a box or a description and the caller need not know
 * which.
 *
 * How it does it, in general terms: **by compiling, not by reading**
 * (issue 311d). The text goes to the generator, which turns it into
 * the construction calls it describes, and the compiler that built
 * this binary compiles them. So a description becomes a program
 * exactly one way, and this header is a caller of that way rather
 * than a second implementation of it.
 *
 * The parser those calls are derived from lives with the compiler now
 * (scripts/099-mapparse.h) and is not part of any program built with
 * this engine. Reading text is the compiler's job.
 *
 * Failure is fatal and always names the description — and both ends
 * of a wire — because these messages are the surface a person
 * actually touches.
 */





/* {{{ cera_map_load_file() — issues 602–605 */
/*
 * The whole journey: parse, build every station from its placement function
 * (first pass), resolve and type-check every arrow (second pass),
 * validate what needs the whole map, start the pool with its workers
 * parked, and seed. The caller releases the pool when ready:
 *
 *   cera_map_t *m = cera_map_load_file("program.map", 0);
 *   cera_pool_release(m->pool);
 *   cera_pool_join(m->pool);
 *   cera_map_destroy(m);
 *
 * Every failure between here and the seed is fatal and names its
 * station; validation failures are collected and printed together
 * before stopping, because someone fixing a new map wants the whole
 * list.
 */
cera_map_t *cera_map_load_file(const char *path, int n_workers);

/*
 * The same, for an artifact that says at the top that it lost work
 * (issue 712). Reading one through the ordinary door is refused,
 * because a program quietly missing results somebody computed is the
 * failure this engine refuses everywhere. Salvaging is a different
 * act and has a different name so that whoever does it has said out
 * loud that they know what is missing.
 */
cera_map_t *cera_map_load_salvage(const char *path, int n_workers);
/* }}} */

/* {{{ cera_map_seed_count() — issue 605's reading */
/* How many stations the seed enqueued on load — a map that seeds one
 * when its author expected ten has a wiring mistake. */
int cera_map_seed_count(cera_map_t *m);
/* }}} */

/* {{{ cera_map_instantiate_file() — issue 217 */
/*
 * **A description brought inside a program that already exists.**
 *
 * Reading a file into a fresh program is this with the program fixed
 * at "a new empty one", which is what it always was. What changes is
 * that the program may already have stations in it, so a description
 * is a **template being instantiated** rather than a program being
 * merged: new stations are built for its stations and wired the way
 * it says, and one description can be instantiated as many times into
 * one program as anybody likes with nothing shared between the
 * copies.
 *
 * **Nothing that already exists is renumbered**, so the invariant
 * everything here rests on — an index means what it meant — is never
 * approached. What the description says and where its stations land
 * are two different numbers, related by a table rather than by an
 * offset: adding a station hands back a *freed* place before it grows
 * the table, so a program that has had removals gets whatever holes
 * exist, in whatever order. The offset is what the translation
 * degenerates to when nothing has been removed.
 *
 * Legal at any moment, because every operation it is made of is.
 */
typedef struct map_instance {
    /* Where each of the description's stations landed, in the order
     * the description declared them. The engine needs it; a parent
     * should want the doors instead. */
    int *station;
    int  count;
} cera_map_instance_t;

cera_map_instance_t cera_map_instantiate_file(cera_map_t *m, const char *path);

/*
 * **The nth door of an instance facing that way**, or -1. This is the
 * whole of what a parent is entitled to know about something it
 * brought inside itself: everything that is not a door belongs to the
 * description's author to rename or restructure.
 */
int  cera_map_instance_entrance(cera_map_t *m, const cera_map_instance_t *in, int nth);
int  cera_map_instance_result(cera_map_t *m, const cera_map_instance_t *in, int nth);
void cera_map_instance_free(cera_map_instance_t *in);
/* }}} */

/* {{{ cera_map_add_part() / cera_map_connect_parts() — issue 217 */
/*
 * **Adding a box and adding a map are one operation.**
 *
 * A map is a list of boxes and the wiring between them; a box is a
 * list of one. Adding a map walks its list, instantiates each box and
 * connects them the way it says; adding a box walks a list of length
 * one and connects nothing.
 *
 * **A part is where values go in and where they come out.** For a map
 * those are the stations it declared as doors. For a single box they
 * are the same station, because a box's own input ports are its way
 * in and its own output port is its way out — a box is a map of one
 * station whose doors are itself.
 *
 * That is what lets a handle be passed around without ever being
 * taken apart: everything that consumes one takes it whole, so
 * nothing has to exist whose only job is to pull a field out of it.
 *
 * Which kind a name refers to is **resolved rather than guessed**: a
 * box lives in the binary and a description lives on disk, both are
 * looked for, and finding both or neither is refused with the places
 * that were searched named.
 */
typedef struct map_part {
    int entrance;   /* where values go in, or -1 if nothing may be fed */
    int result;     /* where values come out */
} cera_map_part_t;

const char *cera_map_add_part(cera_map_t *m, const char *what, cera_map_part_t *out);
const char *cera_map_connect_parts(cera_map_t *m, cera_map_part_t from, int from_port,
                              cera_map_part_t to, int to_port);
/* }}} */




/* ==================================================================
 *
 * 049 — reports, the observer thread, the dump, rewiring
 *
 * Was src/049-observe.h. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * 049-observe.h — seeing inside a running map.
 *
 * What this is: phase 7's surface. None of it is required for
 * correctness; all of it is required for confidence. Buffer growth
 * reported instead of silently absorbed, per-station statistics that
 * locate a bottleneck instead of letting it be guessed at, a dump of
 * the loaded map that reads back as a map, and rewiring while it
 * runs.
 *
 * How it does it, in general terms: the counters were left in place
 * by the phases that built the structures (growth counts in phase 2,
 * high water alongside); this phase reads them out loud. Timing is
 * heavier than counting, so it compiles out entirely unless
 * CERA_STATS is defined — a measurement apparatus whose cost is
 * unmeasured is a rumour, and one that cannot be removed is a tax.
 */



#include <stdio.h>



/* ------------------------------------------------------------------ */
/* Reporting (issues 701, 702).                                       */
/* ------------------------------------------------------------------ */

/* {{{ cera_map_report_buffers() — issue 701 */
/*
 * Every port's growth story: doublings, current capacity, high-water
 * occupancy — plus the pool ring's own, because the two piles form
 * in different places (a lesson from phase 2). High water matters
 * more than capacity: a thousand-slot buffer that held two values
 * had one bad moment; one that held nine hundred is a bottleneck.
 */
void cera_map_report_buffers(cera_map_t *m, FILE *out);
/* }}} */

/* {{{ cera_map_report_stations() — issue 702 */
/*
 * Per station: runs, tasks produced for others, and — when compiled
 * with CERA_STATS — time inside the box and time spent waiting on the
 * station's mutex. Three orderings of the same data, because the
 * interesting station is a different one under each.
 *
 * A third time was reported until issue 210: gather time, charged to
 * the station that pulled rather than to the one that ran, because
 * that is where a pulled value was actually paid for. Nothing pulls
 * now, so a station's own box time is the whole of its cost.
 */
enum report_order {
    CERA_REPORT_BY_TIME = 0,
    CERA_REPORT_BY_CONTENTION,
    CERA_REPORT_BY_COUNT,
    CERA_REPORT_ORDER_COUNT
};
void cera_map_report_stations(cera_map_t *m, FILE *out, int order);
/* }}} */

/* {{{ cera_map_observe_start() / cera_map_observe_stop() — issue 701 */
/*
 * Periodic emission of both reports to a file, from a small thread
 * that is not a worker and pushes nothing (so termination stays
 * sound). An interval of zero refuses to start: diagnostics nobody
 * reads are a background thread doing nothing useful.
 */
void cera_map_observe_start(cera_map_t *m, const char *path, int interval_ms);
void cera_map_observe_stop(cera_map_t *m);
/* }}} */

/* {{{ cera_map_report_shutdown() — issue 701's loud parting word */
/* Called by cera_map_destroy: any port grown past the shout threshold is
 * named on stderr, so a quietly-absorbing map gets decided about. */
void cera_map_report_shutdown(cera_map_t *m);
/* }}} */

/* {{{ cera_stats hooks — filled by generated shims and the engine */
/*
 * The shims emitted by the generator call the first of these around
 * every box run when CERA_STATS is defined; the engine calls the
 * rest from the delivery path. All of them are cheap atomics on the
 * station's own counters; all of the *timing* callers compile out
 * without the define.
 */
void cera_stats_box_time(cera_task_t *t, long ns);
/* }}} */

/* ------------------------------------------------------------------ */
/* The dump (issue 703). Lives in 051-dump.c.                         */
/* ------------------------------------------------------------------ */

/* {{{ cera_map_dump() */
/*
 * Write the in-memory station table back out in the map file format,
 * derived facts as comments beside the lines that parse. Dumped from
 * the table, never from any remembered text: the table is what
 * exists, and any disagreement with the original file is a loader
 * bug nothing else would catch. Requires a map loaded from a file
 * (names retained); refuses a hand-built map by name.
 */
void cera_map_dump(cera_map_t *m, FILE *out);
/* }}} */

/* ------------------------------------------------------------------ */
/* Rewiring (issue 704). Lives in 052-rewire.c.                       */
/* ------------------------------------------------------------------ */

/* {{{ cera_map_unwire() / cera_map_disconnect() — issues 704, 106 */
/*
 * Cut one wire on a live map, by rebuilding the destination list
 * without it. `cera_map_unwire` hands a refusal back so a caller can
 * collect it; `cera_map_disconnect` stops the program.
 *
 * **A third face used to sit here and it is gone** (issue 106). It
 * printed the refusal and returned minus one, which 704 chose
 * deliberately — a loader that dies serves its author, while a
 * running engine that dies for one bad control instruction takes the
 * plant down with it. The cost was booked at the time as a debt in
 * plain words: *a caller can ignore a return value*, and an ignored
 * refusal leaves a program running that somebody believes they just
 * edited successfully. The debt is paid off rather than serviced.
 */
const char *cera_map_unwire(cera_map_t *m, int from_station, int port,
                       int to_station, int to_port);
void        cera_map_disconnect(cera_map_t *m, int from_station, int port,
                           int to_station, int to_port);
/* }}} */



/* ==================================================================
 *
 * 073 — code that arrives after the build
 *
 * Was src/073-latebox.h. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * 073-latebox.h — boxes that arrive after the program started.
 *
 * What this is: the door through which C source becomes a box a
 * running program can place. Everything a build does to a box source,
 * done later: run the generator over it, compile the result, load it,
 * and add what it found to the table stations are placed from.
 *
 * How it does it, in general terms: the same generator and the same
 * compiler the build used, invoked as programs. **The same compiler
 * matters and is not incidental** — the build records which one it
 * was and this uses that one, so a program has exactly one answer to
 * `sizeof` by construction rather than by checking (issue 310).
 *
 * Why a signature is not enough, since it is the obvious idea: a
 * signature gives names — the box's name, its parameter type names,
 * its return type name. What it cannot give is **sizes and offsets**,
 * which are what the engine actually runs on, and the standing rule is
 * that the compiler computes every one of them and nothing guesses.
 * So the signature supplies the paperwork and a compiler supplies the
 * numbers. There is no version of this that skips the compiler.
 *
 * What it costs: a program that never brings in new code never invokes
 * a compiler and never needs one present. A program that does needs
 * the toolchain, exactly then, which is the only moment anyone would
 * expect otherwise.
 */






/* {{{ cera_late_compile_source() */
/*
 * Compile C source into the running program and add every box it
 * defines to the table.
 *
 * Returns the number of boxes added, or -1 on failure. A failure
 * leaves the table exactly as it was — nothing is half-added — and
 * puts **the compiler's own output** on stderr rather than a summary
 * of it, because the message that says what is wrong with a piece of C
 * is the one the compiler wrote.
 *
 * The source is saved to the RAM-backed scratch tier at the moment it
 * is compiled, treated exactly like a log: written as it happens,
 * gone at reboot. That is what lets a dump taken afterwards be
 * reloaded by a fresh process on the same machine — the artifact
 * exists before anybody needs it, which is the same reasoning that
 * opens the diagnostic report's destination during startup rather
 * than while dying.
 */
int cera_late_compile_source(const char *c_source);
/* }}} */

/* {{{ cera_late_box_count() / cera_late_box_at() */
/*
 * The boxes added after the program started, in the order they
 * arrived. The compiled-in ones are not included: those are
 * box_places and have always been reachable directly.
 *
 * Lookup by name goes through cera_box_place_find, which walks both, so
 * almost nothing needs these. They exist for a report that wants to
 * say what a program has grown, and for the tests.
 */
int                cera_late_box_count(void);
const cera_box_place_t *cera_late_box_at(int i);
/* }}} */

/* {{{ cera_late_unload_box() */
/*
 * Unload a box added while the program ran, freeing the library its
 * code came in.
 *
 * **Refused while any station in the given map places it.** Unloading
 * code a station names is exactly the crash this is built to avoid,
 * and the check is the cheap half of the problem.
 *
 * The expensive half is that a worker may be *inside* that code right
 * now, having picked up a task for it a moment ago. That is answered
 * by the same per-worker counter the scrapyard uses (issue 214) — the
 * counter deliberately spans a whole task rather than a delivery
 * walk, so it answers "might somebody be inside this box" and "might
 * somebody be inside this station" with one number. The library is
 * handed to the map's scrapyard and closed once nobody can be in it.
 *
 * **What it checks is the map you hand it.** A process running
 * several maps could have another one placing this box, and nothing
 * here can see that, because nothing in this engine is process-wide
 * and there is no list of running maps to consult. Unloading a box
 * another map places is the caller's to avoid; it is stated rather
 * than defended, which is the same footing as reaching into a program
 * by anything other than its input and output stations.
 *
 * Returns 0, or -1 with a reason on stderr.
 */
int cera_late_unload_box(cera_map_t *m, const char *name);
/* }}} */

/* {{{ cera_late_source_dir() */
/* Where saved sources go, so a reloader can find one from a box's
 * name alone. */
const char *cera_late_source_dir(void);
/* }}} */

/* {{{ cera_late_compile_map() — issue 311d */
/*
 * **A description handed to a running program, compiled into it.**
 *
 * The text goes through the same pipe a box source does — write it
 * out, run the generator, run the compiler that built this binary,
 * load the result — and comes back as the function that builds it.
 * Call that function on any program to get the stations and wiring the
 * description asked for.
 *
 * **Nothing is compiled twice.** The boxes the description names are
 * already in this process, so what is compiled is the description and
 * nothing else: no second copy of any box, no call wrappers, no field
 * tables. The generated code declares the functions that build
 * stations for those boxes and binds to the ones this program
 * published.
 *
 * Which is why a program has to publish them, and does — see
 * [098-engine-surface.syms](098-engine-surface.syms.info.md). A
 * description naming a box this program does not hold fails at load,
 * naming the symbol it wanted.
 *
 * Returns the row describing what was compiled — its path, how many
 * stations it declares, and the function that builds it — or NULL with
 * a reason on stderr. The row and its function belong to a library
 * that stays loaded, so they may be kept and used again, on any
 * program.
 */
const cera_map_build_t *cera_late_compile_map(const char *map_text);
/* }}} */

/* {{{ cera_late_spill_sources() — issue 712 */
/*
 * Write every source this program is made of into a directory, under
 * the paths it was compiled as — what the build put in, and everything
 * that has arrived since.
 *
 * This is what lets a captured program be built again somewhere else.
 * A program that grew boxes is made of more than its build compiled,
 * and a description of one names boxes whose source exists nowhere on
 * the machine that reads it. Written beside the description, at the
 * paths the description addresses them by, the two together are a
 * program somebody can build.
 *
 * Returns how many were written, or -1 with a reason on stderr.
 */
int cera_late_spill_sources(const char *dir);
/* }}} */

/* {{{ cera_late_source_text() — issue 311d */
/*
 * **The C one late-arriving source was compiled from**, by the path
 * it was compiled under, or NULL.
 *
 * The text is not copied here and nothing allocates: a loaded object
 * carries its own source as a C array
 * ([311c](../issues/completed/311c-source-rides-in-the-binary.md)),
 * exactly as the program's own generated file does, so this returns a
 * pointer into the loaded object and lives as long as it does.
 *
 * Callers should reach for `cera_box_source_text` instead, which asks this
 * after asking what the build compiled in. Two lookups exist because
 * the two tables live in different objects; one question is asked, and
 * whether a box arrived early or late is not part of it.
 */
const char *cera_late_source_text(const char *path);
/* }}} */



/* ==================================================================
 *
 * 091 — ending a program, and putting one down
 *
 * Was src/091-stopping.h. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * 091-stopping.h — every way a program ends except the happy one.
 *
 * What this is: issue 106. Running out of work is handled and proven
 * elsewhere — the last worker to fall asleep looks once more, finds
 * nothing, and stops everyone. This is the rest: a program told to
 * stop, and a program that found something it refuses to continue
 * past.
 *
 * How it does it, in general terms: **no handler is installed
 * anywhere.** The three signals are blocked in every thread, and the
 * thread that started the program waits for one to arrive as an
 * ordinary value. That is the whole trick, and it removes the hardest
 * constraint in the design — a signal handler may call almost
 * nothing, while a thread that woke up holding a number may call
 * anything at all. It can take locks, format text, walk the station
 * table.
 *
 * The pool's own ending arrives at that same waiting point, as one
 * more signal. One place to wait, woken for two reasons, told apart
 * by which number came back.
 */





/* {{{ exit codes — issue 106 */
/*
 * **Everything below the signal offset belongs to the program**; the
 * offset and above belongs to signals by convention, so no meaning
 * here ever reaches up there.
 *
 * The three middle codes are the ones worth having. They make the
 * distinction this project already draws — a fault the caller can
 * correct and retry, against one it cannot — visible to a shell
 * script rather than only to somebody reading the message.
 */
#define CERA_EXIT_FINISHED     0    /* ran out of work, or was asked to stop */
#define CERA_EXIT_BAD_FILE     65   /* a map file the engine refused        */
#define CERA_EXIT_BAD_CALL     70   /* an invalid construction call         */
#define CERA_EXIT_NO_RESOURCE  71   /* out of memory; no edit fixes it      */
#define CERA_EXIT_INTERRUPTED  130  /* a person interrupted it              */
/* The quit path is not in this list: it aborts, and the shell
 * computes 131 from the signal itself. Naming a number here would be
 * inventing an agreement that already exists. */
/* }}} */

/* {{{ cera_prepare() — issue 106 */
/*
 * **Block the three signals in every thread, and open the report's
 * destination.** Call this before any thread exists, because a thread
 * inherits the mask of the thread that made it — which is what makes
 * "every thread" true without visiting any of them.
 *
 * `report_path` names where a diagnostic report goes; NULL means the
 * project's RAM-backed scratch directory, which is right for something
 * read while debugging and useless as a post-mortem after a reboot —
 * that is what a core dump is for.
 *
 * **The destination is opened now rather than while dying**, and that
 * is not tidiness. The directory lives on a filesystem a reboot
 * empties, so it may be absent, and creating it on a failure path
 * means a system call that can fail for reasons a dying program can do
 * nothing about. A descriptor is an integer, and writing to an integer
 * is the one file operation available on every path here — including
 * the one that is forbidden to take a lock.
 */
void cera_prepare(const char *report_path);

/* Where the report will be written. Valid after cera_prepare. */
const char *cera_report_path(void);
/* }}} */

/* {{{ cera_wait() — issue 106 */
/*
 * **Wait for the program to end, however it ends**, and return the
 * code the process should exit with.
 *
 * The pool must already be released. Three signals are answered, and
 * they form a progression: each needs less cooperation from the
 * program than the one before, and each is the right answer for a
 * program in worse condition than the last.
 *
 * | | polite shutdown | interrupt | quit |
 * |---|---|---|---|
 * | who sends it | a service manager | a person at a terminal | a person who wants evidence |
 * | what it means | wind down, there is time | stop, and tell me why | stop now, leave the body |
 * | diagnostics | none | everything | only what needs no lock |
 * | waits for running boxes | yes | no | no |
 * | how it ends | zero, by the existing rule | 130, explicitly | aborts, leaving a core |
 *
 * **The polite path adds no mechanism at all**, which is the argument
 * for it: it shuts the one door the outside can push work through and
 * then goes back to waiting, so the program ends exactly the way it
 * would have ended on its own. It writes no diagnostics, because
 * nobody asked for any and a supervisor stopping a healthy program
 * does not want a report it did not request.
 *
 * **It borrows a clock rather than inventing one.** On a wedged
 * program this path never completes, which is correct: whatever sent
 * the signal already has a timer and will escalate to the uncatchable
 * kill when it expires. That supervisor's clock is the only clock in
 * the system that knows how long is too long for this deployment, and
 * a guess made here is wrong on a slow machine and wrong differently
 * on a fast one.
 *
 * **A second interrupt skips everything and exits at once.** Ctrl+C
 * twice has to always work; a diagnostic path that can itself get
 * stuck would trap the person it was written for, which is the exact
 * failure it exists to prevent.
 */
int cera_wait(cera_map_t *m);
/* }}} */

/* {{{ cera_capture() / cera_capture_now() — issue 712 */
/*
 * **Put a running program down on disk so it can be picked up again.**
 *
 * `cera_capture` is the polite one. It shuts the entrance — the only
 * way anything outside pushes work in — lets everything already in
 * flight finish and deliver, waits for the workers to go home, and
 * then writes the artifact. What it produces is **complete** by
 * construction: no task was running when it was written, so nothing
 * was lost.
 *
 * **There is no bound on the wait, deliberately.** Quiet is decidable
 * exactly — the pool knows when its queue is empty and every worker
 * is asleep — so the only case that never returns is a box that never
 * returns, and nothing inside the process can tell that from a box
 * that is merely slow. Whoever asked for the capture already has a
 * clock, and theirs is the only one that knows how long is too long
 * for this deployment. This is the same reasoning that keeps a
 * timeout out of `cera_wait`, and it is not weaker here.
 *
 * `cera_capture_now` is for when that clock runs out. It writes
 * immediately, whatever is happening, and the artifact says so: a
 * header naming every station a worker is still inside, and the plain
 * statement that their inputs are lost and their results were never
 * delivered.
 *
 * **The incompleteness is stated, never inferred.** A program reviving
 * such an artifact without being told would be quietly missing work
 * somebody computed, which is the standing rule of this engine wearing
 * a different hat: a fallback is a warning and a warning is an error.
 * Reading one back is refused unless the caller asks for salvage.
 *
 * Both return 0, or -1 with a reason on stderr.
 */
int cera_capture(cera_map_t *m, const char *path);
int cera_capture_now(cera_map_t *m, const char *path);

/*
 * **A capture that stands alone**, into a directory: the description,
 * and beside it every source the program is made of, at the paths the
 * description addresses them by.
 *
 * The plain capture writes a description and nothing else, which is
 * enough for a program whose boxes all came from its build — anything
 * built the same way already has them. It is **not** enough for a
 * program that grew boxes while it ran: such a program is made of more
 * than its build compiled, and its description names boxes whose source
 * exists nowhere on the machine that reads it.
 *
 * What this produces is a program somebody can build. Building it
 * needs the engine, which is what building anything with this engine
 * needs — the same dependency a consumer already has, not a new one.
 * The binary that comes out needs no toolchain of its own, because by
 * then every box is compiled in like any other.
 *
 * Sources are written before the description, so a half-written
 * capture is a directory obviously missing its description rather than
 * one whose description names sources that are not there — the first
 * cannot be mistaken for whole and the second can.
 *
 * Drains first, exactly as the plain capture does. Returns 0 or -1.
 */
int cera_capture_whole(cera_map_t *m, const char *dir);
/* }}} */

/* {{{ cera_stop_now() — issue 106 */
/*
 * **An invalid operation ends the program**, having first said
 * everything it can about what went wrong.
 *
 * Not a return value a caller may discard. The surface still hands a
 * refusal *back* rather than dying where the failure happens, because
 * a caller reading a file collects every mistake in it and presents
 * them together — so a refusal travels, accumulates, and the stop
 * happens once with the whole list. A single instruction arriving
 * alone produces a list of one.
 *
 * What this buys is that **a program cannot be left half-built by
 * ignored refusals**, because there is no surviving path in which a
 * refused instruction leaves a program running that somebody believes
 * they just edited successfully.
 *
 * `m` may be NULL when there is no program to describe yet.
 */
void cera_stop_now(cera_map_t *m, int exit_code, const char *why);
/* }}} */



#endif /* CERA_H */
