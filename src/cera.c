/*
 * cera.c — the engine, entire.
 *
 * What this is: one translation unit holding every part of the
 * runtime — the thread pool, the station table, the delivery path,
 * constants, the reader, the reports, the parts that change a
 * running program, the parts that compile new code into one, and
 * the parts that end one.
 *
 * Why one file rather than eleven. A function in the same
 * translation unit as its callers can be static, and a static
 * function is not a linker symbol at all. Eleven files meant every
 * joint between them had to be a global name, so a host program
 * linking this engine inherited about forty ordinary English words
 * it never asked for. One file makes private the default and public
 * a deliberate act — the act being a declaration in cera.h.
 *
 * How it is arranged: eleven sections in the project's reading
 * order, each formerly a numbered file, each opening with a banner
 * naming what it was. A #line directive at every seam keeps compiler
 * errors and debugger backtraces pointing at the original source.
 *
 * GENERATED ONCE from the numbered bodies by scripts/110-amalgamate.lua
 * (issue 901) and edited by hand from then on.
 */
#include "cera.h"

/* ==================================================================
 *
 * The joints — how the engine reaches itself
 *
 * These were declarations in a header, because a header was the only
 * way one engine file could reach another. Nothing outside calls them,
 * and after issue 903 nothing outside can: they are private, and a
 * private function is not a linker symbol at all.
 *
 * They are declared here rather than left to fall out of definition
 * order because the sections below call each other in both directions
 * — delivery reaches a slot the station layer defines, and the station
 * layer builds a task delivery owns.
 *
 * Their documentation came with them. It was written to explain the
 * machinery, and the machinery is here.
 * ================================================================== */

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
static dest_set_t *out_port_dests(const out_port_t *p);
static dest_set_t *dest_set_build(const dest_set_t *from, int add_station,
                           int add_port, int drop_station, int drop_port);

/* {{{ map_deliver() — issue 205 */
/*
 * The delivery walk: the pool's finish hook. Takes a finished task,
 * chooses the outgoing port by the station's kind, and walks that
 * port's destinations delivering the output value to each.
 */
static void map_deliver(void *ctx, task_t *t);
/* }}} */

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
static void *in_port_slot(const in_port_t *sl, int index);
static int   in_port_slot_move(const in_port_t *sl, int index, int from, int to);

/*
 * The same transition on a slot the caller has already located.
 * The scan walks pages and therefore holds the address already; going
 * back through an ordinal would make it resolve a page per candidate,
 * which is a walk down the page list for every slot it looks at
 * (issue 210e).
 */
static int   slot_move_at(void *slot, int elem_size, int from, int to);

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
static int   slot_state_at(const void *slot, int elem_size);

/* {{{ in_port_waiting_text() — issue 712 */
/*
 * Every value waiting in this port's buffer, written down as text,
 * comma separated. Returns how many characters it wanted — ask with
 * no room, allocate, ask again — and zero when nothing is waiting.
 *
 * The order is slot order, which the engine does not promise means
 * anything: a port has no head and no tail, and the guarantees page
 * says nothing is promised about the order values leave one. A
 * capture writes them as stored and a revival delivers them back that
 * way, which is exactly as faithful as the engine is.
 */
static int in_port_waiting_text(const in_port_t *sl, char *out, int room);
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
static in_port_page_t *in_port_add_page(in_port_t *sl);
static void            in_port_free_pages(in_port_t *sl);
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
static const char *in_port_kind_name(unsigned char kind);
/* }}} */

/* {{{ station_out_port() */
/* The port at an index, or null if never wired — which delivery
 * reads as "discard". */
static out_port_t *station_out_port(station_t *s, int index);
/* }}} */

/* {{{ in_port_constant_free() — teardown joint */
/* A port's constant and, for a string, the characters it points at.
 * Owned by the port and freed with the map. */
static void in_port_constant_free(in_port_t *sl);
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
static int in_port_constant_text(const in_port_t *sl, char *out, int room);
/* }}} */

/* {{{ task_build() — the one way a task comes into existence */
/*
 * Exposed so the seed sweep (issue 605) creates its first tasks
 * through the same path delivery uses — one way, not two. The claimed
 * buffer carries one value per port, of every kind: statics are
 * claimed under the station's mutex beside the ring pops now (issue
 * 401), so nothing is left to resolve here.
 */
static task_t *task_build(map_t *m, int station_index,
                   const unsigned char *claimed, int port);
/* }}} */

/*
 * Whether one row is what a name refers to. Three forms, one rule: a
 * bare function name, a basename and a function, or a path and a
 * function (issue 311a). Exposed because the compiled-in rows and the
 * rows that arrived while the program ran are searched separately and
 * must agree about what a name means.
 */
static int box_place_matches(const box_place_t *row, const char *name);
/* }}} */

/*
 * Two joints that shared a documentation block with a call that is
 * public, which is why they arrived here separately: splitting a block
 * is a judgement about prose rather than a move.
 *
 * slot_set_at had drifted away from the slot calls it belongs with and
 * was sitting under an unrelated one.
 */
static void  slot_set_at(void *slot, int elem_size, int to);

/*
 * The scrapyard, entire. A thing the engine has stopped using cannot
 * be freed at once, because a worker may still be reading it; it is
 * filed, and freed when no worker can still be inside the epoch it was
 * filed in. The same question is asked about three different things —
 * a replaced destination set, a removed station's ports and buffers
 * (issue 216), and the compiled code of a box nobody places any more
 * (issue 310) — so there is one mechanism rather than three that
 * drift.
 *
 * map_retire sweeps before filing, so a program that changes shape
 * forever reclaims as it goes rather than growing forever.
 */
static void        map_retire(map_t *m, void *p, void (*free_fn)(void *));
static void        map_scrap_sweep(map_t *m);
static int         map_scrap_count(map_t *m);
static void        map_scrap_free_all(map_t *m);

/*
 * A joint that only a white-box test reaches.
 *
 * The white-box tests compile as one unit with this file (issue 903),
 * so from here "who calls this" depends on which unit is being built:
 * inside 064's unit the slot-state joint has a caller, inside 077's the
 * scrapyard count has one, and when this file is compiled on its own
 * neither does. The compiler is right to say so, and this says back
 * that it is expected.
 *
 * Marked rather than deleted, because deleting one is a decision about
 * the engine's shape rather than about moving it. The ordinal form of a
 * slot move is exactly the address form applied to a located slot, and
 * the engine uses the address form everywhere for the page-walk reason
 * documented beside it — so whether the ordinal form should exist at
 * all is a real question, and not this issue's to answer.
 */
#define CERA_TEST_ONLY __attribute__((unused))

/* ================================================================== */


/* ==================================================================
 *
 * 012 — the pool
 *
 * Was libs/012-pool.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/libs/012-pool.c"
/*
 * 012-pool.c — the thread pool: one queue, many workers, no boss.
 *
 * What this is: the engine's only scheduler. Work arrives as task
 * structs pushed into a ring; worker threads take them out and run
 * them. There is no thread that decides who does what — whoever is
 * free takes whatever is oldest.
 *
 * How it does it, in general terms: one mutex guards the ring, a
 * count of sleeping workers, and the stop flag, so every decision
 * about "is there work" and "is anyone awake" is made under a single
 * lock and cannot tear. The ring holds pointers out to heap-allocated
 * tasks and nothing holds pointers into the ring, so the ring may be
 * reallocated to twice its size whenever it fills — growth moves the
 * shelf, never the boxes on it.
 *
 * Built across issues 101 (the ring), 102 (workers), 103 (sleeping),
 * and 104 (termination).
 */


#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>   /* raising the finished signal (issue 106) */

/*
 * The starting capacity is deliberately small. Growth is cheap and
 * the phase 1 demo wants to show it happening; a generous initial
 * size would only hide the mechanism. Chosen during issue 101.
 */
#define POOL_INITIAL_CAPACITY 8

/* {{{ struct worker */
/*
 * One worker thread's identity: which pthread it is, which index it
 * answers to, and the pool it belongs to. The index is what phase 7
 * statistics will be keyed on.
 */
typedef struct worker {
    pthread_t thread;
    int       index;
    pool_t   *pool;
} worker_t;
/* }}} */

/* {{{ struct pool */
/* {{{ struct pool_epoch */
/*
 * A worker's epoch, padded to a cache line. Two workers bumping their
 * own counters must not write the same line, or an uncontended write
 * becomes a contended one and the whole point of the mechanism — that
 * it costs nothing — is lost.
 */
struct pool_epoch {
    _Atomic uint64_t v;
    /*
     * **Which station this worker is inside**, or -1 (issue 106).
     *
     * A number the pool ferries and never interprets, exactly like
     * the one on the task it came from. It rides here rather than in
     * an array of its own because it is written at the same two
     * moments as the epoch beside it and by the same thread, so it
     * costs one more write to a line that is already being written
     * and is already nobody else's.
     *
     * **A number rather than the task's address**, and that is the
     * whole reason it exists at all. The report written when somebody
     * hits ctrl+C wants to say which station each worker was in, and
     * the report written when somebody hits ctrl+backslash wants to
     * say it while taking no locks and touching nothing that could
     * have been freed. A stale integer is a wrong answer; a stale
     * pointer is a crash inside the thing that exists to explain a
     * crash.
     */
    _Atomic int32_t  station;
    char pad[64 - sizeof(_Atomic uint64_t) - sizeof(_Atomic int32_t)];
};
/* }}} */

struct pool {
    /* The ring. `slots` holds pointers out to tasks; capacity is the
     * array length; head is the oldest task, tail the next free slot.
     * One slot is always left empty so head==tail means empty and
     * never means full. All four are guarded by `mutex`. */
    task_t **slots;
    int      capacity;
    int      head;
    int      tail;

    pthread_mutex_t mutex;

    /* Woken whenever a task is pushed or the pool is stopping.
     * Sleeping workers wait here. Arrives with issue 103. */
    pthread_cond_t wake;

    /* Workers park here at creation until pool_release. Arrives with
     * issue 102. */
    pthread_cond_t start_gate;
    int            released;

    worker_t *workers;
    int       n_workers;

    /* How many workers are registered asleep, exact because it is
     * only ever touched under `mutex`. Issue 104's termination rule
     * reads it. */
    int sleeping;

    /* Set once, by the last sleeper finding the queue truly empty,
     * or by pool_destroy on a never-released pool. Every worker that
     * sees it returns. */
    int stop;

    /* Outside submitters — threads that are not workers but may still
     * push. While any are registered, an all-asleep pool is idle, not
     * finished. */
    int outside;

    /* Whether the workers have already been collected. Joining a
     * thread twice is an error the platform reports confusingly, so
     * join is made idempotent instead — destroy after join is the
     * normal call order and must stay legal. */
    int joined;

    /* Delivery's seat: called after a task runs, before it is freed. */
    /*
     * One epoch per worker, each on its own cache line so two workers
     * bumping theirs never write the same line (issue 214). Odd while
     * inside a task, even while not. Sixty-four bits, so a counter
     * cannot wrap all the way back to a snapshot in any run and read
     * as unchanged when it is not.
     */
    struct pool_epoch *epochs;

    pool_finish_t finish;
    void         *finish_ctx;

    /* Measurements for the phase 1 demo: the queue's own story. */
    int high_water;
    int growths;

    /*
     * **A signal to raise when this pool finishes by itself**, or 0
     * (issue 106).
     *
     * The last worker to fall asleep decides a program is over, and
     * the thread that wants to know is waiting for a *signal* —
     * because it is also waiting to be told to stop, and one waiting
     * point woken for two reasons is simpler than two waits. So the
     * end of the work becomes one more signal, told apart from the
     * others by its number.
     *
     * Opt-in, and it has to be: the default action for most signals
     * is to kill the process, so a pool that raised one unasked would
     * end every program that does not expect it. Nothing sets this
     * unless somebody is waiting for it.
     */
    int finished_signal;
};
/* }}} */

/* {{{ queue_count() */
/* How many tasks the ring currently holds. Caller holds the mutex. */
static int queue_count(pool_t *p)
{
    /* Two paths: unwrapped (tail ahead of head) is plain subtraction;
     * wrapped (tail behind head) adds one lap of the ring. */
    if (p->tail >= p->head)
        return p->tail - p->head;
    return p->tail + p->capacity - p->head;
}
/* }}} */

/* {{{ queue_grow() */
/*
 * Double the ring. Caller holds the mutex, so nothing else is inside.
 *
 * The wrapped portion must be copied so the queue reads contiguously
 * again — a ring that has wrapped stores its oldest entries at the
 * high end of the array and its newest at the low end, and doubling
 * the array without unwrapping would leave a hole in the middle of
 * the line.
 *
 * Safe because the ring holds pointers out to tasks and nothing holds
 * pointers into the ring: the shelf moves, the boxes on it do not.
 */
static void queue_grow(pool_t *p)
{
    int held = queue_count(p);
    int new_capacity = p->capacity * 2;
    task_t **fresh = malloc((size_t)new_capacity * sizeof *fresh);
    if (!fresh) {
        fprintf(stderr, "pool: queue growth to %d entries failed: out of memory\n",
                new_capacity);
        abort();
    }

    /* Copy oldest-first so the new ring starts at zero. Two paths:
     * an unwrapped queue is one block, a wrapped one is two. */
    if (p->tail >= p->head) {
        memcpy(fresh, p->slots + p->head, (size_t)held * sizeof *fresh);
    } else {
        int first_block = p->capacity - p->head;
        memcpy(fresh, p->slots + p->head, (size_t)first_block * sizeof *fresh);
        memcpy(fresh + first_block, p->slots, (size_t)p->tail * sizeof *fresh);
    }

    free(p->slots);
    p->slots = fresh;
    p->capacity = new_capacity;
    p->head = 0;
    p->tail = held;
    p->growths++;
}
/* }}} */

/* {{{ pool_push() */
void pool_push(pool_t *p, task_t *t)
{
    pthread_mutex_lock(&p->mutex);

    /* If advancing the tail would land on the head, the ring is one
     * short of full — grow before writing, never overwrite. */
    if ((p->tail + 1) % p->capacity == p->head)
        queue_grow(p);

    p->slots[p->tail] = t;
    p->tail = (p->tail + 1) % p->capacity;

    int held = queue_count(p);
    if (held > p->high_water)
        p->high_water = held;

    /* Wake every sleeper rather than a chosen one: there is no
     * information that would make choosing better than arbitrary,
     * and whoever arrives first takes the task (issue 103). */
    pthread_cond_broadcast(&p->wake);

    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* {{{ pool_pop() */
task_t *pool_pop(pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    task_t *t = NULL;
    /* Two paths: an empty ring returns null — the caller decides
     * whether that means sleep (a worker) or done (a test); a
     * non-empty ring surrenders its oldest entry. */
    if (p->head != p->tail) {
        t = p->slots[p->head];
        p->head = (p->head + 1) % p->capacity;
    }
    pthread_mutex_unlock(&p->mutex);
    return t;
}
/* }}} */

/* {{{ worker identity — thread-local index */
/*
 * Each worker thread notes its own index here at startup. Any code
 * running on that thread — a box, a shim, the statistics — can ask
 * cheaply which worker it is on. Threads that are not workers, such
 * as the main thread, read the initial -1.
 */
static __thread int this_worker_index = -1;

int pool_worker_index(void)
{
    return this_worker_index;
}
/* }}} */

/* {{{ pool_worker_epoch() */
uint64_t pool_worker_epoch(pool_t *p, int worker)
{
    if (!p || !p->epochs || worker < 0 || worker >= p->n_workers)
        return 0;
    return atomic_load_explicit(&p->epochs[worker].v, memory_order_acquire);
}
/* }}} */

/* {{{ worker_main() */
/*
 * The run loop (issues 102, 103, 104). The shape is one big loop
 * under the mutex, dropping it only while actually running a task:
 *
 *   - stop set        -> return, letting pool_join collect us.
 *   - queue non-empty -> pop, unlock, run, deliver, free, relock.
 *   - queue empty     -> register asleep. If that registration makes
 *                        every worker asleep and nobody outside can
 *                        still push, look one last time and declare
 *                        the program finished. Otherwise wait.
 *
 * The check-queue and register-asleep steps happen inside one hold
 * of the mutex, and the wait releases that same mutex atomically, so
 * the lost-wakeup race described in issue 104 — a worker preempted
 * between deciding to sleep and registering — has no window here at
 * all. The re-scan the issue demands survives as the last sleeper's
 * final look at the queue before declaring termination; see the
 * first-pass report for why the race itself is unreproducible in
 * this lock discipline.
 */
static void *worker_main(void *arg)
{
    worker_t *w = arg;
    pool_t *p = w->pool;
    int finished_here = 0;

    this_worker_index = w->index;

    pthread_mutex_lock(&p->mutex);

    /* Park at the starting gate until pool_release. Seeding happens
     * while everyone is parked here, which is what keeps issue 104's
     * "nothing pushes from outside after startup" true for maps. */
    while (!p->released && !p->stop)
        pthread_cond_wait(&p->start_gate, &p->mutex);

    for (;;) {
        if (p->stop)
            break;

        if (p->head != p->tail) {
            /* Work exists: take the oldest and run it with the lock
             * dropped, so the queue stays open while user code runs. */
            task_t *t = p->slots[p->head];
            p->head = (p->head + 1) % p->capacity;
            pthread_mutex_unlock(&p->mutex);

            /*
             * The epoch, bumped around **the whole task** rather than
             * around any one part of it (issue 214). Odd means this
             * worker is inside a task; even means it is not.
             *
             * One counter for the whole task rather than one per
             * window is deliberate. Two things want to know whether a
             * worker might be inside something: reclaiming a
             * destination set a rewire replaced, and unloading the
             * compiled code of a box nobody places any more. A worker
             * is inside a box earlier in a task than it is inside a
             * delivery walk, so a counter spanning the whole task
             * answers both. Destination sets become freeable slightly
             * later than they strictly must, which costs nothing
             * anybody measures, and there is one mechanism instead of
             * two that drift apart.
             *
             * It is a relaxed store to a line nobody else writes, so
             * it costs one uncontended write per task and no
             * coordination whatsoever.
             */
            _Atomic uint64_t *epoch = &p->epochs[this_worker_index].v;
            atomic_store_explicit(epoch,
                atomic_load_explicit(epoch, memory_order_relaxed) + 1,
                memory_order_release);

            /* Which station, for a report somebody may ask for while
             * this is still running (issue 106). Ferried, not read. */
            atomic_store_explicit(&p->epochs[this_worker_index].station,
                                  t->station, memory_order_relaxed);

            t->call(t);
            if (p->finish)
                p->finish(p->finish_ctx, t);
            free(t);

            atomic_store_explicit(&p->epochs[this_worker_index].station,
                                  -1, memory_order_relaxed);

            atomic_store_explicit(epoch,
                atomic_load_explicit(epoch, memory_order_relaxed) + 1,
                memory_order_release);

            pthread_mutex_lock(&p->mutex);
            continue;
        }

        /* Empty. Register asleep — count and decision under the same
         * lock, which is what keeps the count exact (issue 103). */
        p->sleeping++;

        if (p->sleeping == p->n_workers && p->outside == 0) {
            /* Every worker is now asleep and nobody outside can
             * push. This thread's registration completed the count,
             * so it looks one final time (issue 104). Under this
             * mutex the queue cannot have changed since the check
             * above — the look is kept anyway, as the documented
             * protocol, and because a future refactor that splits
             * the locking will need it to be genuinely load-bearing. */
            if (p->head != p->tail) {
                p->sleeping--;
                continue;
            }
            /* Truly finished: every other worker already found
             * nothing, so nobody is left who could enqueue. Stop by
             * broadcast, never by breaking out alone — a worker left
             * parked in a wait turns shutdown into a hang. */
            p->stop = 1;
            p->sleeping--;
            pthread_cond_broadcast(&p->wake);
            /* Tell whoever is waiting for a signal that the work ran
             * out (issue 106). Raised after the lock is released, at
             * the bottom of this function, because a signal delivered
             * to a thread that then wants this mutex would find it
             * held by the thread that raised it. */
            finished_here = p->finished_signal;
            break;
        }

        pthread_cond_wait(&p->wake, &p->mutex);
        p->sleeping--;
        /* Waking proves nothing: several workers wake for one task,
         * and waits can end spuriously. Loop back and look again. */
    }

    pthread_mutex_unlock(&p->mutex);

    /*
     * The one worker that decided the program was over says so, to
     * the process rather than to a thread (issue 106). Every thread
     * blocks these signals, so it stays pending until whoever is
     * waiting asks for it — which is exactly the handoff wanted, and
     * needs no thread identity to be recorded anywhere.
     */
    if (finished_here)
        kill(getpid(), finished_here);
    return NULL;
}
/* }}} */

/* {{{ pool_signal_when_finished() */
void pool_signal_when_finished(pool_t *p, int signo)
{
    pthread_mutex_lock(&p->mutex);
    p->finished_signal = signo;
    /*
     * **It may already have happened**, and asking afterwards must
     * still get an answer. A short program can run out of work
     * between being released and anybody sitting down to wait, and a
     * waiter that then waits for a signal nobody will ever raise
     * waits forever — which is the exact failure this line exists to
     * prevent, found by a test that hung.
     *
     * Raising it now rather than remembering to raise it later keeps
     * the two cases one case: whoever waits gets the signal, and it
     * does not matter which side of the finish they arrived on.
     */
    int already = p->stop;
    pthread_mutex_unlock(&p->mutex);
    if (already && signo)
        kill(getpid(), signo);
}
/* }}} */

/* {{{ pool_stop() */
/*
 * **Stop starting new things**, which is what halting honestly means
 * here (issue 106). A worker inside a box finishes that box, because
 * there is no safe way to interrupt executing C; a worker looking for
 * work finds the flag instead and returns.
 *
 * Whatever is still queued stays queued and is never run. That is
 * visible rather than hidden: tearing the pool down afterwards says
 * how many were left.
 */
void pool_stop(pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    p->stop = 1;
    pthread_cond_broadcast(&p->wake);
    pthread_cond_broadcast(&p->start_gate);
    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* {{{ pool_finished() */
/*
 * **Whether this pool has already decided the work is over.**
 *
 * It exists so that a caller arriving too late can be *told* rather
 * than quietly achieving nothing. The window is real and has caught
 * two different callers: a program that seeds nothing has an empty
 * queue and, until somebody registers a standing promise, nobody
 * promising anything — so between the gate opening and the first
 * delivery the last sleeper correctly declares it finished. Anything
 * pushed afterwards is a task nobody will ever run.
 *
 * The rule that prevents it is issue 104's and has not changed: make
 * the promise before opening the gate. This is how somebody finds out
 * they did not.
 */
int pool_finished(pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    int done = p->stop;
    pthread_mutex_unlock(&p->mutex);
    return done;
}
/* }}} */

/* {{{ pool_queued() / pool_worker_station() */
int pool_queued(pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    int n = queue_count(p);
    pthread_mutex_unlock(&p->mutex);
    return n;
}

/*
 * **Readable from any thread, holding nothing.** This is the one
 * measurement the report written under a held lock is allowed to
 * take, so it must be a plain load of a plain number and never a
 * dereference of anything.
 */
int pool_worker_station(pool_t *p, int worker)
{
    if (worker < 0 || worker >= p->n_workers)
        return -1;
    return atomic_load_explicit(&p->epochs[worker].station,
                                memory_order_relaxed);
}
/* }}} */

/* {{{ decide_worker_count() */
/*
 * Three paths, most explicit first: a positive argument is obeyed;
 * the SORAMECH_WORKERS environment variable is next, because tests
 * want one worker and race hunts want two; failing both, one worker
 * per online processor.
 */
static int decide_worker_count(int n_workers)
{
    if (n_workers > 0)
        return n_workers;

    const char *env = getenv("SORAMECH_WORKERS");
    if (env && *env) {
        int n = atoi(env);
        if (n <= 0) {
            fprintf(stderr, "pool: SORAMECH_WORKERS is '%s', not a positive number\n", env);
            abort();
        }
        return n;
    }

    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online < 1) {
        fprintf(stderr, "pool: could not count online processors\n");
        abort();
    }
    return (int)online;
}
/* }}} */

/* {{{ pool_create() */
pool_t *pool_create(int n_workers, pool_finish_t finish, void *finish_ctx)
{
    pool_t *p = calloc(1, sizeof *p);
    if (!p) {
        fprintf(stderr, "pool: allocation failed\n");
        abort();
    }

    p->capacity = POOL_INITIAL_CAPACITY;
    p->slots = malloc((size_t)p->capacity * sizeof *p->slots);
    if (!p->slots) {
        fprintf(stderr, "pool: queue allocation failed\n");
        abort();
    }

    pthread_mutex_init(&p->mutex, NULL);
    pthread_cond_init(&p->wake, NULL);
    pthread_cond_init(&p->start_gate, NULL);

    p->finish = finish;
    p->finish_ctx = finish_ctx;

    p->n_workers = decide_worker_count(n_workers);
    /* Sized from the count the pool actually settled on, not from
     * what the caller asked for — zero means "decide for me", and an
     * array sized from the request would be one slot while N workers
     * wrote into it. */
    p->epochs = calloc((size_t)p->n_workers, sizeof *p->epochs);
    if (!p->epochs) {
        fprintf(stderr, "pool: out of memory for the worker epochs\n");
        exit(71);
    }
    /* A worker inside no station says so. Zeroed memory would claim
     * every worker is inside station zero, which is a real station
     * and therefore a lie rather than an absence (issue 106). */
    for (int i = 0; i < p->n_workers; i++)
        atomic_store_explicit(&p->epochs[i].station, -1,
                              memory_order_relaxed);

    p->workers = calloc((size_t)p->n_workers, sizeof *p->workers);
    if (!p->workers) {
        fprintf(stderr, "pool: worker table allocation failed\n");
        abort();
    }

    for (int i = 0; i < p->n_workers; i++) {
        p->workers[i].index = i;
        p->workers[i].pool = p;
        int err = pthread_create(&p->workers[i].thread, NULL,
                                 worker_main, &p->workers[i]);
        if (err != 0) {
            fprintf(stderr, "pool: creating worker %d failed: %s\n",
                    i, strerror(err));
            abort();
        }
    }

    return p;
}
/* }}} */

/* {{{ pool_release() */
void pool_release(pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    p->released = 1;
    pthread_cond_broadcast(&p->start_gate);
    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* {{{ pool_join() */
void pool_join(pool_t *p)
{
    if (p->joined)
        return;
    p->joined = 1;
    for (int i = 0; i < p->n_workers; i++) {
        int err = pthread_join(p->workers[i].thread, NULL);
        if (err != 0) {
            fprintf(stderr, "pool: joining worker %d failed: %s\n",
                    i, strerror(err));
            abort();
        }
    }
}
/* }}} */

/* {{{ pool_submitter_register() / pool_submitter_unregister() */
void pool_submitter_register(pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    p->outside++;
    pthread_mutex_unlock(&p->mutex);
}

void pool_submitter_unregister(pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    p->outside--;
    if (p->outside < 0) {
        fprintf(stderr, "pool: submitter unregistered more times than registered\n");
        abort();
    }
    /* If this was the last outside submitter and every worker is
     * already asleep over an empty queue, nobody is left to notice
     * that termination has become decidable — so nudge them. One
     * woken worker re-runs the empty case, becomes the last sleeper,
     * and decides. */
    if (p->outside == 0)
        pthread_cond_broadcast(&p->wake);
    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* {{{ pool_destroy() */
void pool_destroy(pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    int started = p->released;
    if (!started) {
        /* A pool destroyed before release: stop the parked workers
         * where they stand. A released pool instead terminates
         * itself, and the join below collects it. */
        p->stop = 1;
        pthread_cond_broadcast(&p->start_gate);
        pthread_cond_broadcast(&p->wake);
    }
    pthread_mutex_unlock(&p->mutex);

    pool_join(p);

    /* Any tasks still in the ring were never run; freeing them here
     * would guess at their ownership, and a stopped-early pool with
     * work left is a caller mistake worth hearing about. */
    if (p->head != p->tail)
        fprintf(stderr, "pool: destroyed with %d tasks never run\n", queue_count(p));

    pthread_mutex_destroy(&p->mutex);
    pthread_cond_destroy(&p->wake);
    pthread_cond_destroy(&p->start_gate);
    free(p->workers);
    free(p->epochs);
    free(p->slots);
    free(p);
}
/* }}} */

/* {{{ pool_worker_count() */
int pool_worker_count(pool_t *p)
{
    return p->n_workers;
}
/* }}} */

/* {{{ pool_queue_stats() */
void pool_queue_stats(pool_t *p, int *capacity, int *high_water, int *growths)
{
    pthread_mutex_lock(&p->mutex);
    if (capacity)   *capacity = p->capacity;
    if (high_water) *high_water = p->high_water;
    if (growths)    *growths = p->growths;
    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* 012's private macros end with 012. */
#undef POOL_INITIAL_CAPACITY

/* ==================================================================
 *
 * 019 — the station table
 *
 * Was src/019-station.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/019-station.c"
/*
 * 019-station.c — building and dismantling the station table.
 *
 * What this is: the structural half of the engine. It allocates the
 * table of stations, hangs input and output ports off them, and tears
 * it all down. Nothing in this file moves a value; motion lives in
 * the delivery file. Data structures here, dataflow there — an error
 * in one is then findable without reading the other.
 *
 * How it does it, in general terms: one allocation for the table,
 * one per station's port array, one per ring buffer, ports and
 * destinations as small linked nodes created as wires are declared.
 * Every cross-reference is an index, so nothing here ever needs
 * fixing up when storage grows elsewhere.
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Ring buffers start small on purpose: growth is cheap, proven, and
 * worth seeing in the demo; a generous initial size would only hide
 * the mechanism. The depth itself is IN_PORT_DEFAULT_CAPACITY, declared
 * beside the record in the header because the port's contract is
 * where a reader looks for it — and because issue 210b gave a port a
 * way to ask for a different one, which means two places now have to
 * agree on what "unless somebody says otherwise" is.
 */

/* {{{ fail() */
/*
 * Construction errors are author errors: the map being described is
 * wrong, and building on top of a wrong description helps nobody.
 * Say what, where, and stop.
 */
static void fail(const char *what)
{
    /*
     * **An invalid operation ends the program** (issue 106), with an
     * exit code that says which kind of fault it was: one a caller
     * can correct and retry. It used to abort, which left a core and
     * told a shell script nothing.
     *
     * There is no map to hand over here, because this helper is
     * reached from places that hold one and places that do not, and a
     * report about the wrong program is worse than no report. What
     * dies with a report is the path that knows which program it was
     * editing.
     */
    char said[512];
    snprintf(said, sizeof said, "map construction: %s", what);
    sora_stop_now(NULL, SORA_EXIT_BAD_CALL, said);
}

/* {{{ fail_resource() */
/*
 * The other kind, and the distinction is the point (issue 106): out
 * of memory is not something a caller can correct and retry, so
 * anything that retries on failure has to be able to tell the two
 * apart in code rather than in prose.
 */
static void fail_resource(const char *what)
{
    char said[512];
    snprintf(said, sizeof said, "map construction: %s", what);
    sora_stop_now(NULL, SORA_EXIT_NO_RESOURCE, said);
}
/* }}} */
/* }}} */

/* {{{ slot_stride() */
/*
 * How many bytes one slot occupies: its value, then its state, then
 * enough padding that the next slot's value is aligned too.
 *
 * The alignment is inferred rather than known, and the inference is
 * the only subtle line in this file. The placement functions carry every
 * type's *size* and no type's *alignment* — nothing has needed the
 * latter before, because a plain array of values strided by their own
 * size is aligned for free. Adding a state byte per slot breaks that
 * for the first time.
 *
 * What rescues it is a rule the C standard guarantees: a type's
 * alignment always divides its size. So the largest power of two
 * dividing elem_size is *at least* the alignment the type wants, and
 * rounding the stride up to it is safe without knowing what the type
 * actually is. Sixteen is the ceiling because no ordinary C type
 * needs more than max_align_t, and rounding past it would only waste
 * memory.
 *
 * The cost is worth stating plainly. A port of four-byte integers
 * goes from four bytes per slot to eight — the state needs a byte and
 * the alignment rounds it to four. A port of two-hundred-byte structs
 * goes from two hundred to two hundred and eight. So the overhead is
 * large in proportion exactly where it is small in absolute terms,
 * and negligible where the values are big, which is the case this
 * whole line of work is about.
 */
static int slot_stride(int elem_size)
{
    int align = 1;
    while (align < 16 && elem_size % (align * 2) == 0)
        align *= 2;
    int total = elem_size + (int)sizeof(_Atomic unsigned char);
    return (total + align - 1) / align * align;
}
/* }}} */

/* {{{ in_port_page_at() */
/*
 * The page holding a given page number, counted from the first.
 *
 * A walk down a short list, and the cost paging charges (issue 210e).
 * It is bounded by how many pages a port has, which is one until a
 * consumer falls behind its producer — and a port deep enough for
 * this walk to matter is one phase 7's buffer report is already
 * shouting about, so the long chain is a symptom of a problem the
 * engine is supposed to be complaining about rather than absorbing.
 *
 * The scan does not use this per candidate. It resolves a page once
 * and then follows `next`, which is what keeps a sweep to one pointer
 * hop per page boundary instead of a walk per slot.
 */
static in_port_page_t *in_port_page_at(const in_port_t *sl, int page)
{
    in_port_page_t *pg = sl->pages;
    while (page-- > 0 && pg)
        pg = atomic_load_explicit(&pg->next, memory_order_acquire);
    return pg;
}
/* }}} */

/* {{{ in_port_slot() */
static void *in_port_slot(const in_port_t *sl, int index)
{
    in_port_page_t *pg = in_port_page_at(sl, index / sl->page_slots);
    if (!pg) {
        /* An ordinal past the last page means the caller computed a
         * position the port does not have. Nothing should be able to:
         * the scan bounds itself by the capacity, and the capacity is
         * the sum of the pages. Stopping is right because continuing
         * would read whatever follows the list. */
        fprintf(stderr, "station: slot %d asked for on a port that has "
                        "%d\n", index, sl->capacity);
        abort();
    }
    return pg->slots + (size_t)(index % sl->page_slots) * (size_t)sl->stride;
}
/* }}} */

/* {{{ in_port_add_page() */
/*
 * One more page on the end. Used both to give a port its first page
 * and to grow it, because they are the same act (issue 210e) — which
 * is the shape the station table already uses one level up.
 */
static in_port_page_t *in_port_add_page(in_port_t *sl)
{
    /* Zeroed rather than merely allocated, because a slot's state is
     * part of it and empty is zero (issue 210c) — a fresh page has to
     * be a page of *empty* slots, or the first reader to reach it
     * would find whatever the allocator left behind and believe it. */
    in_port_page_t *pg = calloc(1, sizeof *pg
                                + (size_t)sl->page_slots * (size_t)sl->stride);
    if (!pg) fail_resource("out of memory for a page of a ring buffer");

    if (!sl->pages) {
        sl->pages = pg;
    } else {
        in_port_page_t *last = sl->pages;
        in_port_page_t *next;
        while ((next = atomic_load_explicit(&last->next,
                                            memory_order_relaxed)) != NULL)
            last = next;
        /* Release, so a scanner that follows this link sees a page of
         * fully-zeroed slots rather than whatever calloc had not yet
         * made visible. Only growth ever writes a link, and growth
         * holds the station's mutex, so this is the one writer. */
        atomic_store_explicit(&last->next, pg, memory_order_release);
    }
    /* **After** the page is linked, never before.** Seeing the larger
     * capacity is what tells a scanner the slots exist; publishing the
     * number first would invite a sweep into slots that are not
     * reachable yet. */
    atomic_fetch_add_explicit(&sl->capacity, sl->page_slots,
                              memory_order_release);
    return pg;
}
/* }}} */

/* {{{ in_port_free_pages() */
static void in_port_free_pages(in_port_t *sl)
{
    in_port_page_t *pg = sl->pages;
    while (pg) {
        in_port_page_t *next = atomic_load_explicit(&pg->next,
                                                    memory_order_relaxed);
        free(pg);
        pg = next;
    }
    sl->pages = NULL;
    atomic_store_explicit(&sl->capacity, 0, memory_order_relaxed);
}
/* }}} */

/* {{{ slot_move_at() */
/*
 * The state machine, on a slot the caller has already located.
 *
 * Split out from the index-taking form because the scan walks pages
 * and therefore already holds the address (issue 210e). Going back
 * through an ordinal would make it resolve a page per candidate,
 * turning a sweep into a walk down the page list for every slot it
 * looks at — quadratic in the number of pages, on the hot path,
 * to recompute something it just had.
 */
static int slot_move_at(void *slot, int elem_size, int from, int to)
{
    /* The state sits immediately after the value bytes. Reached
     * through a byte pointer and an explicit offset rather than a
     * struct member, because a slot's size is not known until the
     * port exists — the value in the middle of it is as wide as the
     * parameter this port feeds. */
    _Atomic unsigned char *state = (_Atomic unsigned char *)
        ((unsigned char *)slot + elem_size);

    unsigned char expected = (unsigned char)from;
    /* Acquire-release on success: a reader that wins ready-to-claimed
     * must see every byte the writer copied before it published, and
     * a writer that wins claimed-to-empty must not have its next copy
     * hoisted above the release. Acquire on failure, because a caller
     * that lost still read the state and will decide what to do from
     * it. */
    return atomic_compare_exchange_strong_explicit(
        state, &expected, (unsigned char)to,
        memory_order_acq_rel, memory_order_acquire);
}
/* }}} */

/* {{{ slot_state_at() / slot_take_at() */
/*
 * The claim side's transition, without a compare-and-swap (issue
 * 210d, step 6).
 *
 * **A ready slot has exactly one possible mover, and under the
 * station's mutex that mover is us.** Other claimers are excluded by
 * the lock. A writer never touches a ready slot: it moves a slot
 * empty → reserved → ready, and its search for an empty one
 * compare-and-swaps from *empty*, which simply fails against a ready
 * slot without writing anything. So nothing can change this byte
 * between reading it and writing it, and a read-modify-write would be
 * paying for an exclusion that the lock has already bought.
 *
 * That matters because the *search* asks this question of every
 * candidate it walks past. A compare-and-swap per candidate is what
 * made small values measurably slower when the scan replaced index
 * arithmetic; a load per candidate does not.
 *
 * The acquire is still needed and is the whole reason these are not
 * plain memory accesses: it is what makes the writer's copied bytes
 * visible to the worker that takes the slot from ready.
 */
static int slot_state_at(const void *slot, int elem_size)
{
    const _Atomic unsigned char *state = (const _Atomic unsigned char *)
        ((const unsigned char *)slot + elem_size);
    return atomic_load_explicit(state, memory_order_acquire);
}

static void slot_set_at(void *slot, int elem_size, int to)
{
    _Atomic unsigned char *state = (_Atomic unsigned char *)
        ((unsigned char *)slot + elem_size);
    /* Release, so that whoever next acquires this slot sees everything
     * this thread did to its bytes beforehand. */
    atomic_store_explicit(state, (unsigned char)to, memory_order_release);
}
/* }}} */

/* {{{ in_port_slot_move() */
/*
 * The same transition, named by ordinal rather than by address, for
 * every caller that has an index in hand and no page to walk from.
 */
CERA_TEST_ONLY static int in_port_slot_move(const in_port_t *sl, int index, int from, int to)
{
    return slot_move_at(in_port_slot(sl, index), sl->elem_size, from, to);
}
/* }}} */

/* {{{ static int add_shelf() */
/*
 * One more shelf, and its pointer written into the short array that
 * names them. That array holds addresses rather than mutexes, so
 * growing it by reallocation is safe — the same kind of copy the
 * pool's ring already does. No station record is ever copied.
 */
static int add_shelf(map_t *m)
{
    station_t *shelf = calloc(STATIONS_PER_SHELF, sizeof *shelf);
    if (!shelf)
        return -1;
    station_t **shelves = realloc(m->shelves,
                                  (size_t)(m->n_shelves + 1) * sizeof *shelves);
    if (!shelves) {
        free(shelf);
        return -1;
    }
    shelves[m->n_shelves] = shelf;
    m->shelves = shelves;
    m->n_shelves++;
    return 0;
}
/* }}} */

/* {{{ map_create() */
map_t *map_create_empty(void)
{
    map_t *m = calloc(1, sizeof *m);
    if (!m) fail_resource("out of memory for the map");
    pthread_mutex_init(&m->rewire_mutex, NULL);
    pthread_mutex_init(&m->scrap_mutex, NULL);
    return m;
}
/* }}} */

/* {{{ map_create() */
map_t *map_create(int n_stations)
{
    if (n_stations <= 0)
        fail("a map needs at least one station");

    map_t *m = calloc(1, sizeof *m);
    if (!m) fail_resource("out of memory for the map");

    pthread_mutex_init(&m->rewire_mutex, NULL);
    pthread_mutex_init(&m->scrap_mutex, NULL);

    /*
     * Shelves enough for what was asked for (issue 211). Asking for a
     * count up front is now a convenience rather than a commitment:
     * the table grows a shelf at a time afterwards, and nothing
     * already placed ever moves.
     *
     * Reserved directly rather than by calling map_add_station in a
     * loop, because that call hands back the first place nobody has
     * filled — which is the same place every time until somebody
     * fills it. Reserving N places and filling them is a different
     * act from asking for somewhere to put one thing.
     */
    while (n_stations > m->n_shelves * STATIONS_PER_SHELF)
        if (add_shelf(m) < 0)
            fail_resource("out of memory for the station table");
    atomic_store_explicit(&m->n_stations, n_stations, memory_order_release);

    return m;
}
/* }}} */

/* {{{ map_add_station() */
int map_add_station(map_t *m)
{
    /* Exclusive, under the lock every other structural change already
     * takes (issue 704). Two threads each finding the same free place,
     * or each deciding the shelves are full, would otherwise hand two
     * callers one index. The hand-raising ring the note asked for is
     * not built and is moot: adding a shelf is one allocation and one
     * pointer write, so there is no long stretch for anybody to raise a
     * hand during. `strategems/raise-your-hand.md` keeps the pattern
     * and the lesson that displaced it. */
    pthread_mutex_lock(&m->rewire_mutex);

    /*
     * A freed place first. Removing a station is what removes the
     * wires to it (issue 216), so a place whose shim is clear holds
     * nothing stale and can simply be taken. A program that adds and
     * removes forever therefore reaches a steady size rather than
     * climbing.
     */
    int count = atomic_load_explicit(&m->n_stations, memory_order_acquire);
    for (int i = 0; i < count; i++) {
        station_t *s = map_station(m, i);
        if (!s->call && !atomic_load_explicit(&s->removed,
                                              memory_order_acquire)) {
            pthread_mutex_unlock(&m->rewire_mutex);
            return i;
        }
    }

    if (count >= m->n_shelves * STATIONS_PER_SHELF && add_shelf(m) < 0) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return -1;
    }

    /*
     * Published last, and by one write. Everything about the record is
     * already zeroed by the shelf's own allocation, so a reader that
     * sees this count sees a station that is complete — an empty one,
     * which is exactly what a station is before anything is placed in
     * it.
     */
    atomic_store_explicit(&m->n_stations, count + 1, memory_order_release);
    pthread_mutex_unlock(&m->rewire_mutex);
    return count;
}
/* }}} */

/* {{{ map_place() */
void map_place(map_t *m, int station, task_call_t shim, int kind,
               int n_in_ports, const int *elem_sizes, int out_size)
{
    if (station < 0 || station >= m->n_stations)
        fail("placing a box at a station index outside the table");
    if (kind < 0 || kind >= STATION_KIND_COUNT)
        fail("placing a box of a kind that does not exist");
    if (n_in_ports < 0)
        fail("a station cannot have a negative number of slots");

    station_t *s = map_station(m, station);
    if (s->call)
        fail("placing a box at a station already occupied");

    pthread_mutex_init(&s->mutex, NULL);
    s->call = shim;
    s->kind = (unsigned char)kind;
    s->out_size = out_size;
    s->cursor = 0;

    s->n_in_ports = n_in_ports;
    s->in_ports = NULL;
    if (n_in_ports > 0) {
        s->in_ports = calloc((size_t)n_in_ports, sizeof *s->in_ports);
        if (!s->in_ports) fail_resource("out of memory for a port array");
    }

    for (int i = 0; i < n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        if (elem_sizes[i] <= 0)
            fail("a port's element size must be positive");
        /* Every port starts life as a ring buffer — the default the
         * map format also assumes (issue 601). Becoming a static, or
         * having its source taken away, is a conversion applied
         * afterwards; neither one frees what is allocated here
         * (issue 210b). */
        sl->kind = IN_PORT_RING;
        sl->elem_size = elem_sizes[i];
        sl->stride = slot_stride(sl->elem_size);
        /* The first page, which is the same act as growing (issue
         * 210e): a port with one page and a port with nine differ
         * only in how many times this has happened. */
        sl->pages = NULL;
        sl->capacity = 0;
        sl->page_slots = IN_PORT_DEFAULT_CAPACITY;
        in_port_add_page(sl);
        sl->read_hint = 0;
        sl->write_hint = 0;
        sl->held = 0;

        /* The other storage, allocated at the same moment and for the
         * same reason (issues 210b, 401): a port has room for both a
         * buffer and a constant whatever it is currently for, so
         * changing which one is in effect is a field write and never
         * an allocation. Zeroed, so a port whose constant has not been
         * set holds zeroes rather than whatever was there — though
         * nothing reads it until constant_set says somebody wrote it. */
        sl->constant = calloc(1, (size_t)sl->elem_size);
        if (!sl->constant) fail_resource("out of memory for a port's constant");
        sl->constant_string = NULL;
        sl->constant_set = 0;
    }
}
/* }}} */

/* {{{ static void station_label_into() */
/*
 * The name a map file gave a station, or its index when nothing gave
 * it one. A program built by calling the surface has no names, and a
 * complaint that says "?" about it is one nobody can act on.
 */
static void station_label_into(map_t *m, int i, char *out, size_t room)
{
    if (m->station_names && i < m->n_named && m->station_names[i])
        snprintf(out, room, "%s", m->station_names[i]);
    else
        snprintf(out, room, "%d", i);
}
/* }}} */

/* {{{ static void no_such_port_into() */
/*
 * **One sentence for "that port does not exist", written once**
 * (issue 210g).
 *
 * The loader used to compose its own, and it was the better of the
 * two: it named the box, counted its ports, and remembered that a
 * comparator carries one more than its parameter list shows. The
 * surface's said only which numbers disagreed. Two spellings of one
 * refusal is the small version of the same fault two construction
 * paths were — so the good one moved down here, where every caller
 * reaches it, and the loader kept nothing of its own but the file and
 * the line it prefixes this with.
 *
 * The threshold clause is why this cannot be a format string at each
 * call site: a comparator's last port is not one of the box's
 * parameters, so a reader counting parameters in the box source finds
 * one fewer than the refusal names and concludes the engine is
 * confused.
 */
static void no_such_port_into(map_t *m, int station, int port,
                              char *out, size_t room)
{
    station_t *s = map_station(m, station);
    char who[64];
    station_label_into(m, station, who, sizeof who);
    snprintf(out, room,
             "station '%s' has no port %d — '%s' has %d port%s (its "
             "parameters%s)",
             who, port, s->box_name ? s->box_name : "?", s->n_in_ports,
             s->n_in_ports == 1 ? "" : "s",
             s->kind == STATION_COMPARATOR ? ", plus the threshold" : "");
}
/* }}} */

/* {{{ map_in_port_start_depth() */
/*
 * **A refusal travels rather than stopping here** (issue 210g).
 *
 * This used to die on the spot, which was the last port operation
 * that did. A caller reading a file collects every mistake in it and
 * presents them together, and it cannot collect what killed the
 * process — so a depth on a port that does not exist would have been
 * the one fault in a map file that hid every fault after it.
 *
 * Existing callers that pass sound arguments are unaffected: a
 * function that returned nothing now returns nothing they have to
 * look at, and the only difference is that the ones who *do* look can
 * say where the trouble was.
 */
const char *map_in_port_start_depth(map_t *m, int station, int port, int slots)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said,
                 "station %d is outside the table", station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        no_such_port_into(m, station, port, said, sizeof said);
        return said;
    }
    /* One slot is a legitimate depth. It used to take two, because a
     * spare was held back so that head meeting tail could mean empty
     * rather than full; a slot that carries its own state needs no
     * such stand-in, and every slot is usable (issue 210c). */
    if (slots < 1) {
        snprintf(said, sizeof said,
                 "a ring buffer needs at least one slot, and %d was asked for",
                 slots);
        return said;
    }

    in_port_t *sl = &s->in_ports[port];
    if (sl->held != 0) {
        char who[64];
        station_label_into(m, station, who, sizeof who);
        snprintf(said, sizeof said,
                 "%s.%d already holds values — this is a starting depth, and "
                 "the start has been and gone", who, port);
        return said;
    }

    /* The starting depth sets the **page size**, not merely the first
     * page's size (issue 210e). Every page a port ever adds is this
     * big, so asking for deep buffers gets large pages everywhere
     * rather than a long chain of small ones — the same lever pointed
     * at the same problem, which is what keeps the page walk short
     * for a program that knew it would need depth.
     *
     * The port is empty, which the check above proved, so there is
     * nothing to carry across: drop the pages it has and give it one
     * of the new size. */
    in_port_free_pages(sl);
    sl->page_slots = slots;
    in_port_add_page(sl);
    sl->read_hint = 0;
    sl->write_hint = 0;
    return NULL;
}
/* }}} */

/* {{{ map_in_port_convert() */
void map_in_port_convert(map_t *m, int station, int port, int kind)
{
    /* A case of the one configuration operation (issue 210g), kept as
     * a name because "convert this port" is what callers already say.
     * Passing no text means *the value it had before*, which is why
     * becoming a static again works and becoming one for the first
     * time is refused here. */
    const char *no = map_configure_port(m, station, port, kind, NULL);
    if (no)
        fail(no);
}
/* }}} */

/* {{{ map_configure_port() */
/*
 * **The one operation that says where a port's values come from**
 * (issue 210g): a station, a port, a source, and — when the source is
 * a value — the value itself, written as text.
 *
 * Binding a constant, taking a source away, and giving a port back to
 * the arrows were three calls with three shapes, some dying on
 * refusal and some returning a code, and the difference between them
 * was history rather than meaning. They are one thing now and the
 * other two are cases of it. What that buys is not tidiness: it is
 * that there is one description of what it means to give a port a
 * source, and it is executable — the loader calls it while reading a
 * file, and a debugger or a workbench calls the same one on a running
 * program.
 *
 * **Text distinguishes the two ways to become a constant.** Given
 * text, the port takes that value. Given none, it goes back to the
 * value it held before, which is legitimate because a constant
 * survives being converted away exactly as waiting values do (issue
 * 210f) — and which is refused when there is no such value, because
 * the tag would then be in effect over storage nobody ever wrote.
 *
 * **Returns a refusal rather than stopping the program**, so that a
 * caller reading a file can collect every mistake in it and present
 * them together instead of one per run. That is the policy issue 212
 * settles and this is the surface it applies to.
 *
 * One thing still stops the program: text that does not parse. The
 * reader dies where the malformed value is, naming the station, the
 * port and the field, and moving that onto this return path belongs
 * with the rest of the refusal policy rather than being half done
 * here.
 */
const char *map_configure_port(map_t *m, int station, int port,
                               int source, const char *text)
{
    /* Per thread, because two threads may be editing two different
     * maps and a shared buffer would let one overwrite the other's
     * complaint. Valid until this thread's next refusal. */
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said,
                 "station %d is outside the table", station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        /* The loader's wording, which named the box and remembered
         * the comparator's threshold, moved down here so that every
         * caller gets it and nobody keeps a second copy (issue 210g). */
        no_such_port_into(m, station, port, said, sizeof said);
        return said;
    }
    if (source < 0 || source >= IN_PORT_KIND_COUNT) {
        snprintf(said, sizeof said,
                 "there is no such source for a port");
        return said;
    }

    if (source == IN_PORT_STATIC) {
        if (text) {
            /* Binding parses the text and sets the tag together, so a
             * value that will not parse never leaves the port in a
             * state that claims to hold one. */
            map_in_port_static_text(m, station, port, text);
            return NULL;
        }
        if (!s->in_ports[port].constant_set) {
            snprintf(said, sizeof said,
                     "station %d port %d has never held a value, so there is "
                     "none to go back to — give it one as text",
                     station, port);
            return said;
        }
    }

    /* Under the station's mutex, as one of the rare structural
     * operations, so no readiness walk sees a port mid-change.
     * Nothing is freed and nothing is cleared: the whole of the change
     * is the tag, which is what makes it cheap and lossless. */
    pthread_mutex_lock(&s->mutex);
    s->in_ports[port].kind = (unsigned char)source;
    pthread_mutex_unlock(&s->mutex);
    return NULL;
}
/* }}} */

/* {{{ map_check_sources() */
/*
 * **Every parameter needs somewhere to get a value** (issue 210g).
 *
 * A port with no source is not an error while a program is being
 * assembled — it is the ordinary state of a station that exists
 * before anybody has finished wiring it, and being able to exist that
 * way is what lets a program be built a piece at a time. It becomes
 * an error at the moment somebody says the program is finished.
 *
 * Caught here rather than at the first task, because here it can name
 * the station and the port while the person who mis-wired them is
 * still looking. A station whose port has no source simply never
 * becomes ready, which is correct behaviour and a terrible way to
 * find out: the symptom is a program that runs and quietly does less
 * than it was asked to.
 *
 * **Every one of them, collected**, rather than the first — somebody
 * fixing a new program wants the whole list, not one per run. The
 * count is reported even when the list is trimmed, so a long one
 * never reads as a short one.
 *
 * **The check has no exceptions and never will.** A parameter a box
 * could do without was proposed and refused (issue 210h), because it
 * would have been the only exemption to the rule that a station runs
 * when every one of its slots holds a value. So this is unqualified:
 * a port with no source is an error, full stop.
 */
const char *map_check_sources(map_t *m)
{
    static _Thread_local char said[512];
    int used = 0, found = 0;

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        /* An empty place in the table is not a station (issue 216). */
        if (!s->call)
            continue;
        for (int j = 0; j < s->n_in_ports; j++) {
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) != IN_PORT_NONE)
                continue;
            found++;
            if (used < (int)sizeof said - 64) {
                /* The name a map file gave it, or its index when
                 * nothing gave it one — a program built by calling
                 * this surface has no names, and a complaint that
                 * says "?" about it is a complaint nobody can act
                 * on. Both spellings read the same way: which
                 * station, then which port.
                 *
                 * Through the shared speller, which is the third
                 * copy of these four lines being retired. The copies
                 * had drifted in a way that mattered: this one read
                 * the names array by station index without asking
                 * how long it is, and stations added after the last
                 * naming call leave it shorter than the table. */
                char who[64];
                station_label_into(m, i, who, sizeof who);
                used += snprintf(said + used, sizeof said - (size_t)used,
                                 "%s%s.%d has no source",
                                 used ? "; " : "", who, j);
            }
        }
    }

    if (!found)
        return NULL;
    if (found > 1 && used < (int)sizeof said - 32)
        snprintf(said + used, sizeof said - (size_t)used,
                 " (%d ports in all)", found);
    return said;
}
/* }}} */

/* {{{ map_name_station() */
/*
 * **What to call a station** (issue 212), which is the sixth thing
 * construction has to be able to say.
 *
 * The engine never reads these — every wire is an index, and that is
 * deliberate. They exist so a program can be *written back out* as a
 * file that reads in again, and so a person watching a live view sees
 * something other than numbers. A program with no names still runs
 * perfectly; it simply cannot be described on disk.
 *
 * It had to become an operation rather than staying a thing only the
 * loader did. The loader used to copy every name onto the map in one
 * go once its own lookup table had served, which works exactly while
 * the only way to build a program is to read a file. A program built
 * by calling this surface would otherwise dump as a row of indices —
 * and then a file-built program and a surface-built one could not be
 * compared, which is the proof that there is one construction path
 * rather than two that agree by coincidence.
 *
 * The array grows with the table, because stations are added one at a
 * time now rather than counted in advance.
 */
const char *map_name_station(map_t *m, int station, const char *name)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    if (!name || !*name)
        return "a station's name cannot be empty";

    if (m->n_named < m->n_stations) {
        char **grown = realloc(m->station_names,
                               (size_t)m->n_stations * sizeof *grown);
        if (!grown)
            return "out of memory naming a station";
        for (int i = m->n_named; i < m->n_stations; i++)
            grown[i] = NULL;
        m->station_names = grown;
        m->n_named = m->n_stations;
    }

    free(m->station_names[station]);
    m->station_names[station] = strdup(name);
    if (!m->station_names[station])
        return "out of memory naming a station";
    return NULL;
}
/* }}} */

/* {{{ map_station_set_cursor() — issue 712 */
/*
 * **Put an iterator back where it had got to.**
 *
 * An iterator takes its exits in turn, and which one is next is the
 * one memory a station keeps. A program written down mid-run and
 * revived with its iterators reset would send the next value to an
 * exit it was never going to — the shape would be right and the
 * behaviour wrong, which is the worst kind of wrong for a capture to
 * be.
 *
 * Refused on anything else, because there is nothing for it to mean:
 * a plain station has one exit and a comparator chooses by comparing,
 * so neither has a position to be in.
 */
const char *map_station_set_cursor(map_t *m, int station, int at)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (s->kind != STATION_ITERATOR) {
        snprintf(said, sizeof said,
                 "station %d is not an iterator, so it has no exit it is "
                 "pointing at", station);
        return said;
    }
    if (at < 0 || (s->n_out_ports > 0 && at >= s->n_out_ports)) {
        snprintf(said, sizeof said,
                 "station %d has %d exits, so it cannot be pointing at "
                 "number %d", station, s->n_out_ports, at);
        return said;
    }
    s->cursor = at;
    return NULL;
}
/* }}} */

/* {{{ map_designate_output() */
/*
 * **Say that this station is a place the program's results come
 * from** (issue 209).
 *
 * It stays an ordinary station: same shape, same readiness, running
 * whatever box it was placed with or none. The designation adds
 * exactly one rule — when its output port is wired nowhere, values
 * are held rather than discarded — and one meaning, which is that a
 * parent composing this program has somewhere to wire from and a
 * person reading it can tell which station is the point.
 *
 * A program may have **several**, each with its own input ports and
 * its one output port, because a box returns one value and so a
 * station has one output port and so a program output is one station.
 * The alternative — one station whose output ports each owned a
 * subset of its inputs — needs a box returning several values, which
 * C does not have, and would be the only thing in the engine with
 * several readiness checks over subsets of its ports.
 */
const char *map_designate_output(map_t *m, int station)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (!s->call) {
        snprintf(said, sizeof said,
                 "station %d has no box placed — place, then designate",
                 station);
        return said;
    }
    if (s->out_size == 0) {
        /* A station whose box returns nothing has no output port, so
         * there is nothing for a parent to wire from and nothing to
         * hold. Refused rather than accepted-and-useless, because the
         * mistake is almost certainly the wrong station. */
        snprintf(said, sizeof said,
                 "station %d returns nothing, so it has no results to be "
                 "the source of", station);
        return said;
    }
    s->door = DOOR_OUT;
    return NULL;
}
/* }}} */

/* {{{ map_start_beside() */
map_t *map_start_beside(map_t *parent)
{
    if (!parent->pool)
        fail("starting a program beside one that has not started itself");

    map_t *m = map_create_empty();
    /*
     * The same workers, and nothing else shared. A task now says
     * which program it belongs to, so a worker finishing one does not
     * need to know whose pool it is running on — which is what makes
     * this a field assignment rather than a mechanism.
     */
    m->pool = parent->pool;
    m->pool_is_borrowed = 1;
    return m;
}
/* }}} */

/* {{{ map_designate_input() */
/*
 * **Say that this station is where the outside delivers** (issue
 * 213), which is the other door and the same design.
 *
 * Without it, a value gets into a running program exactly one way:
 * somebody holding the program calls the delivery entry naming a
 * station and a port. That works and it is what every test does, and
 * it means **the caller has to know the program's insides**. Rename
 * an interior station and every caller breaks. That is not
 * encapsulation — the program has no surface, only internals that
 * happen to be reachable.
 *
 * The mark says which ports the outside is allowed to deliver to.
 * Everything after that is an ordinary delivery down an ordinary
 * wire, which is why this needs no new mechanism in the delivery
 * path at all.
 *
 * **One output port, so one station per argument group.** A box
 * returns one value, so a station has one output port, so a program
 * taking several unrelated arguments has several input stations. The
 * alternative wants a C function returning several values, and faking
 * it with a struct something downstream takes apart means a function
 * written to satisfy the engine — which is the thing this design will
 * not ask anybody for.
 *
 * Fan-out is a different thing and was always free: one input
 * station's output port may feed as many interior stations as it is
 * wired to.
 */
const char *map_designate_input(map_t *m, int station)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (!s->call) {
        snprintf(said, sizeof said,
                 "station %d has no box placed — place, then designate",
                 station);
        return said;
    }
    if (s->door == DOOR_OUT) {
        /* A program whose entrance is its exit is not a program with
         * two doors; it is somebody having designated the wrong
         * station. Refused rather than quietly overwritten. */
        snprintf(said, sizeof said,
                 "station %d is already where results come from — a station "
                 "cannot be both doors", station);
        return said;
    }
    s->door = DOOR_IN;
    return NULL;
}
/* }}} */

/* {{{ map_deliver_argument() */
/*
 * **Deliver a value from outside the program** (issue 213).
 *
 * The difference between this and the ordinary delivery entry is not
 * mechanical — underneath it is the same call — it is *who may use
 * it*. This one refuses any station that is not a declared door, and
 * that refusal is the whole of what gives a program a surface. A
 * caller reaching an interior station is reaching inside, and the
 * point of the designation is that reaching inside stops being
 * possible by accident.
 *
 * The size is checked against the port, because a caller from outside
 * is exactly the caller least likely to be right about it — inside
 * the graph a wire was checked when it was drawn, and here there is
 * no wire, so this is the only moment.
 */
const char *map_deliver_argument(map_t *m, int station, int port,
                                 const void *value, int size)
{
    static _Thread_local char said[224];

    /*
     * **Shut, because somebody asked this program to wind down**
     * (issue 106). The entrance is the only way anything outside puts
     * work into a program, so refusing here is the whole of what
     * "stop accepting new work" can mean — and it is what lets the
     * queue drain and the ordinary ending fire.
     *
     * Asked first, before the station is even looked at, because a
     * closing program has nothing useful to say about which of its
     * doors somebody was aiming at.
     */
    if (atomic_load_explicit(&m->closing, memory_order_acquire)) {
        snprintf(said, sizeof said,
                 "this program is winding down and is not accepting new "
                 "work");
        return said;
    }

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (s->door != DOOR_IN) {
        snprintf(said, sizeof said,
                 "station %d is not a declared entrance — the outside may "
                 "only deliver to a program's input stations", station);
        return said;
    }
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said,
                 "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    if (size != s->in_ports[port].elem_size) {
        snprintf(said, sizeof said,
                 "that port takes %d bytes and %d were offered",
                 s->in_ports[port].elem_size, size);
        return said;
    }

    map_deliver_value(m, station, port, value);
    return NULL;
}
/* }}} */

/* {{{ map_output_waiting() / map_output_take() */
/*
 * The two halves of collecting a program's results from outside,
 * mirroring the call that writes a constant in (issue 209).
 *
 * **Both, because one is not usable without the other.** A caller
 * asked to drain results needs to know whether there are any, and
 * asking by taking and checking for failure makes "none waiting"
 * indistinguishable from "not an output station" without a second
 * question anyway.
 *
 * Under the station's own mutex, which is the same lock a worker
 * finishing a box takes to put a result there — unlike a slot, which
 * belongs to one worker, a held result belongs to the station until
 * somebody takes it.
 */
int map_output_waiting(map_t *m, int station)
{
    if (station < 0 || station >= m->n_stations)
        return 0;
    station_t *s = map_station(m, station);
    pthread_mutex_lock(&s->mutex);
    int n = s->n_held;
    pthread_mutex_unlock(&s->mutex);
    return n;
}

int map_output_take(map_t *m, int station, void *into, int size)
{
    if (station < 0 || station >= m->n_stations)
        return 0;
    station_t *s = map_station(m, station);
    if (size != s->out_size)
        fail("taking a result into something the wrong size for it");

    pthread_mutex_lock(&s->mutex);
    if (s->n_held == 0) {
        pthread_mutex_unlock(&s->mutex);
        return 0;
    }
    /* Oldest first, and the shuffle is deliberate over the
     * alternative. Results are taken far less often than they are
     * produced, and a caller draining them wants them in the order
     * the program produced them — which is the one ordering this
     * engine can still honestly offer, because a single station
     * produced them all in sequence. */
    memcpy(into, s->held, (size_t)size);
    s->n_held--;
    if (s->n_held > 0)
        memmove(s->held, (unsigned char *)s->held + size,
                (size_t)s->n_held * (size_t)size);
    pthread_mutex_unlock(&s->mutex);
    return 1;
}
/* }}} */

/* {{{ map_bring_up() */
const char *map_bring_up(map_t *m)
{
    static _Thread_local char said[768];
    int used = 0, faults = 0;

    /*
     * **A port with no source is said out loud and is not a fault**,
     * and which of those it is took resolving between two issues that
     * disagreed.
     *
     * Issue 210g asked for it to be a configuration error, caught
     * while somebody is still looking rather than on the first task
     * built minutes into a run. Issue 210b then made a port with no
     * source a state the map file can *spell*, so that a half-built
     * program could be written down and read back — and this issue
     * says plainly that a station may hold such a port indefinitely,
     * because that is what lets a program be assembled from nothing
     * and wired one arrow at a time.
     *
     * The later two win, and they are right: nothing breaks. Such a
     * station simply never becomes ready, which is the same outcome
     * as a buffered input nothing feeds. Refusing it would make
     * "add a station now, wire it in a moment" impossible to express,
     * which is the sequence this whole surface exists to make
     * ordinary.
     *
     * So it is a warning, and a loud one, because a station that
     * silently never runs is the hardest thing to notice from
     * outside.
     */
    const char *unsourced = map_check_sources(m);
    if (unsourced)
        fprintf(stderr, "map: WARNING: %s — %s will not run until %s\n",
                unsourced,
                strchr(unsourced, ';') ? "those stations" : "that station",
                strchr(unsourced, ';') ? "they are finished"
                                       : "it is finished");

    /*
     * **Which ports arrows land on, worked out once for the whole
     * program** (issue 212).
     *
     * This used to be asked per station: for each one, walk every
     * *other* station's destinations looking for arrows that land
     * here. That is the table walked once per station — quadratic in
     * the number of stations, which nobody minded at a dozen of them.
     *
     * Composing is what changes the number. A program brought into
     * another produces one table holding both, and a table that holds
     * several programs' worth of boxes is exactly how a dozen
     * stations stops being a dozen. Turning it round costs one array
     * and answers the same question: walk every destination in the
     * program once, marking where each one lands. The work becomes
     * proportional to the stations plus the wires, which is the size
     * of the thing being described rather than its square.
     *
     * One flat array of flags, indexed by a station's first port plus
     * the port number, so there is one allocation rather than one per
     * station. An empty place contributes no ports and is skipped,
     * which keeps the offsets honest without a special case.
     */
    int *first_port = calloc((size_t)m->n_stations + 1, sizeof *first_port);
    if (!first_port)
        return "out of memory checking a program";
    int total_ports = 0;
    for (int i = 0; i < m->n_stations; i++) {
        first_port[i] = total_ports;
        station_t *s = map_station(m, i);
        total_ports += s->call ? s->n_in_ports : 0;
    }
    first_port[m->n_stations] = total_ports;

    unsigned char *landed = calloc((size_t)(total_ports > 0 ? total_ports : 1),
                                   sizeof *landed);
    if (!landed) {
        free(first_port);
        return "out of memory checking a program";
    }
    for (int k = 0; k < m->n_stations; k++) {
        station_t *other = map_station(m, k);
        for (out_port_t *p = other->out_ports; p; p = p->next) {
            dest_set_t *set = out_port_dests(p);
            for (int di = 0; set && di < set->n; di++) {
                int at = set->items[di].station;
                int port = set->items[di].port;
                if (at < 0 || at >= m->n_stations)
                    continue;
                station_t *dest = map_station(m, at);
                if (!dest->call || port < 0 || port >= dest->n_in_ports)
                    continue;
                landed[first_port[at] + port] = 1;
            }
        }
    }

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;   /* an empty place is not a station (issue 216) */

        char who[64];
        station_label_into(m, i, who, sizeof who);

        const unsigned char *landed_on = landed + first_port[i];

        int has_ring = 0, any_arrow = 0;
        for (int j = 0; j < s->n_in_ports; j++) {
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) == IN_PORT_RING)
                has_ring = 1;
            any_arrow |= landed_on[j];
        }

        /* An arrow landing on a port with no source would have
         * nowhere to put its value at all — nothing to queue into and
         * nothing to overwrite.
         *
         * **A static destination stopped being one of these** (issue
         * 405). A value arriving there overwrites the constant, which
         * is how a constant gets computed at startup rather than
         * written down, and is a property of the wire rather than of
         * the box — so it is visible in the map file instead of
         * happening invisibly inside C. */
        for (int j = 0; j < s->n_in_ports; j++) {
            unsigned char k = atomic_load_explicit(&s->in_ports[j].kind,
                                                   memory_order_relaxed);
            if (landed_on[j] && k != IN_PORT_RING && k != IN_PORT_STATIC) {
                faults++;
                if (used < (int)sizeof said - 128)
                    used += snprintf(said + used, sizeof said - (size_t)used,
                                     "%san arrow lands on %s.%d, but that "
                                     "port is %s, not a buffer",
                                     used ? "; " : "", who, j,
                                     in_port_kind_name(k));
            }
        }

        /* Buffered inputs nothing feeds. Loud but not a fault: a
         * program under construction has these, and so does one fed
         * from outside by a test or a control surface. Silently never
         * running is the hardest thing to notice from outside, which
         * is why it is said at all. */
        /*
         * Buffered inputs nothing feeds. Loud but not a fault: a
         * program under construction has these, and so does one fed
         * from outside. Silently never running is the hardest thing
         * to notice from outside, which is why it is said at all.
         *
         * **A declared entrance is exempt, and that is not a special
         * case being carved out — it is the warning's own escape
         * clause becoming checkable.** The sentence has always ended
         * "unless something outside delivers into it"; a station
         * marked as a door is precisely one that something outside
         * delivers into (issue 213). Warning about it would be
         * telling somebody that the thing they just declared might
         * not happen.
         */
        if (has_ring && !any_arrow && s->door != DOOR_IN)
            fprintf(stderr,
                    "map: WARNING: station %s has buffered inputs that no "
                    "arrow feeds — unless something outside delivers into "
                    "it, it will never run\n", who);
    }

    free(landed);
    free(first_port);

    /*
     * **A program says where its results come from, or it is not
     * finished** (issue 209).
     *
     * Bringing a program up is a caller declaring it finished, and a
     * finished program that has never said what it produces has not
     * said what it is for. The parallel is a C function returning
     * void: it still declares its return, and the declaration is what
     * a caller reads.
     *
     * What the requirement buys is that a program's interface is
     * **total**. Without it there are two different ways to produce
     * nothing — no result station at all, and a result station nobody
     * wired anything into — and only the second is legible. The
     * engine cannot tell a program that deliberately does all its work
     * by side effect from one whose author forgot the results, because
     * a box is a C function and nothing about it says whether it
     * touches the world. Requiring the declaration moves that from
     * something the engine would have to guess into something the
     * program states, and a program that writes to disk and returns
     * nothing then declares exactly that.
     *
     * **A station with nothing wired into it satisfies this**, which
     * is the whole point: the declaration is the interface, and what
     * flows through it is a separate matter.
     *
     * It is asked here rather than while a program is being built,
     * because a program under construction legitimately has no result
     * station yet — the same reason the other whole-program checks
     * live here. A caller that never says it is finished is never
     * asked.
     */
    int has_result = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (s->call && s->door == DOOR_OUT)
            has_result = 1;
    }
    if (!has_result) {
        faults++;
        if (used < (int)sizeof said - 128)
            used += snprintf(said + used, sizeof said - (size_t)used,
                             "%sthis program never says where its results "
                             "come from — one station has to be marked as "
                             "the way out, even when nothing is wired into "
                             "it", used ? "; " : "");
    }

    if (faults) {
        if (used < (int)sizeof said - 48)
            snprintf(said + used, sizeof said - (size_t)used,
                     " — nothing was started");
        return said;
    }

    /*
     * The one place anything walks the station table looking for
     * work. From here on every station is reached by index, through a
     * wire; the engine never scans, and this is the single exception.
     *
     * Every station with no buffered input can run now, because
     * nothing has to arrive first. A station with an unfinished port
     * is skipped rather than refused — the check above already
     * refused it, so reaching here means there are none.
     */
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call || s->seeded)
            continue;

        int has_ring = 0;
        for (int j = 0; j < s->n_in_ports; j++)
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) == IN_PORT_RING)
                has_ring = 1;
        if (has_ring)
            continue;

        /* Through the same door delivery uses — one way a task comes
         * into existence, not two. */
        s->seeded = 1;
        if (map_station_try_start(m, i))
            m->seeded++;
    }
    return NULL;
}
/* }}} */

/* {{{ in_port_kind_name() */
static const char *in_port_kind_name(unsigned char kind)
{
    static const char *const names[IN_PORT_KIND_COUNT] = {
        [IN_PORT_RING]   = "a buffer",
        [IN_PORT_STATIC] = "a static value",
        [IN_PORT_NONE]   = "a port with no source yet",
    };
    return kind < IN_PORT_KIND_COUNT ? names[kind]
                                    : "a port of an unknown kind";
}
/* }}} */

/* {{{ station_out_port() */
/*
 * The port at a given index, walking the list. Ports are few — one
 * for plain, three for a comparator — so the walk is cheaper than
 * any cleverness. Returns null when the port was never created,
 * which delivery reads as "discard".
 */
static out_port_t *station_out_port(station_t *s, int index)
{
    out_port_t *p = s->out_ports;
    for (int i = 0; p && i < index; i++)
        p = p->next;
    return p;
}
/* }}} */

/* {{{ out_port_dests() */
static dest_set_t *out_port_dests(const out_port_t *p)
{
    if (!p)
        return NULL;
    /* Acquire, so everything the writer put in the set before
     * publishing the pointer is visible to whoever follows it. */
    return atomic_load_explicit(&p->dests, memory_order_acquire);
}
/* }}} */

/* {{{ dest_set_build() */
/*
 * A new set from an old one, plus one wire or minus one. Never edits
 * what it was given — that set may have walkers inside it right now,
 * and the whole design rests on nothing it holds ever changing.
 *
 * Passing -1 as a station means "add nothing" or "drop nothing". A
 * drop removes **one** matching pair, not every match, because a wire
 * drawn twice is two wires and removing one should leave the other.
 */
static dest_set_t *dest_set_build(const dest_set_t *from, int add_station,
                           int add_port, int drop_station, int drop_port)
{
    int old_n = from ? from->n : 0;
    int n = old_n + (add_station >= 0 ? 1 : 0);
    dest_set_t *set = calloc(1, sizeof *set + (size_t)(n > 0 ? n : 1)
                                              * sizeof(destination_t));
    if (!set)
        fail_resource("out of memory for a destination set");

    int out = 0;
    int dropped = 0;
    for (int i = 0; i < old_n; i++) {
        if (!dropped && drop_station >= 0
            && from->items[i].station == drop_station
            && from->items[i].port == drop_port) {
            dropped = 1;
            continue;
        }
        set->items[out++] = from->items[i];
    }
    if (add_station >= 0) {
        /* Appended, so fan-out visits destinations in the order the
         * wires were drawn. Nothing in the engine depends on that
         * order — a delivery visits all of them and the order values
         * arrive elsewhere was never promised — but the dump writes
         * them in array order, so keeping it means dump, load, dump
         * produces the same text without anybody arranging it. */
        set->items[out].station = add_station;
        set->items[out].port = add_port;
        out++;
    }
    set->n = out;
    return set;
}
/* }}} */

/* {{{ struct scrap_item */
/*
 * One thing waiting to be freed, and the photograph of every worker's
 * epoch taken when it was filed.
 *
 * A worker whose epoch is now **even** is not inside a task, and one
 * whose epoch **differs from the snapshot** has finished the task it
 * was in. Either way it cannot still be using what this holds.
 */
struct scrap_item {
    struct scrap_item *next;
    void              *p;
    void             (*free_fn)(void *);
    uint64_t          *snapshot;
    int                n_snapshot;
};
/* }}} */

/* {{{ static int nobody_can_hold() */
/*
 * True when no worker can still be inside the task it was in when
 * this was filed.
 *
 * Nothing waits and nothing spins. An idle worker is asleep and
 * therefore even, so it passes without ever having to move — which is
 * what would otherwise deadlock a sweep against a quiet pool.
 *
 * Sixty-four bits, so a counter cannot wrap all the way back to its
 * snapshot in any run this engine will ever have and read as
 * unchanged when it is not.
 */
static int nobody_can_hold(map_t *m, const struct scrap_item *it)
{
    /*
     * Filed when there were no workers at all — during construction,
     * before the pool exists. Nobody can be inside something that was
     * already replaced before anyone could reach it, so it is free to
     * go the first time anybody sweeps.
     */
    if (it->n_snapshot == 0)
        return 1;
    for (int i = 0; i < it->n_snapshot; i++) {
        uint64_t now = pool_worker_epoch(m->pool, i);
        if ((now % 2) == 0)
            continue;                       /* not in a task */
        if (now != it->snapshot[i])
            continue;                       /* a different task since */
        return 0;                           /* might be inside this one */
    }
    return 1;
}
/* }}} */

/* {{{ map_scrap_sweep() */
static void map_scrap_sweep(map_t *m)
{
    pthread_mutex_lock(&m->scrap_mutex);
    struct scrap_item **link = &m->scrap_head;
    while (*link) {
        struct scrap_item *it = *link;
        if (nobody_can_hold(m, it)) {
            /* Unfiled first, freed under the same hold: a second
             * toucher arriving afterwards does not find it, so there
             * is nothing for it to free twice. That is the whole of
             * what this lock is for. */
            *link = it->next;
            it->free_fn(it->p);
            free(it->snapshot);
            free(it);
        } else {
            link = &it->next;
        }
    }
    pthread_mutex_unlock(&m->scrap_mutex);
}
/* }}} */

/* {{{ map_retire() */
static void map_retire(map_t *m, void *p, void (*free_fn)(void *))
{
    if (!p)
        return;

    /* Sweep before filing, so the work happens exactly where the need
     * is created and a program that changes shape forever reclaims as
     * it goes. */
    map_scrap_sweep(m);

    int workers = m->pool ? pool_worker_count(m->pool) : 0;
    uint64_t *snapshot = NULL;
    if (workers > 0) {
        snapshot = calloc((size_t)workers, sizeof *snapshot);
        if (!snapshot)
            fail_resource("out of memory retiring something");
        for (int i = 0; i < workers; i++)
            snapshot[i] = pool_worker_epoch(m->pool, i);
    }

    struct scrap_item *it = calloc(1, sizeof *it);
    if (!it)
        fail_resource("out of memory retiring something");
    it->p = p;
    it->free_fn = free_fn;
    it->snapshot = snapshot;
    it->n_snapshot = workers;

    pthread_mutex_lock(&m->scrap_mutex);
    it->next = m->scrap_head;
    m->scrap_head = it;
    pthread_mutex_unlock(&m->scrap_mutex);

    /*
     * A map with no pool has no workers, so there are no epochs to
     * snapshot and nothing that could be inside anything. Those items
     * are freeable the first time anybody sweeps — which is the
     * construction case, where every wire drawn replaces the set the
     * one before it made.
     */
}
/* }}} */

/* {{{ map_scrap_count() */
CERA_TEST_ONLY static int map_scrap_count(map_t *m)
{
    pthread_mutex_lock(&m->scrap_mutex);
    int n = 0;
    for (struct scrap_item *it = m->scrap_head; it; it = it->next)
        n++;
    pthread_mutex_unlock(&m->scrap_mutex);
    return n;
}
/* }}} */

/* {{{ map_scrap_free_all() */
/*
 * Empties the scrapyard. Called at teardown, when every worker has
 * been collected and nothing can be using anything.
 */
static void map_scrap_free_all(map_t *m)
{
    pthread_mutex_lock(&m->scrap_mutex);
    struct scrap_item *it = m->scrap_head;
    m->scrap_head = NULL;
    while (it) {
        struct scrap_item *next = it->next;
        it->free_fn(it->p);
        free(it->snapshot);
        free(it);
        it = next;
    }
    pthread_mutex_unlock(&m->scrap_mutex);
}
/* }}} */

/* {{{ map_connect() */
void map_connect(map_t *m, int from_station, int port,
                 int to_station, int to_port)
{
    /*
     * A face on the one wiring operation (issue 212), for a caller
     * that wants a refusal to stop the program.
     *
     * This used to be a second implementation with its own rules, and
     * they were quietly *weaker*: it never asked whether the
     * destination was a buffer, and it never compared the widths. So
     * a program could be built by hand that the same program read
     * from a file would have been refused — two sets of rules meant
     * to agree, with one of them missing two.
     */
    const char *no = map_wire(m, from_station, port, to_station, to_port);
    if (no)
        fail(no);
}
/* }}} */

/* {{{ map_start() */
void map_start(map_t *m, int n_workers)
{
    if (m->pool)
        fail("the map was already started");
    /* Delivery rides the pool's finish hook: after a worker runs a
     * task, the map decides where its output goes. This is the whole
     * of the pool's knowledge of the engine — one function pointer. */
    m->pool = pool_create(n_workers, map_deliver, m);

    /* A process-wide "active map" pointer used to be set here, so that
     * a box — which receives only values and has no handle to anything
     * — could reach the statics table's write call. **That pointer was
     * the singleton**: it is what made a process able to run only one
     * map. Issue 405 removed the reason for it by giving a static's
     * write a real address, a station and a port, reachable from
     * outside the graph where every other configuration change already
     * comes from. Nothing here holds process-wide state now, so two
     * maps can run side by side and not see each other. */
}
/* }}} */

/* {{{ map_in_port_depth() */
int map_in_port_depth(map_t *m, int station, int port)
{
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        fail("asking the depth of a port that does not exist");
    in_port_t *sl = &s->in_ports[port];

    /* A maintained count rather than index arithmetic (issue 210d):
     * with values claimed wherever they sit, the distance between two
     * indices stopped describing how many are waiting.
     *
     * A port that is not a buffer still answers, and answers honestly.
     * A static reports whatever its slots were carrying when it
     * stopped being a buffer, which is the truth — those values are
     * waiting, and will be served if it becomes a buffer again. */
    pthread_mutex_lock(&s->mutex);
    int depth = sl->held;
    pthread_mutex_unlock(&s->mutex);
    return depth;
}
/* }}} */

/* {{{ map_destroy() */
/* Phase 7 joints, implemented in the observe module; declared here
 * narrowly so teardown can call them without the whole header. */
void map_observe_stop(map_t *m);
void map_report_shutdown(map_t *m);

void map_destroy(map_t *m)
{
    map_observe_stop(m);
    /* A borrowed pool belongs to the program that made it, and other
     * programs may still be running on it (issue 212). */
    if (m->pool && !m->pool_is_borrowed)
        pool_destroy(m->pool);
    map_report_shutdown(m);
    if (m->station_names) {
        /* Over what the array actually holds, not over the station
         * count: stations are added one at a time and the names grow
         * behind them, so the two are not always equal (issue 212). */
        for (int i = 0; i < m->n_named; i++)
            free(m->station_names[i]);
        free(m->station_names);
    }

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call) {
            pthread_mutex_destroy(&s->mutex);
            continue;
        }
        /* Results nobody took. Freed rather than reported, because
         * the pile-up was already shouted about from the first
         * doubling — saying it twice at teardown would be the same
         * fault wearing a different hat (issue 209). */
        free(s->held);
        s->held = NULL;

        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_free_pages(&s->in_ports[j]);
            /* Both storages, because a port carries both whatever it
             * was being used for (issue 401). */
            in_port_constant_free(&s->in_ports[j]);
        }
        free(s->in_ports);
        out_port_t *p = s->out_ports;
        while (p) {
            free(out_port_dests(p));
            out_port_t *next = p->next;
            free(p);
            p = next;
        }
        pthread_mutex_destroy(&s->mutex);
    }
    /* Everything a rewire replaced and left filed. By now the pool is
     * gone, so nothing can be walking any of it (issue 214). */
    map_scrap_free_all(m);
    pthread_mutex_destroy(&m->scrap_mutex);
    pthread_mutex_destroy(&m->rewire_mutex);
    for (int i = 0; i < m->n_shelves; i++)
        free(m->shelves[i]);
    free(m->shelves);
    free(m);
}
/* }}} */

/* ==================================================================
 *
 * 020 — delivery, readiness, routing
 *
 * Was src/020-delivery.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/020-delivery.c"
/*
 * 020-delivery.c — how a value becomes the next thing that runs.
 *
 * What this is: the central path of the engine, and the moving half
 * of the station layer. A box function has just returned a value;
 * this file carries it into whoever was waiting, notices the moment
 * a station has everything it needs, and turns that moment into a
 * task in the pool. Nothing polls, nothing scans: the act of
 * finishing is the act of scheduling.
 *
 * How it does it, in general terms: a delivery writes one value into
 * a port and then asks one question — is this station now complete?
 *
 * **Almost none of that is under a lock.** Writing takes no lock at
 * all: reserving a slot is one compare-and-swap and the copy that
 * follows goes into bytes the writer owns. The station's mutex covers
 * exactly one thing, the walk that checks every port for a ready slot
 * and then takes one from each — a scan and a state write per port,
 * with no bytes moving. The copies out, the task allocation and the
 * push all happen after it is dropped.
 *
 * What protects the values, then, is not exclusion but **ownership**:
 * a slot in *reserved* or *claimed* belongs to exactly one worker and
 * no other worker may touch it, which is written into the slot state
 * table itself. Bytes nobody else may touch need no lock around them.
 * That is the entire reason two invocations of one station can run at
 * once without meeting, and why several workers can copy out of one
 * station while another holds the lock doing its flips.
 *
 * Built across issues 202–206; routing grew in phase 5 at the
 * dispatch row marked for it. A second row was marked for the pull
 * path and filled in phase 4, and issue 210 took it out again — see
 * docs/implementation-notes/056-no-pull-path.md for why. The shape
 * held up: adding that path was a row, and removing it was a row.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Timing exists only when SORA_STATS is compiled in (issue 702) —
 * a clock read per box on short boxes is real overhead, and a
 * measurement apparatus that cannot be removed is a tax. The macros
 * vanish entirely without the define.
 */
#ifdef SORA_STATS
#include <time.h>
static long stats_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}
#define STATS_MARK(var) long var = stats_now_ns()
#define STATS_CHARGE(counter, since) (counter) += stats_now_ns() - (since)
#else
#define STATS_MARK(var) do { } while (0)
#define STATS_CHARGE(counter, since) do { } while (0)
#endif

/* {{{ die() */
/* Dataflow errors are engine bugs or unbuilt phases; both stop the
 * program and say which station, because a wrong answer that keeps
 * flowing is worse than no answer. */
static void die(const char *what, int station)
{
    fprintf(stderr, "delivery: %s (station %d)\n", what, station);
    abort();
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Port motion (issues 202, 203). Callers hold the station's mutex.   */
/* ------------------------------------------------------------------ */

/* {{{ in_port_scan() */
/*
 * The scan (issue 210d). Start where the hint says, sweep forward,
 * wrap, stop where you started; take the first slot that will move
 * from `from` to `to`, and leave the hint pointing just past it.
 * Returns the slot's index, or -1 for a sweep that found nothing.
 *
 * **The hint is read once**, and that is what bounds the work: the
 * sweep visits at most every slot the port has, exactly one time
 * each, and then gives up. Re-reading a hint that other workers keep
 * pushing forward would let a reader chase it, and a scan that can be
 * outrun is a scan with no bound.
 *
 * **Other workers move the hint while a sweep is in progress, and
 * that is fine.** It is a hint, so a sweeper that started from a value
 * now stale is not wrong, only slightly less lucky — it pays a few
 * extra slots of walking. Nothing about correctness rests on the
 * number being current; what rests on it is only how quickly a worker
 * finds work.
 *
 * The write to the hint is a plain one and races with other writes to
 * it. The worst outcome of losing that race is a hint pointing
 * somewhere unhelpful, which costs a longer sweep next time. Making it
 * atomic would buy nothing, because there is no value it could hold
 * that would be wrong.
 *
 * One function serves both directions because a reader looking for a
 * ready slot and a writer looking for an empty one are the same
 * search with different names, and the ways they differ — which
 * transition, which hint — are arguments rather than logic.
 */
static int in_port_scan(in_port_t *sl, int *hint, int from, int to,
                        void **found, int exclusive)
{
    /*
     * The capacity is read **once**, and that is what bounds the
     * sweep. Growth can raise it while this runs; a scanner that kept
     * re-reading a number another thread keeps raising could be made
     * to walk further every time it looked. Reading it once means the
     * worst case is a sweep that misses slots which appeared a moment
     * ago, and the next sweep finds them.
     */
    int cap = atomic_load_explicit(&sl->capacity, memory_order_acquire);
    int start = *hint;
    if (start < 0 || start >= cap)
        start = 0;

    /*
     * The pages are walked rather than indexed (issue 210e). Resolving
     * a slot's ordinal to a page costs a walk down the list, so doing
     * it per candidate would make a sweep quadratic in the number of
     * pages. It is done **once**, here, and the sweep then follows
     * `next` — one pointer hop per page boundary and plain pointer
     * arithmetic in between, which is what the old single array did
     * everywhere.
     */
    int page = start / sl->page_slots;
    int off  = start % sl->page_slots;
    in_port_page_t *pg = sl->pages;
    for (int i = 0; i < page && pg; i++)
        pg = pg->next;

    for (int i = 0; i < cap && pg; i++) {
        void *slot = pg->slots + (size_t)off * (size_t)sl->stride;
        /*
         * Two ways to ask, and which one is right is a property of the
         * caller rather than of the slot (issue 210d, step 6).
         *
         * A claimer runs under the station's mutex and is looking for
         * a *ready* slot. Nothing else can move one: other claimers
         * are excluded by the lock, and a writer only ever
         * compare-and-swaps from *empty*, which fails against a ready
         * slot without writing. So a load is enough, and only the slot
         * it actually takes is written to.
         *
         * A writer holds no lock and is looking for an *empty* slot,
         * where it genuinely races other writers. That one has to be a
         * compare-and-swap, because the loser must be told it lost.
         *
         * The difference matters because this question is asked of
         * every candidate walked past, not only of the one taken.
         */
        int got = exclusive
            ? (slot_state_at(slot, sl->elem_size) == from
               && (slot_set_at(slot, sl->elem_size, to), 1))
            : slot_move_at(slot, sl->elem_size, from, to);
        if (got) {
            int c = page * sl->page_slots + off;
            int next = c + 1;
            *hint = next >= cap ? 0 : next;
            /* The address, handed back because the sweep already has
             * it. A caller that took an ordinal and then asked for the
             * slot again would walk the page list a second time to
             * recompute something this loop was holding (issue 210e).
             */
            if (found)
                *found = slot;
            return c;
        }
        /* Forward one slot, crossing to the next page at its end and
         * back to the first page at the last one's — the wrap that
         * makes this a ring. */
        if (++off == sl->page_slots) {
            off = 0;
            pg = atomic_load_explicit(&pg->next, memory_order_acquire);
            page++;
            if (!pg) {
                pg = sl->pages;
                page = 0;
            }
        }
    }
    return -1;
}
/* }}} */

/* {{{ in_port_grow_locked() */
/*
 * One more page of slots on the end (issue 210e). Nothing is copied
 * and no existing slot moves, so there is no window to get right.
 *
 * **This replaced a copy-and-unwrap that had an ordering problem with
 * no correct answer**, and the shape of that problem is worth keeping
 * because it is what paging buys. Growing by allocating a larger
 * array meant carrying the live values across. Copy first and then
 * publish, and a value popped from the old array during the copy
 * exists in both places and is delivered twice; publish first and
 * then copy, and readers see an empty buffer while it fills. Neither
 * order is safe on its own. What made it safe was that nothing else
 * could happen at all during the copy, because the station's mutex
 * was held for the whole of it.
 *
 * That protection is exactly what issue 210d spends: once a claimer
 * copies its bytes outside the lock, "nothing else is happening" stops
 * being true, and relocating a slot underneath a worker that owns it
 * is the one thing slot ownership does not cover. So the copy had to
 * go rather than be ordered correctly — **removing the copy removes
 * the requirement rather than satisfying it.**
 *
 * Growth still takes the station's mutex, as one of the rare
 * structural operations, so that two threads meeting a full buffer
 * add one page between them rather than one each.
 */
static void in_port_grow_locked(in_port_t *sl)
{
    in_port_add_page(sl);
    /* A writer looking for space should start where the space now is.
     * Only a hint: being wrong costs a sweep, not a mistake. */
    sl->write_hint = sl->capacity - sl->page_slots;
    sl->growths++;
}
/* }}} */
/* {{{ in_port_write_locked() */
static void in_port_write(station_t *s, in_port_t *sl, const void *value)
{
    /* Look for somewhere to put it, and grow only if there is
     * genuinely nowhere (issue 210d). This used to grow when the tail
     * was one short of the head — a test on indices, which had to
     * leave a slot spare so that the two meeting could mean empty
     * rather than full. Asking the slots directly needs no spare and
     * no arithmetic: a full buffer is one where nothing answers.
     */
    void *slot = NULL;
    int c = in_port_scan(sl, &sl->write_hint, SLOT_EMPTY, SLOT_RESERVED,
                         &slot, 0);
    if (c < 0) {
        /* Nowhere to put it, so the port has to grow — and growth is
         * the one part of a write that takes the station's mutex, so
         * that two threads meeting a full buffer add one page between
         * them rather than one each.
         *
         * The scan is tried **again** after the lock is taken, before
         * growing. Another thread may have grown it while this one was
         * waiting, and adding a second page on top of the first would
         * be a buffer that doubles every time two writers are unlucky
         * together. */
        pthread_mutex_lock(&s->mutex);
        c = in_port_scan(sl, &sl->write_hint, SLOT_EMPTY, SLOT_RESERVED,
                         &slot, 0);
        if (c < 0) {
            in_port_grow_locked(sl);
            c = in_port_scan(sl, &sl->write_hint,
                             SLOT_EMPTY, SLOT_RESERVED, &slot, 0);
        }
        pthread_mutex_unlock(&s->mutex);
        if (c < 0) {
            fprintf(stderr, "delivery: a port with no free slot immediately "
                            "after growing to %d slots\n",
                    atomic_load(&sl->capacity));
            abort();
        }
    }

    /* **The copy happens with no lock held** (issue 210d). The scan
     * moved this slot to *reserved*, which means it belongs to this
     * thread and no other thread may touch its value — so the bytes
     * need no exclusion from anybody. Publishing it cannot fail for
     * any reason but somebody having touched a slot that was this
     * thread's alone, which is an engine bug worth stopping for.
     *
     * The release on that transition is what makes these bytes
     * visible to whoever later takes the slot from *ready*. */
    memcpy(slot, value, (size_t)sl->elem_size);
    if (!slot_move_at(slot, sl->elem_size, SLOT_RESERVED, SLOT_READY)) {
        fprintf(stderr, "delivery: publishing a slot this thread had "
                        "reserved, and somebody else had moved it\n");
        abort();
    }

    /* **Counted after it is published, never before.** A claimer asks
     * this number whether the port holds anything and then goes
     * looking; a count raised before the value was visible would send
     * it to find nothing, which the claim treats as an engine bug
     * rather than a lost race. Raising it afterwards can only mean a
     * claimer looked a moment too early and did not fire — and the
     * writer's own readiness check, which happens next, covers that. */
    int held = atomic_fetch_add_explicit(&sl->held, 1,
                                         memory_order_acq_rel) + 1;

    /* A diagnostic, and now a racy one: two writers can read the same
     * high-water mark and both write it back. The number can therefore
     * under-report by a little under contention. That is accepted
     * rather than fixed, because making it exact would put a
     * read-modify-write on the delivery path to sharpen a figure whose
     * only job is to tell a person that one input side is outpacing
     * its siblings — and it will still say so. */
    if (held > sl->high_water)
        sl->high_water = held;
}
/* }}} */

/* {{{ in_port_take_locked() */
/*
 * Take a ready slot, and take **only** the slot (issue 210d). The
 * value stays where it is; what changes is who owns it.
 *
 * This is the half of a claim that has to be under the station's
 * mutex, and it is deliberately the cheap half: a scan and one state
 * write per port. No bytes move here. The copying happens afterwards,
 * outside the lock, in `in_port_release`.
 *
 * **What makes that safe is ownership rather than exclusion.** A slot
 * in *claimed* belongs to exactly one worker and no other worker may
 * touch its value — which is written into the state table itself, not
 * asserted here. Bytes nobody else may touch need no lock around them.
 *
 * Returns whether it got one. Under the mutex, with the readiness
 * walk having already answered for this port, it always will: no
 * other claimer can be inside, and a writer only ever *adds*
 * availability. A no here is therefore an engine bug rather than a
 * lost race, which is why the caller stops rather than retrying.
 */
static int in_port_take_locked(in_port_t *sl, void **taken)
{
    int c = in_port_scan(sl, &sl->read_hint, SLOT_READY, SLOT_CLAIMED,
                         taken, 1);
    if (c < 0)
        return 0;
    sl->held--;
    return 1;
}
/* }}} */

/* {{{ in_port_release() */
/*
 * The other half, and the expensive one: copy the value out of a slot
 * this worker owns and hand the slot back empty. **Called with no lock
 * held at all.**
 *
 * That is where this whole line of work pays. A two-hundred-byte
 * struct per port used to be copied inside the station's mutex, so
 * every worker delivering into that station waited behind it; now
 * several workers copy out of one station at the same moment while a
 * different worker holds the lock doing its flips.
 *
 * The worker is finished with the slot the moment the copy lands in
 * its buffer. The box does not run until somebody picks the task up
 * later, reading from the task and holding no slot at all — which is
 * why no user code is ever inside this window either.
 */
static void in_port_release(in_port_t *sl, void *slot, void *into)
{
    memcpy(into, slot, (size_t)sl->elem_size);
    if (!slot_move_at(slot, sl->elem_size, SLOT_CLAIMED, SLOT_EMPTY)) {
        fprintf(stderr, "delivery: releasing a slot this thread had claimed, "
                        "and somebody else had moved it\n");
        abort();
    }
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The readiness dispatch (issue 204). Two tables, indexed by the     */
/* port's kind: "does it hold a value?" and "claim one". Another      */
/* port kind is a new row in each, never a new branch in two          */
/* functions that must be kept in agreement.                          */
/* ------------------------------------------------------------------ */

/* {{{ filled: ring / static */
static int ring_filled(const in_port_t *sl)
{
    /* A maintained count rather than two indices differing. The
     * indices stopped being able to answer this when values began
     * being claimed wherever they sat rather than from a computed
     * position (issue 210d) — the distance between a head and a tail
     * describes a contiguous run, and there is no longer one. */
    return sl->held > 0;
}

static int static_filled(const in_port_t *sl)
{
    /* A static's value is simply always there (issue 401). */
    (void)sl;
    return 1;
}

static int none_filled(const in_port_t *sl)
{
    /* Nobody has said where this port's value comes from, so there is
     * no value and there is no prospect of one (issue 210b). This is
     * the whole of what *none* does at run time: a station holding one
     * answers no to readiness forever, no matter what arrives at its
     * other ports, and therefore never runs. Not an error, and not a
     * value — a station waiting to be finished being built. */
    (void)sl;
    return 0;
}

static int (*const in_port_filled[IN_PORT_KIND_COUNT])(const in_port_t *) = {
    [IN_PORT_RING]   = ring_filled,
    [IN_PORT_STATIC] = static_filled,
    [IN_PORT_NONE]   = none_filled,
};
/* }}} */

/* {{{ claim: ring / static */
/*
 * Claiming happens in two moments. Ring values are popped here,
 * under the mutex, which is what makes them spoken-for. A static is
 * resolved later, during task construction, outside the mutex, so
 * the statics table's lock never nests inside a station's. The claim
 * table records that split: a null entry means "resolved at build
 * time".
 *
 * The split used to carry a second reason and a sharper one — a
 * gatherer ran user code, and user code under a station's lock would
 * hand the hot path to whoever wrote the slowest box. Nothing is
 * gathered now (issue 210), so what remains is only lock ordering.
 */
static void ring_claim(in_port_t *sl, void *into, void **taken)
{
    /* Under the mutex the readiness walk has already established that
     * this port holds something, and nothing can have taken it since:
     * other claimers are excluded by the lock, and a writer only ever
     * *adds* availability — it moves a slot empty → reserved → ready
     * and never touches a ready one. So a scan that comes back
     * empty-handed means the count and the slots disagree, which is an
     * engine bug rather than a lost race.
     *
     * **There is no roll-back path here, and there is not meant to
     * be.** An earlier design claimed slots one at a time without the
     * lock and undid them when a later port came up empty, which
     * needed an ordering rule to keep two workers from each holding
     * half a claim forever. Checking every port *before* flipping any
     * of them makes that whole situation unreachable: either all the
     * ports answer and the claim succeeds outright, or one does not
     * and nothing was ever taken. */
    if (!in_port_take_locked(sl, taken)) {
        fprintf(stderr, "delivery: a port that answered ready had no ready "
                        "slot when asked for one\n");
        abort();
    }
    (void)into;
}

static void static_claim_locked(in_port_t *sl, void *into, void **taken)
{
    /* Nothing is taken, so nothing is released afterwards: a static is
     * peeked, never consumed. **And the copy stays under the lock**,
     * which is the one place issue 210d's argument does not reach. A
     * claimed ring slot belongs to one worker, and that ownership is
     * what lets its bytes be copied without exclusion; a static
     * belongs to nobody in particular, so the only thing standing
     * between a claim and a concurrent write to the same constant is
     * this mutex. Moving this copy outside it would reintroduce
     * exactly the torn read that putting the value on the port was
     * meant to make impossible. */
    *taken = NULL;
    /* A copy, under the station's mutex, beside the ring pops (issue
     * 401). It used to be resolved later, during task construction and
     * outside this lock, and the reason was lock ordering: the value
     * lived in a table with a mutex of its own, and that mutex must
     * never nest inside a station's.
     *
     * With the value on the port there is no second lock to order, so
     * the split has nothing left to buy — and closing it gains
     * something real. The static half of an input set is now as
     * mutually consistent as the buffered half: every value a task
     * carries was taken in one window, under one lock, so a box
     * reading two statics can no longer get values that were correct
     * at two different moments and never together.
     *
     * Nothing is consumed. A static is always full, which is the whole
     * reason the kind exists. */
    memcpy(into, sl->constant, (size_t)sl->elem_size);
}

static void none_claim(in_port_t *sl, void *into, void **taken)
{
    (void)taken;
    /* Unreachable, and saying so out loud is the point. The walk above
     * this one asks every port whether it is filled before it claims
     * from any of them, and an unconfigured port answers no — so
     * arriving here means the readiness check and the claim disagreed
     * about the same port, which is an engine bug rather than a
     * situation to handle (issue 210b). */
    (void)sl; (void)into;
    fprintf(stderr, "delivery: claimed from a port that has no source — the "
                    "readiness walk and the claim walk disagreed\n");
    abort();
}

/*
 * **No row here is an absence any more** (issues 210b, 401). The
 * static row was a null, and the caller tested the function pointer
 * for truth to discover it; the meaning was "resolved later, outside
 * the mutex", which was a real and correct decision written as a hole
 * that a reader had to already know the meaning of. The decision it
 * encoded is gone with the statics table, so the hole is gone with it.
 */
static void (*const in_port_claim_locked[IN_PORT_KIND_COUNT])
                   (in_port_t *, void *, void **) = {
    [IN_PORT_RING]   = ring_claim,
    [IN_PORT_STATIC] = static_claim_locked,
    [IN_PORT_NONE]   = none_claim,
};
/* }}} */

/* {{{ station_input_bytes() */
/*
 * Total bytes of one complete input set. Element sizes never change
 * after construction, so this reads without the mutex.
 */
static int station_input_bytes(const station_t *s)
{
    int total = 0;
    for (int i = 0; i < s->n_in_ports; i++)
        total += s->in_ports[i].elem_size;
    return total;
}
/* }}} */

/* {{{ station_ready_and_claim_locked() */
/*
 * The one rule made real: walk every port; if any is empty, nothing
 * happens and the value just written waits for its siblings. If all
 * are full, claim one value from each ring port into the caller's
 * buffer — copied out and the head advanced, so no other thread can
 * claim the same ones. Returns whether a task became due.
 */
static int station_ready_and_claim_locked(station_t *s, unsigned char *claimed,
                                          void **taken)
{
    /*
     * **Check all, then flip all** (issue 210d), and the order is the
     * whole safety argument. Nothing can take a ready slot away
     * between the two walks: other claimers are excluded by this
     * mutex, and a writer only ever adds availability — it moves a
     * slot empty → reserved → ready and never touches a ready one. So
     * either every port answers and the claim succeeds outright, or
     * one does not and nothing was ever taken.
     *
     * That is what makes a roll-back path unnecessary rather than
     * merely unused, and it is why livelock here is unreachable rather
     * than prevented.
     */
    for (int i = 0; i < s->n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        if (!in_port_filled[sl->kind](sl))
            return 0;
    }

    int offset = 0;
    for (int i = 0; i < s->n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        /* Every kind, unconditionally. The caller used to test the
         * function pointer here because the static row was null; there
         * is no null now, so the dispatch is a call rather than a call
         * guarded by a question about the table's own shape.
         *
         * A ring port records the slot it took and copies nothing; a
         * static copies here, under the lock, and records nothing.
         * The two are different because only one of them is owned. */
        in_port_claim_locked[sl->kind](sl, claimed + offset, &taken[i]);
        offset += sl->elem_size;
    }
    return 1;
}
/* }}} */

/* {{{ station_release_claimed() */
/*
 * The copies, and the release, with **no lock held** (issue 210d).
 *
 * Runs after the caller has dropped the station's mutex. Each slot
 * named here is in *claimed*, which means it belongs to this worker
 * and no other worker may touch its value — so the bytes need no
 * exclusion, and several workers can be doing this on one station at
 * the same moment while another holds the lock doing its flips.
 *
 * Statics are absent from this walk by construction: they recorded no
 * slot, because they were peeked rather than taken, and their bytes
 * were copied under the lock where the only protection they have
 * lives.
 */
static void station_release_claimed(station_t *s, unsigned char *claimed,
                                    void **taken)
{
    int offset = 0;
    for (int i = 0; i < s->n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        if (taken[i])
            in_port_release(sl, taken[i], claimed + offset);
        offset += sl->elem_size;
    }
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The task struct in motion (issue 206).                             */
/* ------------------------------------------------------------------ */

/* {{{ task_build() */
/*
 * One allocation, sized exactly for this box: the struct, the array
 * of input pointers, the input bytes, the output bytes. Runs after
 * the station's mutex is released, so a slow allocator delays one
 * task rather than everyone aiming at that station. Static slots are
 * resolved here — outside the lock, on the assembling thread's own
 * time. Non-static since phase 6: the seed sweep builds its first
 * tasks through this same door.
 */
static task_t *task_build(map_t *m, int station_index,
                   const unsigned char *claimed, int port)
{
    station_t *s = map_station(m, station_index);

    int in_bytes = station_input_bytes(s);
    size_t total = sizeof(task_t)
                 + (size_t)s->n_in_ports * sizeof(void *)
                 + (size_t)in_bytes
                 + (size_t)s->out_size;

    task_t *t = malloc(total);
    if (!t)
        die("out of memory building a task", station_index);

    t->call = s->call;
    t->station = station_index;
    t->port = port;
    t->n_in = s->n_in_ports;
    /* Zero rather than left over: with timing compiled out nothing
     * ever writes it, and the delivery walk adds it to the station
     * unconditionally. */
    t->box_ns = 0;

    /* The pointer array sits immediately after the struct; the value
     * bytes after it; the output after those. One free() takes the
     * whole thing back. */
    t->in = (void **)(t + 1);
    unsigned char *data = (unsigned char *)(t->in + s->n_in_ports);

    /*
     * Every value was claimed under the station's mutex before this
     * ran, whatever kind of port it came from (issue 401) — so this is
     * one copy per port out of the caller's buffer, and there is no
     * longer a case that reaches back into the map for a value it
     * failed to bring along.
     *
     * The claimed buffer may be null only for a station with no ports
     * at all, which the loop below then does not enter.
     */
    int offset = 0;
    for (int i = 0; i < s->n_in_ports; i++) {
        in_port_t *sl = &s->in_ports[i];
        t->in[i] = data + offset;
        if (!claimed)
            die("building a task with no claimed values for a station that "
                "has ports", station_index);
        memcpy(t->in[i], claimed + offset, (size_t)sl->elem_size);
        offset += sl->elem_size;
    }
    /* The program this task belongs to, carried so that finishing it
     * does not depend on which pool ran it (issue 212). The map was
     * already being passed here and discarded, which is how small
     * this turned out to be. */
    t->owner = m;

    t->out = s->out_size > 0 ? data + in_bytes : NULL;
    return t;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Delivery itself (issues 204, 205).                                 */
/* ------------------------------------------------------------------ */

/* {{{ map_station_try_start() */
/*
 * Readiness, claim, build, push — a delivery with the delivering taken
 * out. Three callers wanted exactly this and were each doing their own
 * version of it (issue 401).
 *
 * The seed sweep enqueued a station without asking whether it was
 * ready, which was safe only while the unasked question happened to
 * have the same answer — and stopped being safe the moment a static's
 * value had to be claimed like any other, because the seed had no
 * claim buffer to put one in.
 *
 * Setting or writing a static is supposed to run the ordinary
 * readiness check on its station. That is what replaced the pull path,
 * and it is what makes a chain of stations wired through static ports
 * into a recalculation graph. It was documented as a guarantee and was
 * not actually happening.
 *
 * A write cannot make something run that could not run anyway, because
 * the check it triggers is this one: an empty ring port still answers
 * no, and the engine will not invent a value for it.
 */
int map_station_start_after(map_t *m, int station,
                            void (*while_locked)(void *), void *ctx)
{
    if (station < 0 || station >= m->n_stations)
        die("starting a station outside the table", station);
    station_t *s = map_station(m, station);
    if (!s->call)
        die("starting a station with no box placed", station);
    /* Removed and not yet reclaimed: nothing new starts from it
     * (issue 216). */
    if (atomic_load_explicit(&s->removed, memory_order_acquire))
        return 0;

    int in_bytes = station_input_bytes(s);
    unsigned char claimed[in_bytes > 0 ? in_bytes : 1];
    void *taken[s->n_in_ports > 0 ? s->n_in_ports : 1];
    int port = 0;

    pthread_mutex_lock(&s->mutex);
    /*
     * Whatever the caller wanted done *inside* this hold, done here.
     *
     * There is exactly one such caller and it is writing a static
     * (issue 210d, step 7). A write to a static and the readiness
     * check it triggers both want this mutex, and doing them as two
     * acquisitions would leave a gap between the value changing and
     * the question being asked. Handing the work in rather than
     * exporting a locked variant is what keeps every acquisition of a
     * station's mutex inside this file, where the discipline can be
     * read in one place.
     */
    if (while_locked)
        while_locked(ctx);
    int due = station_ready_and_claim_locked(s, claimed, taken);
    if (due && s->kind == STATION_ITERATOR && s->n_out_ports > 0) {
        port = s->cursor;
        s->cursor = (s->cursor + 1) % s->n_out_ports;
    }
    pthread_mutex_unlock(&s->mutex);

    if (due) {
        /* Outside the lock: the copies, then the task (issue 210d). */
        station_release_claimed(s, claimed, taken);
        pool_push(m->pool, task_build(m, station, in_bytes > 0 ? claimed : NULL,
                                      port));
    }
    return due;
}
/* }}} */

/* {{{ map_station_try_start() */
int map_station_try_start(map_t *m, int station)
{
    return map_station_start_after(m, station, NULL, NULL);
}
/* }}} */

/* {{{ map_station_start_while_ready() — issue 712 */
/*
 * **Keep starting while the station stays ready**, which is what the
 * one rule says should happen and what asking once does not do.
 *
 * The gap this closes is narrow and was invisible until a program was
 * revived from a capture. On the delivery path, asking once is
 * enough: a value arrives, at most one task can become due, and the
 * next value asks again. But a port can *accumulate* while another
 * port has nothing — and then that other port fills all at once, by a
 * constant being bound to it or by a revival putting a queue back.
 * At that moment the station is ready several times over, and the
 * single ask started one task and stranded the rest.
 *
 * A station whose every port is a static is ready forever, because a
 * constant is never consumed — so looping on one would never stop.
 * It is left alone here: nothing accumulates in a station with no
 * buffer, so there is never more than one start owed to it, and the
 * seed sweep already gives it that one.
 *
 * Returns how many tasks became due.
 */
int map_station_keep_starting(map_t *m, int station)
{
    station_t *s = map_station(m, station);

    /*
     * **A station with no buffer is asked once and left alone**, and
     * that is the whole reason this is split from the ask itself.
     * A constant is never consumed, so such a station is ready
     * forever and looping on it would never stop. It gets exactly one
     * start when it first becomes complete, and exactly one more each
     * time a constant on it is written — which is what "run again
     * because something changed" means where there is nothing to
     * drain.
     */
    for (int j = 0; j < s->n_in_ports; j++)
        if (atomic_load_explicit(&s->in_ports[j].kind,
                                 memory_order_relaxed) == IN_PORT_RING)
            goto drain;
    return 0;

drain:;
    int started = 0;
    while (map_station_try_start(m, station))
        started++;
    return started;
}

int map_station_start_while_ready(map_t *m, int station)
{
    int started = map_station_try_start(m, station) ? 1 : 0;
    return started + map_station_keep_starting(m, station);
}
/* }}} */

/* {{{ map_deliver_value() */
int map_deliver_value(map_t *m, int station, int port, const void *value)
{
    if (station < 0 || station >= m->n_stations)
        die("delivering to a station outside the table", station);
    station_t *s = map_station(m, station);

    /*
     * The station may have been removed since this value set out
     * (issue 216). A worker reads a port's destinations once and then
     * visits them, so a removal can land between the read and the
     * visit — the wire it was following is gone, and so is what it
     * pointed at.
     *
     * **The value is discarded, which is what this engine already
     * does with a value that has nowhere to go.** A port wired
     * nowhere discards; an unwired comparator outcome discards. This
     * is the same shape: a value in flight toward something that is
     * no longer there. Removing a station is a deliberate act by
     * somebody who knew what was wired into it, and the alternative —
     * stopping the program — would make every removal a race against
     * whatever was already moving.
     */
    if (atomic_load_explicit(&s->removed, memory_order_acquire) || !s->call)
        return 0;

    if (port < 0 || port >= s->n_in_ports)
        die("delivering to a port the station does not have", station);
    /*
     * **A value arriving at a static port overwrites it** (issue 405),
     * rather than queueing into a buffer the port does not have.
     *
     * This is not the back channel returning, and the distinction is
     * the whole point. The back channel had a *box* reach out and
     * write a value with nothing in the wiring showing it, so two
     * stations could be talking with no arrow between them and the
     * picture lied. Here the box is untouched: it takes its
     * arguments, returns one value, remembers nothing, and has no
     * idea what happens next. **The wire is what says this value
     * overwrites a static** — visible in the map file, visible in the
     * dump, drawable on a canvas. A box still may not write a static;
     * a wire may deliver into one.
     *
     * What it buys is a constant that is *computed* rather than
     * written down. A station that reads a clock, seeded so it runs
     * once, wired into a downstream station's static port: it runs,
     * the timestamp lands, and every invocation afterwards reads it.
     * "Read once at startup and work from that moment" stops needing
     * a feature and becomes something drawn.
     *
     * It goes through the same write an outside caller makes — one
     * path, taking the station's mutex, which the claim already takes,
     * so no claim can see a half-written value. **Two arrows into one
     * static port is last-writer-wins, nondeterministically**, which
     * is stated as a non-guarantee rather than left as a surprise.
     */
    if (s->in_ports[port].kind == IN_PORT_NONE)
        die("delivering into a port that has no source yet", station);
    if (s->in_ports[port].kind == IN_PORT_STATIC) {
        map_in_port_static_write(m, station, port, value,
                                 s->in_ports[port].elem_size);
        return 0;
    }

    /* The claim buffer lives on this thread's stack, sized for one
     * complete input set. It exists so the readiness check allocates
     * nothing while holding the mutex. */
    int in_bytes = station_input_bytes(s);
    unsigned char claimed[in_bytes > 0 ? in_bytes : 1];

    int out_port = 0;
    void *taken[s->n_in_ports > 0 ? s->n_in_ports : 1];

    /* **The write takes no lock** (issue 210d). Reserving a slot is a
     * single compare-and-swap and the copy that follows goes into
     * bytes this thread owns, so deliveries into one station never
     * serialize against each other — only task construction does. */
    in_port_write(s, &s->in_ports[port], value);

    STATS_MARK(wait_start);
    pthread_mutex_lock(&s->mutex);
    STATS_CHARGE(s->mutex_wait_ns, wait_start);
    int due = station_ready_and_claim_locked(s, claimed, taken);
    if (due && s->kind == STATION_ITERATOR && s->n_out_ports > 0) {
        /* The one memory a station keeps, touched at the one moment
         * only one thread can be looking (issue 504): this task
         * takes the cursor's exit, the cursor moves on, and the
         * choice rides out inside the task. The box never sees it. */
        out_port = s->cursor;
        s->cursor = (s->cursor + 1) % s->n_out_ports;
    }
    pthread_mutex_unlock(&s->mutex);

    if (due) {
        /* Outside the lock: the copies, then the task (issue 210d). */
        station_release_claimed(s, claimed, taken);
        pool_push(m->pool, task_build(m, station, claimed, out_port));
    }
    return due;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Routing (issue 205; grows in phase 5).                             */
/* The kind is consulted at exactly this one moment on the way out    */
/* and nowhere else in the engine. A table, not a chain: a fourth     */
/* kind is a row.                                                     */
/* ------------------------------------------------------------------ */

/* {{{ route: plain / comparator / iterator */
static int route_plain(station_t *s, task_t *t)
{
    (void)s; (void)t;
    /* A plain box has one exit. */
    return 0;
}

static int route_comparator(station_t *s, task_t *t)
{
    /* The threshold rode along as the task's last input — claimed
     * like any other port, never handed to the box (issue 502). The
     * comparison happens here, after the box returned, through the
     * type's own three-way compare (issue 503): the sign maps
     * straight onto the ports — less is 0, equal is 1, greater 2. */
    int sign = s->compare(t->out, t->in[t->n_in - 1]);
    return sign + 1;
}

static int route_iterator(station_t *s, task_t *t)
{
    (void)s;
    /* Chosen at enqueue time, under the station's mutex, and
     * recorded in the task (issue 504) — so two tasks assembled a
     * moment apart carry different exits no matter which finishes
     * first. Reading it here is the whole row. */
    return t->port;
}

static int (*const route_choose[STATION_KIND_COUNT])(station_t *, task_t *) = {
    [STATION_PLAIN]      = route_plain,
    [STATION_COMPARATOR] = route_comparator,
    [STATION_ITERATOR]   = route_iterator,
};
/* }}} */

/* {{{ station_hold_result() */
/*
 * Keep one result for somebody outside to take (issue 209).
 *
 * Under the station's own mutex, because a worker finishing a box and
 * a caller draining results genuinely meet here — unlike a slot,
 * which belongs to exactly one worker and needs no lock around its
 * bytes, a held result belongs to the station until it is taken.
 *
 * Doubling, and **the growth is shouted from the first one** rather
 * than summarised at the end like the other two piles. A port backing
 * up means uneven inputs and the task ring backing up means slow
 * consumers; both are performance signals worth a line at teardown.
 * This backing up means nobody is collecting the program's results at
 * all, which is not a performance signal — it is a program computing
 * into somewhere nobody is looking, and waiting until shutdown to
 * mention it wastes the entire run.
 */
static void station_hold_result(map_t *m, int index, station_t *s,
                                const void *value)
{
    pthread_mutex_lock(&s->mutex);
    if (s->n_held == s->held_room) {
        int room = s->held_room ? s->held_room * 2 : 8;
        void *bigger = realloc(s->held, (size_t)room * (size_t)s->out_size);
        if (!bigger) {
            pthread_mutex_unlock(&s->mutex);
            fprintf(stderr, "delivery: out of memory holding a result\n");
            abort();
        }
        s->held = bigger;
        s->held_room = room;
        if (s->held_growths++ > 0 || room > 8)
            fprintf(stderr,
                    "observe: results are piling up at %s — %d waiting and "
                    "nobody taking them; this program is computing into "
                    "somewhere nobody is looking\n",
                    (m->station_names && index < m->n_named
                     && m->station_names[index])
                        ? m->station_names[index] : "an output station",
                    s->n_held);
    }
    memcpy((unsigned char *)s->held + (size_t)s->n_held * (size_t)s->out_size,
           value, (size_t)s->out_size);
    s->n_held++;
    pthread_mutex_unlock(&s->mutex);
}
/* }}} */

/* {{{ map_deliver() */
/*
 * The delivery walk: the pool's finish hook. Two paths at the top —
 * a sink's task is simply done (a box declared void needs no engine
 * support at all), and everything else chooses one port and delivers
 * its output to every destination on it. A port wired nowhere
 * discards, which is what an unwired comparator outcome wants.
 *
 * **The walk takes no lock and copies nothing** (issue 214). A port's
 * destinations are one immutable array; a rewire builds a whole new
 * one and swaps the pointer, so reading that pointer once yields
 * something nobody will ever modify. The old set is filed rather than
 * freed, so a walker already inside one is not walking freed memory.
 *
 * It used to snapshot the list onto this walker's stack under the
 * station's mutex, because a rewire could unlink and free a node
 * under a walker's feet. That cost a lock acquisition and a copy
 * proportional to fan-out **on every value the engine moved**, and it
 * was the last thing holding the station's mutex on the hot path.
 */
static void map_deliver(void *ctx, task_t *t)
{
    /*
     * **The task says which program it belongs to**, not the pool
     * (issue 212). The hook's own context is ignored, and that is the
     * whole of what lets one pool serve several programs: a station
     * index means nothing without the table it indexes, so while the
     * map came from the pool, a pool could serve exactly one map.
     */
    (void)ctx;
    map_t *m = t->owner;
    station_t *s = map_station(m, t->station);

    s->runs++;
    /* The box's own time, charged onto the task by the shim and moved
     * onto the station here — the one place that holds both (issue
     * 405). Zero when timing is compiled out, so this costs an add of
     * nothing rather than a branch. */
    s->box_ns += t->box_ns;

    if (s->out_size == 0)
        return;

    int out_port_index = route_choose[s->kind](s, t);

    out_port_t *port = station_out_port(s, out_port_index);
    dest_set_t *set = out_port_dests(port);
    if (!set || set->n == 0) {
        /*
         * Nobody is wired here. For almost every station that means
         * **discard**, deliberately: an unwired comparator branch is
         * the ordinary case, and a program that sends everything below
         * a threshold somewhere means to drop the rest.
         *
         * For a designated output it means **hold**, and that is the
         * one rule the designation adds (issue 209). A program's own
         * results are the one thing discarding makes meaningless —
         * a program that computed them and dropped them did nothing.
         */
        if (s->door == DOOR_OUT)
            station_hold_result(m, t->station, s, t->out);
        return;
    }

    /* A hundred destinations is a hundred lock-write-check cycles by
     * this one worker before it takes more work — acceptable, because
     * each delivery may unblock a station, so this worker is busy
     * manufacturing parallelism for everyone else. */
    for (int i = 0; i < set->n; i++)
        s->produced += map_deliver_value(m, set->items[i].station,
                                         set->items[i].port, t->out);
}
/* }}} */

/* 020's private macros end with 020. */
#undef STATS_MARK
#undef STATS_CHARGE

/* ==================================================================
 *
 * 027 — support for generated code
 *
 * Was src/027-emitted-support.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/027-emitted-support.c"
/*
 * 027-emitted-support.c — walking what the generator wrote.
 *
 * What this is: the hand-written half of what the generator emits — lookups,
 * printing, and placement-by-name. The data it walks is generated at
 * build time from the box sources; this file never changes when a
 * box does, which is the entire division of labor.
 *
 * How it does it, in general terms: linear walks over two small
 * const arrays. A program has dozens of boxes, not millions, and a
 * lookup happens at load time, not on the delivery path; simplicity
 * wins over any table cleverness.
 */


#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Rows added while the program runs live next door (issue 310). They
 * are declared here rather than in a header because only these two
 * lookups need them: everything else reaches a box through the row it
 * was already handed.
 */
static const box_place_t *late_recover_box(const char *name);
static const box_place_t *late_place_find(const char *name);
const char        *late_source_text(const char *path);

/* {{{ box_place_find() */
/*
 * Which generated placement function writes this box's station
 * (issue 311b). Compiled-in rows first, then anything that arrived
 * after the program started.
 *
 * That order is deliberate and it was the record's rule before it was
 * this one's: a box the program was built with wins over one added
 * afterwards under the same name, so bringing in new code can never
 * quietly replace something a map already depends on. Replacing a
 * name that was never built in works; shadowing one that was does not.
 *
 * **This is the whole of by-name placement, and now the whole of
 * by-name anything** (issue 311b). The record that used to sit beside
 * this table is gone: it held a name, a shim, parameter sizes and type
 * names, a return type, a task size and a comparison — and every one
 * of those is written directly onto the station by the placement
 * function, from a `sizeof` the compiler folded, so the record was a
 * copy of numbers nobody read twice.
 *
 * A placement function is hand placement written by the generator, so
 * naming a box is only a way of finding which one to call — and once
 * the generator reads maps itself it emits the call directly and this
 * lookup stops existing too (issue 311d).
 */
/*
 * **Three ways to say which box, and they are one rule** (issue
 * 311a): a bare function name; a basename and a function; a path and
 * a function.
 *
 * The path is not a fallback — it is a more specific way of saying
 * the same thing, so nobody has to guess which form is the real one
 * and an author who prefers paths everywhere is not fighting the
 * format.
 *
 * One function rather than a loop body, because the compiled-in rows
 * and the rows that arrived while the program ran are searched
 * separately and must agree about what a name means. They did not,
 * briefly, and the symptom was a dump that could not shorten an
 * address it had just written.
 */
static int box_place_matches(const box_place_t *row, const char *name)
{
    if (!strchr(name, ':'))
        return strcmp(row->name, name) == 0;

    /*
     * An address. The whole thing matches exactly, or the part before
     * the colon is a *basename* — `math.c:add` finding
     * `src/boxes/math.c:add`. Matching against a suffix of the path is
     * what makes the brief form work, and requiring the slash before
     * it is what stops `path.c` matching `mypath.c`.
     */
    if (strcmp(row->address, name) == 0)
        return 1;
    size_t n = strlen(name);
    size_t a = strlen(row->address);
    return a > n && row->address[a - n - 1] == '/'
           && strcmp(row->address + a - n, name) == 0;
}

const box_place_t *box_place_find(const char *name)
{
    if (!name || !*name)
        return NULL;
    for (int i = 0; i < n_box_places; i++)
        if (box_place_matches(&box_places[i], name))
            return &box_places[i];
    return late_place_find(name);
}
/* }}} */

/* {{{ struct_text_find() */
/*
 * One type's reader and writer, by name (issue 408). Asked at
 * placement, so that a port holding a struct constant is *handed* its
 * pair — the same way a station is handed its shim and its comparison
 * — and nothing searches anything afterwards.
 */
const struct_text_t *struct_text_find(const char *type_name)
{
    if (!type_name)
        return NULL;
    for (int i = 0; i < n_struct_texts; i++)
        if (struct_texts[i].name
            && strcmp(struct_texts[i].name, type_name) == 0)
            return &struct_texts[i];
    return NULL;
}
/* }}} */

/* {{{ struct_find() */
const struct_info_t *struct_find(const char *type_name)
{
    for (int i = 0; i < n_struct_layouts; i++)
        if (strcmp(struct_layouts[i].name, type_name) == 0)
            return &struct_layouts[i];
    return NULL;
}
/* }}} */

/* {{{ emitted_print() */
/*
 * **What a program can place, and where each one came from.**
 *
 * It used to print every field of every box record — parameter types
 * and sizes, the return type, the task size, whether a comparison
 * existed. None of that is carried any more (issue 311b): the numbers
 * are folded into placement functions and the names live in the box
 * source the binary carries. What is left to print is what is left to
 * know: which names a program answers to, and which file each one was
 * compiled from.
 */
void emitted_print(FILE *out)
{
    fprintf(out, "emitted: %d boxes, %d structs\n",
            n_box_places, n_struct_layouts);
    for (int i = 0; i < n_box_places; i++)
        fprintf(out, "  %-20s %s\n", box_places[i].name,
                box_places[i].address);
    for (int i = 0; i < n_struct_layouts; i++) {
        const struct_info_t *s = &struct_layouts[i];
        fprintf(out, "  struct %s: %d bytes, %d fields\n",
                s->name, s->size, s->n_fields);
        for (int f = 0; f < s->n_fields; f++) {
            const field_info_t *fl = &s->fields[f];
            static const char *const kind_names[] = {
                "int", "uint", "float", "string", "struct",
            };
            fprintf(out, "    +%-3d %-12s %-6s %d bytes\n",
                    fl->offset, fl->name, kind_names[fl->kind], fl->size);
        }
    }
}
/* }}} */

/* {{{ box_source_text() */
/*
 * **The C one box source was compiled from** (issue 311c), by the
 * path the build knew it as.
 *
 * A bare basename matches too, because that is how a person refers to
 * a file they can see — `029-demo-boxes.c` rather than
 * `src/boxes/029-demo-boxes.c`. Where two sources share a basename
 * the full path is the way to say which, exactly as it is for
 * addressing a box.
 */
const char *box_source_text(const char *path)
{
    if (!path || !*path)
        return NULL;

    for (int i = 0; i < sora_n_box_sources; i++)
        if (strcmp(sora_box_sources[i].path, path) == 0)
            return sora_box_sources[i].text;

    /* Then by basename, for somebody who typed what they could see. */
    for (int i = 0; i < sora_n_box_sources; i++) {
        const char *slash = strrchr(sora_box_sources[i].path, '/');
        const char *base = slash ? slash + 1 : sora_box_sources[i].path;
        if (strcmp(base, path) == 0)
            return sora_box_sources[i].text;
    }

    /*
     * **And then what has arrived since** (issue 311d). The build is
     * only the first iteration; a program that has been handed code
     * since is made of more than the build compiled, and asking what
     * it is made of has to get all of it.
     *
     * The build is asked first so that a source compiled in wins over
     * a later one of the same path — the compiled-in copy is what the
     * program's own stations were built from, and the answer should
     * describe the program rather than the last thing that happened
     * to it.
     */
    return late_source_text(path);
}
/* }}} */

/* {{{ map_build_find() */
/*
 * **The compiled form of one description** (issue 311d), by the path
 * the build knew it as or by the bare name somebody would type — the
 * same two ways a box source is found, for the same reason.
 */
const map_build_t *map_build_find(const char *path)
{
    if (!path || !*path)
        return NULL;

    for (int i = 0; i < sora_n_map_builds; i++)
        if (strcmp(sora_map_builds[i].path, path) == 0)
            return &sora_map_builds[i];

    for (int i = 0; i < sora_n_map_builds; i++) {
        const char *slash = strrchr(sora_map_builds[i].path, '/');
        const char *base = slash ? slash + 1 : sora_map_builds[i].path;
        if (strcmp(base, path) == 0)
            return &sora_map_builds[i];
    }
    return NULL;
}
/* }}} */

/* {{{ map_place_box() */
void map_place_box(map_t *m, int station, const char *box_name, int kind)
{
    /*
     * A place that has been removed but not yet reclaimed is not free
     * (issue 216). Its record still carries the port count and return
     * size a task being built right now needs, and the sweep that
     * clears them is what makes the place available. Placing here
     * before then would have the sweep clear the *new* station's
     * record.
     */
    if (station >= 0 && station < m->n_stations
        && atomic_load_explicit(&map_station(m, station)->removed,
                                memory_order_acquire)) {
        fprintf(stderr,
                "map: station %d was removed and is not reclaimed yet — "
                "something may still be inside a task built from it\n",
                station);
        abort();
    }

    const box_place_t *bp = box_place_find(box_name);
    if (!bp) {
        /*
         * Before giving up: a box added while some *earlier* process
         * ran left its source behind under its own name, and this may
         * be that program's dump being reloaded (issue 310). Recovery
         * compiles it back into existence and says out loud that it
         * did — a fallback nobody was told about is the shape this
         * project treats as an error, so this one announces itself
         * every time.
         *
         * When there is no such source, this returns null and the
         * message below is the ordinary answer for a misspelled name,
         * which is the most common mistake a map will ever contain.
         */
        bp = late_recover_box(box_name);
    }
    if (!bp) {
        fprintf(stderr,
                "map: no box named '%s' anywhere the build could see — misspelled, or its "
                "source is not under src/boxes/\n", box_name);
        abort();
    }

    /*
     * **The station is written by the generated placement function**
     * (issue 311b), and by nothing else. Every number in it is a
     * `sizeof` the compiler folded into an immediate, so the sizes are
     * not read from anywhere at run time — they were computed while
     * the box was being compiled and never stored.
     *
     * **The two comparator refusals moved out of here** and into that
     * function, where they were already duplicated. A box that returns
     * nothing cannot be a comparator, because there is nothing to
     * compare; a box whose return type has no comparison cannot be
     * one, because routing on raw bytes would produce an answer and it
     * would be wrong (issue 503). Both are refused wherever a box is
     * placed from rather than only through this door, and the
     * generated version says *more* — it names the box by its full
     * address rather than by the bare word a map happened to use.
     *
     * That was the last thing this path did with the box record, and
     * with it gone the record has no readers left.
     */
    bp->place(m, station, kind);

    /*
     * The type each port feeds, the comparator's extra port, and the
     * comparison function are all written by the placement function
     * too — they used to be copied out of the record here, and that
     * was the last thing this path did with it.
     *
     * Type names are what let a static's text become bytes of the
     * right shape and what the wire checker reports; the comparison is
     * resolved at placement so the delivery path compares through a
     * pointer the station holds rather than looking anything up per
     * value. None of that changed. Only who writes it did.
     */
}
/* }}} */

/* ==================================================================
 *
 * 033 — constants, and values from text
 *
 * Was src/033-statics.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/033-statics.c"
/*
 * 033-statics.c — the values that are simply always there.
 *
 * What this is: a constant living on the input port that reads it.
 * Thresholds, file paths, configuration. A static port is always
 * full, never consumed, and never affects whether a station is ready;
 * this file owns the text-to-bytes reader that fills one, the writer
 * that turns one back into text, and the runtime write that changes
 * one while the program runs.
 *
 * **There was a table here.** Numbered entries on the map, shared by
 * every port that named one, behind a mutex of their own. What that
 * cost was out of proportion to what it bought: it was map-level
 * mutable state, so a process could hold only one running program; it
 * was a second lock nested inside the station's on every claim; and
 * an entry's bytes were shaped by whichever port bound it first, so
 * two ports of different types could read the same bytes each their
 * own way. Issue 401 moved the value onto the port and the table
 * stopped having anything to hold.
 *
 * A station is one instantiation of a box, wired its own way. Its
 * input ports are its own, and what one of them holds is a property
 * of that port on that station and of nothing else. Sharing, when it
 * is wanted, is drawn: one station holds the value and everyone who
 * needs it has an arrow from it, which costs a station and gains a
 * wire somebody can see.
 *
 * How it does it, in general terms: a port is given text and parses
 * it, at its own declared type, into its own storage — a number for
 * an int port, a brace walk over the generated field table for a
 * struct port, the characters themselves for a string port. From then
 * on a claim is a memcpy under the station's mutex, beside the ring
 * pops, and a runtime write is a size-checked overwrite under the
 * same mutex. Anything malformed is fatal at the moment it is given,
 * naming the station, the port, and the field, because a static that
 * half-parses is silent corruption wearing a default.
 *
 * The reader has a mirror at the bottom of this file, turning bytes
 * back into text. Nothing needed one until the table went: the table
 * kept the original string a file gave it and the dump echoed that
 * string, and with no text retained anywhere the bytes have to be
 * spoken. The two walk the same field table in opposite directions
 * and belong beside each other for exactly that reason.
 */





#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Where an error happened. Published as sora_where_t (issue 408),
 * because generated readers name the same place, and spelled `where_t`
 * here so the file that has always used the short name still reads
 * the way it did.
 */
typedef sora_where_t where_t;

/* {{{ die_static() */
static void die_static(const where_t *w, const char *what)
{
    /*
     * **Exit 70 rather than a core dump** (issue 106): text that will
     * not parse is a fault in the calling code, which is a caller can
     * correct and retry — and that is a different thing from running
     * out of memory, which no edit fixes. A shell script can now tell
     * them apart; before, both arrived as the same abnormal death.
     *
     * It still stops where the malformed value is rather than handing
     * a refusal back to be collected with others, and that is not an
     * omission. A value that will not parse is found while the
     * station holding it is being built, and every other fault found
     * at that moment stops there too — because continuing past a
     * station that could not be built means asking questions of
     * something that is not there. What accumulates is the
     * whole-program pass, which runs when everything exists and can
     * therefore report every fault at once.
     */
    char said[512];
    snprintf(said, sizeof said, "statics: station %d port %d: %s",
             w->station, w->port, what);
    sora_stop_now(NULL, SORA_EXIT_BAD_CALL, said);
}
/* }}} */

/* {{{ the escape table, and the two routines that share it — issue 408 */
/*
 * **One table, two directions**, so the writer and the reader cannot
 * disagree about what a backslash introduces.
 *
 * Before this, a string was written out raw and read back by
 * searching for the next quote. A value holding a quote ended its own
 * text early; a value holding a tab or a newline produced a map file
 * with a line break inside a line; a byte above 0x7F went out as
 * whatever the reader's locale made of it. All three are values the
 * engine will happily hold and could not write down — which makes it
 * a correctness hole rather than a matter of polish, because the dump
 * claims to round-trip.
 *
 * **Five named escapes and a hexadecimal form**, and the split is
 * deliberate. The five are the ones a person reading a map should see
 * spelled the way they already know them. Everything else
 * unprintable, and everything from 0x80 up, goes as `\xNN` — because
 * a byte with no agreed spelling is better shown as its number than
 * as a character somebody's terminal invented.
 *
 * **The hexadecimal form is exactly two digits, always.** C's own
 * `\x` consumes as many as it can find, so `"\x41" "2"` and
 * `"\x412"` mean different things and one of them is a compile
 * error — a footgun worth not inheriting. Two digits covers every
 * byte and never runs on into the next character.
 */
static const struct { char spelled; unsigned char is; } escapes[] = {
    { '"',  '"'  },
    { '\\', '\\' },
    { 'n',  '\n' },
    { 't',  '\t' },
    { 'r',  '\r' },
};

/* {{{ static const char *read_quoted() */
/*
 * The other direction, reading from `p` — which must be sitting on
 * the opening quote — into at most `room` bytes, and saying how many
 * arrived. Returns the position just past the closing quote.
 *
 * `what` names the thing being read, so a refusal can say which field
 * or which port was wrong rather than only that something was.
 */
static const char *read_quoted(const char *p, char *out, int room,
                               int *len_out, const char *what,
                               const where_t *w)
{
    char note[192];
    if (*p != '"') {
        snprintf(note, sizeof note, "%s wants a quoted string", what);
        die_static(w, note);
    }
    p++;

    int len = 0;
    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            if (!*p) {
                snprintf(note, sizeof note,
                         "%s: a backslash at the end of the text", what);
                die_static(w, note);
            }
            char spelled = *p++;
            int named = 0;
            for (size_t e = 0; e < sizeof escapes / sizeof *escapes; e++)
                if (spelled == escapes[e].spelled) {
                    c = escapes[e].is;
                    named = 1;
                    break;
                }
            if (!named) {
                if (spelled != 'x') {
                    snprintf(note, sizeof note,
                             "%s: '\\%c' is not an escape this format has",
                             what, spelled);
                    die_static(w, note);
                }
                int value = 0;
                for (int d = 0; d < 2; d++) {
                    char h = *p++;
                    int digit;
                    if (h >= '0' && h <= '9')      digit = h - '0';
                    else if (h >= 'a' && h <= 'f') digit = h - 'a' + 10;
                    else if (h >= 'A' && h <= 'F') digit = h - 'A' + 10;
                    else {
                        snprintf(note, sizeof note,
                                 "%s: '\\x' wants exactly two hexadecimal "
                                 "digits", what);
                        die_static(w, note);
                        return p;
                    }
                    value = value * 16 + digit;
                }
                c = (unsigned char)value;
            }
        }
        if (len >= room) {
            snprintf(note, sizeof note,
                     "%s holds %d characters and more were given",
                     what, room);
            die_static(w, note);
        }
        out[len++] = (char)c;
    }

    if (*p != '"') {
        snprintf(note, sizeof note, "%s: unterminated string", what);
        die_static(w, note);
    }
    *len_out = len;
    return p + 1;
}
/* }}} */
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

/* {{{ classify_port() */
/*
 * What kind of thing this port holds, and — when it is a struct —
 * where its reader and writer are.
 *
 * **The struct is not searched for.** The port was handed the address
 * of its pair at placement, because the placement function knew the
 * type concretely (issues 311b, 408); this used to scan every emitted
 * struct table looking for a matching name.
 *
 * The primitives are still told apart by their spelling, and that is
 * a different act from comparing two types: a wire is legal on
 * **width** alone, because two boxes may spell one shape differently
 * and mean the same data (issue 309). Nothing here compares one type
 * against another. It asks how to turn text into bytes, which needs
 * to know whether those bytes are a number, and which kind.
 */
static type_class_t classify_port(const in_port_t *sl,
                                  const struct_text_t **out_struct)
{
    const char *tn = sl->type_name ? sl->type_name : "";
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

    if (sl->text) {
        if (out_struct) *out_struct = sl->text;
        return TN_STRUCT;
    }
    return TN_UNKNOWN;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Number and string writing, width by width.                         */
/* ------------------------------------------------------------------ */

/* {{{ write_integer() / write_unsigned() / write_float() */
static void write_integer(long long v, unsigned char *out, int size,
                          const where_t *w)
{
    /* Each width is written through its own type so sign extension
     * and truncation are the compiler's, not arithmetic here. */
    switch (size) {
    case 1: { signed char x = (signed char)v;  memcpy(out, &x, 1); break; }
    case 2: { short x = (short)v;              memcpy(out, &x, 2); break; }
    case 4: { int x = (int)v;                  memcpy(out, &x, 4); break; }
    case 8: { long long x = v;                 memcpy(out, &x, 8); break; }
    default: die_static(w, "an integer field of a width the reader does not know");
    }
}

static void write_unsigned(unsigned long long v, unsigned char *out, int size,
                           const where_t *w)
{
    switch (size) {
    case 1: { unsigned char x = (unsigned char)v;   memcpy(out, &x, 1); break; }
    case 2: { unsigned short x = (unsigned short)v; memcpy(out, &x, 2); break; }
    case 4: { unsigned x = (unsigned)v;             memcpy(out, &x, 4); break; }
    case 8: { unsigned long long x = v;             memcpy(out, &x, 8); break; }
    default: die_static(w, "an unsigned field of a width the reader does not know");
    }
}

static void write_float(double v, unsigned char *out, int size,
                        const where_t *w)
{
    if (size == 4) { float x = (float)v; memcpy(out, &x, 4); }
    else if (size == 8) { memcpy(out, &v, 8); }
    else die_static(w, "a floating field of a width the reader does not know");
}
/* }}} */

/* {{{ read_integer() / read_unsigned() / read_float() */
/*
 * The other direction, for turning a value back into text. Each width
 * is read through its own type for the same reason it is written
 * through one: sign extension is the compiler's job, and doing it by
 * hand is how a negative number becomes a very large positive one.
 */
static long long read_integer(const unsigned char *p, int size,
                              const where_t *w)
{
    switch (size) {
    case 1: { signed char x;  memcpy(&x, p, 1); return x; }
    case 2: { short x;        memcpy(&x, p, 2); return x; }
    case 4: { int x;          memcpy(&x, p, 4); return x; }
    case 8: { long long x;    memcpy(&x, p, 8); return x; }
    default: die_static(w, "an integer field of a width the writer does not know");
    }
    return 0;
}

static unsigned long long read_unsigned(const unsigned char *p, int size,
                                        const where_t *w)
{
    switch (size) {
    case 1: { unsigned char x;      memcpy(&x, p, 1); return x; }
    case 2: { unsigned short x;     memcpy(&x, p, 2); return x; }
    case 4: { unsigned x;           memcpy(&x, p, 4); return x; }
    case 8: { unsigned long long x; memcpy(&x, p, 8); return x; }
    default: die_static(w, "an unsigned field of a width the writer does not know");
    }
    return 0;
}

static double read_float(const unsigned char *p, int size, const where_t *w)
{
    if (size == 4) { float x;  memcpy(&x, p, 4); return x; }
    if (size == 8) { double x; memcpy(&x, p, 8); return x; }
    die_static(w, "a floating field of a width the writer does not know");
    return 0;
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


/* ------------------------------------------------------------------ */
/* Turning a value back into words (issue 401). The exact mirror of   */
/* the reader above, walking the same field table the other way.      */
/* ------------------------------------------------------------------ */

/* {{{ struct textbuf / tb_addf() */
/*
 * A growing piece of text that never overflows and always reports how
 * much it wanted. `used` counts characters the caller asked for, which
 * may exceed the room available — so a caller that cares can tell it
 * was cut short and ask again with a bigger buffer, the same contract
 * snprintf offers.
 */
typedef sora_textbuf_t textbuf_t;

static void tb_addf(textbuf_t *tb, const char *fmt, ...);

static void tb_addf(textbuf_t *tb, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    /* Where this write starts, and how much of the buffer is left for
     * it. Once `used` has passed `room` there is no space at all, and
     * the write is counted rather than made. */
    int left = tb->used < tb->room ? tb->room - tb->used : 0;
    char *at = tb->out + (tb->used < tb->room ? tb->used : tb->room);
    int wanted = vsnprintf(left > 0 ? at : NULL, (size_t)left, fmt, args);

    va_end(args);
    if (wanted > 0)
        tb->used += wanted;
}
/* }}} */

/* {{{ static void write_quoted() */
/*
 * `len` bytes, written as a quoted string with everything escaped
 * that has to be. The length is given rather than found, because a
 * value may legitimately contain a zero byte and a char array field
 * filled exactly to its width has no room for a terminator.
 */
static void write_quoted(textbuf_t *tb, const char *bytes, int len)
{
    tb_addf(tb, "\"");
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)bytes[i];
        int named = 0;
        for (size_t e = 0; e < sizeof escapes / sizeof *escapes; e++)
            if (c == escapes[e].is) {
                tb_addf(tb, "\\%c", escapes[e].spelled);
                named = 1;
                break;
            }
        if (named)
            continue;
        if (c < 0x20 || c >= 0x7F)
            tb_addf(tb, "\\x%02x", c);
        else
            tb_addf(tb, "%c", (char)c);
    }
    tb_addf(tb, "\"");
}
/* }}} */


/* {{{ float_text() */
/*
 * Enough digits that reading the text back gives the same value.
 * Seventeen significant digits round-trip a double and nine round-trip
 * a float; fewer would make a dump that loads into a program slightly
 * different from the one dumped, which is exactly the failure the
 * round trip exists to catch.
 *
 * A whole number comes out without a decimal point — 2 rather than
 * 2.0 — and that is fine, because what reads it back is strtod, which
 * does not care. The map file format has never carried types.
 */
static void float_text(textbuf_t *tb, double v, int size)
{
    if (size == 4)
        tb_addf(tb, "%.9g", v);
    else
        tb_addf(tb, "%.17g", v);
}
/* }}} */


/* {{{ the helpers a generated reader and writer call — issue 408 */
/*
 * **One grammar, shared; one routine per struct, emitted.**
 *
 * A struct's reader and writer are generated now, reaching each field
 * by name with its size a `sizeof` at the point of use. What is *not*
 * generated is any of this: the escape rules, the number widths, the
 * refusals. Those are the same for every struct, so emitting them per
 * type would be one grammar written N times and N places for it to
 * drift.
 *
 * So the shape is the opposite of what it looks like at first glance:
 * the part that varies by type is generated, and the part that does
 * not is written once, here, and called.
 */
const char *sora_text_expect(const char *p, char c, const sora_where_t *w,
                             const char *what)
{
    p = skip_ws(p);
    if (*p != c) {
        char note[192];
        snprintf(note, sizeof note, "expected '%c' %s", c, what);
        die_static(w, note);
    }
    return skip_ws(p + 1);
}

const char *sora_text_signed(const char *p, void *out, int size,
                             const sora_where_t *w, const char *field)
{
    p = skip_ws(p);
    char *end;
    long long v = strtoll(p, &end, 0);
    if (end == p) {
        char note[192];
        snprintf(note, sizeof note, "field '%s' wants a number", field);
        die_static(w, note);
    }
    write_integer(v, (unsigned char *)out, size, w);
    return skip_ws(end);
}

const char *sora_text_unsigned(const char *p, void *out, int size,
                               const sora_where_t *w, const char *field)
{
    p = skip_ws(p);
    char *end;
    unsigned long long v = strtoull(p, &end, 0);
    if (end == p) {
        char note[192];
        snprintf(note, sizeof note, "field '%s' wants a number", field);
        die_static(w, note);
    }
    write_unsigned(v, (unsigned char *)out, size, w);
    return skip_ws(end);
}

const char *sora_text_floating(const char *p, void *out, int size,
                               const sora_where_t *w, const char *field)
{
    p = skip_ws(p);
    char *end;
    double v = strtod(p, &end);
    if (end == p) {
        char note[192];
        snprintf(note, sizeof note, "field '%s' wants a number", field);
        die_static(w, note);
    }
    write_float(v, (unsigned char *)out, size, w);
    return skip_ws(end);
}

const char *sora_text_chars(const char *p, char *out, int room,
                            const sora_where_t *w, const char *field)
{
    char note[192];
    snprintf(note, sizeof note, "field '%s'", field);

    p = skip_ws(p);
    int len = 0;
    p = read_quoted(p, out, room, &len, note, w);
    /* Zero-padded to the full width rather than only terminated, so
     * that two structs holding the same text are the same bytes and a
     * comparison over raw bytes means what it looks like it means. */
    for (int i = len; i < room; i++)
        out[i] = '\0';
    return skip_ws(p);
}

void sora_text_put(sora_textbuf_t *tb, const char *literal)
{
    tb_addf(tb, "%s", literal);
}

void sora_text_put_signed(sora_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    tb_addf(tb, "%lld", read_integer((const unsigned char *)bytes, size, &w));
}

void sora_text_put_unsigned(sora_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    tb_addf(tb, "%llu", read_unsigned((const unsigned char *)bytes, size, &w));
}

void sora_text_put_floating(sora_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    float_text(tb, read_float((const unsigned char *)bytes, size, &w), size);
}

void sora_text_put_chars(sora_textbuf_t *tb, const char *chars, int room)
{
    /* Bounded by the array rather than trusted to a terminator,
     * because a field filled exactly to its width has no room for
     * one. */
    int len = 0;
    while (len < room && chars[len])
        len++;
    write_quoted(tb, chars, len);
}
/* }}} */

/* {{{ static void value_text() */
/*
 * One value of this port's type, written down. The bytes are given
 * rather than taken from the port, because two different things are
 * written with the same grammar: the constant a static port holds,
 * and each value waiting in a ring buffer when a running program is
 * captured (issue 712).
 *
 * The port is still needed — it says what shape the bytes are — but
 * not as the place the bytes come from.
 */
static void value_text(const in_port_t *sl, const void *bytes,
                       const char *string, textbuf_t *tb)
{
    where_t w = { -1, -1 };   /* the port is the caller's to name here */
    {
        const struct_text_t *si = NULL;
        switch (classify_port(sl, &si)) {
        case TN_INT:
            tb_addf(tb, "%lld", read_integer(bytes, sl->elem_size, &w));
            break;
        case TN_UINT:
            tb_addf(tb, "%llu", read_unsigned(bytes, sl->elem_size, &w));
            break;
        case TN_FLOAT:
            float_text(tb, read_float(bytes, sl->elem_size, &w),
                       sl->elem_size);
            break;
        case TN_STRING: {
            /* The value is a pointer; the characters live wherever
             * whoever produced them put them. A constant's are the
             * port's own, which is what makes handing the pointer out
             * sound; a queued one's belong to whatever delivered it.
             * Escaped on the way out (issue 408), so text holding a
             * quote does not end its own line early. */
            const char *str = string;
            if (!str) {
                const char *from_bytes = NULL;
                memcpy(&from_bytes, bytes, sizeof from_bytes);
                str = from_bytes;
            }
            write_quoted(tb, str ? str : "", str ? (int)strlen(str) : 0);
            break;
        }
        case TN_STRUCT:
            si->write(bytes, tb);
            break;
        default:
            tb_addf(tb, "?");
            break;
        }
    }
}
/* }}} */

/* {{{ in_port_constant_text() */
static int in_port_constant_text(const in_port_t *sl, char *out, int room)
{
    textbuf_t tb = { out, room, 0 };
    if (room > 0)
        out[0] = 0;

    if (!sl->constant_set)
        tb_addf(&tb, "?");
    else
        value_text(sl, sl->constant, sl->constant_string, &tb);

    /* vsnprintf terminates whatever it wrote; an empty buffer had
     * nowhere to be terminated and was handled above. */
    if (room > 0 && tb.used >= room)
        out[room - 1] = 0;
    return tb.used;
}
/* }}} */

/* {{{ in_port_waiting_text() — issue 712 */
/*
 * **Every value waiting in this port's buffer, written down**, so a
 * running program can be put on disk and picked up again rather than
 * only described.
 *
 * Comma-separated, in slot order, which is **not** an order the
 * engine promises anywhere: the guarantees page says plainly that
 * nothing is promised about the order values leave a port, because a
 * port has no head and no tail — a reader takes any ready slot near a
 * hint. So this writes them in the order they are stored, and a
 * revival delivers them back in that order, which is exactly as
 * faithful as the engine itself is. Promising more would be inventing
 * a guarantee at the moment of writing a file.
 *
 * Returns how many characters it wanted, in the same contract the
 * constant writer offers, so a caller asks for the length and then
 * writes.
 */
static int in_port_waiting_text(const in_port_t *sl, char *out, int room)
{
    textbuf_t tb = { out, room, 0 };
    if (room > 0)
        out[0] = 0;

    int written = 0;
    int capacity = atomic_load(&sl->capacity);
    for (int i = 0; i < capacity; i++) {
        void *slot = in_port_slot(sl, i);
        if (!slot || slot_state_at(slot, sl->elem_size) != SLOT_READY)
            continue;
        if (written++)
            tb_addf(&tb, ", ");
        /* A queued string's characters are wherever the deliverer put
         * them, so there is no second pointer to consult; the bytes
         * in the slot are the pointer. */
        value_text(sl, slot, NULL, &tb);
    }

    if (room > 0 && tb.used >= room)
        out[room - 1] = 0;
    return written ? tb.used : 0;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Giving a port a constant, and changing one.                        */
/* ------------------------------------------------------------------ */

/* {{{ in_port_constant_free() */
static void in_port_constant_free(in_port_t *sl)
{
    free(sl->constant);
    free(sl->constant_string);
    sl->constant = NULL;
    sl->constant_string = NULL;
    sl->constant_set = 0;
}
/* }}} */

/* {{{ port_text_to_bytes() */
/*
 * **Text into the bytes one port's type wants**, which is the one
 * thing this file knows how to do and the reason two very different
 * callers share it.
 *
 * A constant written in a map file and an argument typed on a command
 * line are the same problem: somebody wrote a value down as
 * characters, and the layout it has to become is a fact the compiler
 * computed and the generator recorded. Pointing this at an argument
 * list instead of at a statics line is the same code, the same
 * offsets, and the same messages naming the field that was wrong
 * (issue 213).
 *
 * `into` is `elem_size` bytes the caller owns. `owned_string` comes
 * back non-null when the port is a string port, holding characters
 * the caller must keep alive for as long as anything can read the
 * pointer that was written into `into` — because a string value *is*
 * that pointer, and freeing what it points at is freeing something a
 * box may still be looking at.
 */
static void port_text_to_bytes_ending(const in_port_t *sl, const char *text,
                                      unsigned char *into,
                                      char **owned_string,
                                      const where_t *w,
                                      const char **end);

static void port_text_to_bytes(const in_port_t *sl, const char *text,
                               unsigned char *into, char **owned_string,
                               const where_t *w)
{
    port_text_to_bytes_ending(sl, text, into, owned_string, w, NULL);
}


/*
 * The same reading, saying where it stopped. A list of waiting values
 * is comma separated and a struct value has commas inside it, so the
 * only way to find the separator is to read one value and see where
 * it ended (issue 712). With `end` null this behaves as it always
 * did: whatever follows the value is trailing text and a fault.
 */
static void port_text_to_bytes_ending(const in_port_t *sl, const char *text,
                                      unsigned char *into,
                                      char **owned_string,
                                      const where_t *w,
                                      const char **end)
{
    unsigned char *fresh = into;
    char *fresh_string = NULL;

    const struct_text_t *si = NULL;
    switch (classify_port(sl, &si)) {
    case TN_INT: {
        char *stop;
        long long v = strtoll(text, &stop, 0);
        if (stop == text)
            die_static(w, "an integer port wants a number");
        write_integer(v, fresh, sl->elem_size, w);
        if (end) *end = stop;
        break;
    }
    case TN_UINT: {
        char *stop;
        unsigned long long v = strtoull(text, &stop, 0);
        if (stop == text)
            die_static(w, "an unsigned port wants a number");
        write_unsigned(v, fresh, sl->elem_size, w);
        if (end) *end = stop;
        break;
    }
    case TN_FLOAT: {
        char *stop;
        double v = strtod(text, &stop);
        if (stop == text)
            die_static(w, "a floating port wants a number");
        write_float(v, fresh, sl->elem_size, w);
        if (end) *end = stop;
        break;
    }
    case TN_STRING: {
        /* The claimed value is a pointer; the characters live on the
         * port for the life of the map, which is what makes handing
         * the pointer to a box sound.
         *
         * **Quoted text goes through the shared escape routines**
         * (issue 408). Unquoted text is taken as itself, which is what
         * lets somebody write `in 0 config.txt` without ceremony — and
         * is why a value that needs escaping has to be quoted, because
         * an unquoted backslash is a backslash. */
        int len;
        if (*text == '"') {
            int room = (int)strlen(text);
            fresh_string = malloc((size_t)room + 1);
            if (!fresh_string)
                die_static(w, "out of memory for string storage");
            const char *stop = read_quoted(text, fresh_string, room, &len,
                                           "a string constant", w);
            if (end) *end = stop;
        } else {
            len = (int)strlen(text);
            fresh_string = malloc((size_t)len + 1);
            if (!fresh_string)
                die_static(w, "out of memory for string storage");
            memcpy(fresh_string, text, (size_t)len);
            /* Unquoted text runs to the end of what it was given, so
             * it can only be the last value — which is why a list of
             * waiting strings has to quote every one of them. */
            if (end) *end = text + len;
        }
        fresh_string[len] = 0;
        if (sl->elem_size != (int)sizeof(const char *))
            die_static(w, "a string port that is not pointer-sized");
        memcpy(fresh, &fresh_string, sizeof fresh_string);
        break;
    }
    case TN_STRUCT: {
        if (si->size != sl->elem_size)
            die_static(w, "struct size disagrees with the port");
        const char *after = si->read(text, fresh, w);
        if (end)
            *end = after;
        else if (*skip_ws(after) != 0)
            die_static(w, "trailing text after the struct value");
        break;
    }
    default:
        die_static(w, "the port's type is not one the reader knows");
    }

    *owned_string = fresh_string;
}
/* }}} */

/* {{{ map_in_port_static_text() */
void map_in_port_static_text(map_t *m, int station, int port, const char *text)
{
    where_t w = { station, port };

    if (station < 0 || station >= m->n_stations)
        die_static(&w, "giving a constant to a station outside the table");
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        die_static(&w, "giving a constant to a port the box does not have");
    in_port_t *sl = &s->in_ports[port];
    if (!sl->type_name)
        die_static(&w,
                   "the port has no declared type — a constant needs a station "
                   "placed by name, so the text knows what shape to become");
    if (!text || !*text)
        die_static(&w, "a constant with no text");

    /* Parsed into scratch first and copied in under the mutex, so a
     * malformed value never half-overwrites a working one and a claim
     * running concurrently never sees a value mid-parse. The parse is
     * the part that can fail; the install is the part that must not be
     * interrupted, and keeping them apart is what lets both be true. */
    unsigned char *fresh = calloc(1, (size_t)sl->elem_size);
    if (!fresh)
        die_static(&w, "out of memory parsing a constant");
    char *fresh_string = NULL;
    port_text_to_bytes(sl, text, fresh, &fresh_string, &w);

    /* One of the four rare structural operations (issue 210): the
     * install and the tag together, under the station's mutex, so no
     * readiness walk and no claim sees a port mid-change. */
    pthread_mutex_lock(&s->mutex);
    char *old_string = sl->constant_string;
    memcpy(sl->constant, fresh, (size_t)sl->elem_size);
    sl->constant_string = fresh_string;
    sl->constant_set = 1;
    sl->kind = IN_PORT_STATIC;
    pthread_mutex_unlock(&s->mutex);

    free(fresh);
    free(old_string);

    /* A port that was the last one missing is no longer missing
     * (issue 210). This is what replaced the pull path, and it is one
     * addition rather than a subsystem: a chain of stations wired
     * through static ports becomes a recalculation graph, and
     * construction's own writes are what start a program.
     *
     * Only once the pool exists, because before that there is nowhere
     * to push and the loader is still assembling — the seed sweep is
     * what starts a freshly loaded map, deliberately and once. */
    if (m->pool)
        map_station_start_while_ready(m, station);
}
/* }}} */

/* {{{ map_deliver_argument_text() */
/*
 * **An argument written as text**, turned into the bytes the port
 * wants and delivered through the ordinary door (issue 213).
 *
 * This is the constant reader pointed somewhere else. Somebody typing
 * `{ 5, 2.0, "hey" }` on a command line and somebody writing it in a
 * map file are doing the same thing, so struct arguments in brace
 * syntax come along for free, with the same compiler-computed offsets
 * and the same messages naming the field that was wrong.
 *
 * **A string argument leaks, deliberately.** The value delivered for
 * a string port *is* a pointer, and whatever it points at has to
 * outlive every box that might read it — which is the whole run.
 * Freeing it would be freeing something a box may still be looking
 * at. One allocation per argument, released when the process is, is
 * the honest shape: a command line lives as long as the program does.
 */
const char *map_deliver_argument_text(map_t *m, int station, int port,
                                      const char *text)
{
    static _Thread_local char said[256];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said, "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    in_port_t *sl = &s->in_ports[port];
    if (!sl->type_name) {
        snprintf(said, sizeof said,
                 "station %d port %d has no declared type, so text has no "
                 "shape to become", station, port);
        return said;
    }
    if (!text) {
        snprintf(said, sizeof said, "an argument with no text");
        return said;
    }

    where_t w = { station, port };
    unsigned char *bytes = calloc(1, (size_t)sl->elem_size);
    if (!bytes)
        return "out of memory parsing an argument";

    char *owned = NULL;
    port_text_to_bytes(sl, text, bytes, &owned, &w);

    const char *no = map_deliver_argument(m, station, port, bytes,
                                          sl->elem_size);
    free(bytes);
    /* `owned` is not freed; see above. */
    return no;
}
/* }}} */

/* {{{ map_deliver_command_line() */
/*
 * **The command line, delivered into a program's entrances** (issue
 * 213).
 *
 * A program's arguments are the input ports of the stations it
 * declared as entrances, taken in station order and then in port
 * order. A program with two entrances of two ports each takes four
 * arguments, and which is which is a fact about the program that a
 * person reading its map file can see.
 *
 * **It holds a standing promise while it delivers and drops it
 * afterwards**, which is the rule the pool has always had for
 * anything outside the workers: without it the program can decide it
 * has finished between two arguments. Dropping it afterwards is what
 * lets a program whose arguments are all in actually end.
 *
 * A count that does not match is refused rather than partly
 * delivered, and the refusal says how many the program wanted. Half a
 * command line is a program that waits forever for the rest, which is
 * a worse way to learn about a typo than being told.
 */
const char *map_deliver_command_line(map_t *m, int argc, char **argv)
{
    static _Thread_local char said[256];

    int wanted = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (s->call && s->door == DOOR_IN)
            wanted += s->n_in_ports;
    }

    int given = argc > 0 ? argc - 1 : 0;
    if (given != wanted) {
        snprintf(said, sizeof said,
                 "this program takes %d argument%s and was given %d",
                 wanted, wanted == 1 ? "" : "s", given);
        return said;
    }

    /*
     * **Too late is said out loud rather than achieved quietly.**
     *
     * A program that seeds nothing has an empty queue, and until
     * somebody holds a standing promise it also has nobody promising
     * anything — so between the workers being released and the first
     * argument arriving, the last sleeper correctly decides the
     * program is over. Everything delivered afterwards is a task that
     * will never run, and the symptom is a program that did nothing
     * for no visible reason.
     *
     * The rule that prevents it is the pool's own and has not
     * changed: make the promise before opening the gate. This is not
     * a second mechanism for it — a registration taken here could not
     * close a window that opened before this was called. It is how
     * somebody finds out they left it open.
     */
    if (m->pool && pool_finished(m->pool)) {
        snprintf(said, sizeof said,
                 "this program had already finished before its arguments "
                 "arrived — something outside has to hold a standing "
                 "promise from before the workers are released until the "
                 "last argument is in");
        return said;
    }

    int taken = 0;
    const char *no = NULL;
    for (int i = 0; i < m->n_stations && !no; i++) {
        station_t *s = map_station(m, i);
        if (!s->call || s->door != DOOR_IN)
            continue;
        for (int j = 0; j < s->n_in_ports && !no; j++)
            no = map_deliver_argument_text(m, i, j, argv[1 + taken++]);
    }

    return no;
}
/* }}} */

/* {{{ map_in_port_queue_text() — issue 712 */
/*
 * **Values put back into a buffer**, from the text a capture wrote.
 *
 * This is the revival half of writing a running program down. The
 * text is what stood between the brackets on an `in` line: values
 * separated by commas, read one at a time because a struct value has
 * commas inside it and only reading one can tell an outer comma from
 * an inner one.
 *
 * **Each value goes in through the ordinary delivery**, which is what
 * makes a revival faithful rather than approximate. Delivering runs
 * the readiness check, so a station whose ports refill becomes ready
 * exactly as it would have, and the tasks that form are the tasks
 * that would have formed. Nothing is reconstructed; the same door is
 * used.
 *
 * **The order is the order the text gives**, which the engine does
 * not promise means anything — a port has no head and no tail. A
 * capture writes slots as it finds them and this puts them back that
 * way, which is exactly as faithful as the engine is about order.
 *
 * Returns NULL, or a refusal naming what went wrong.
 */
const char *map_in_port_queue_text(map_t *m, int station, int port,
                                   const char *text)
{
    static _Thread_local char said[256];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said, "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    in_port_t *sl = &s->in_ports[port];
    if (!sl->type_name) {
        snprintf(said, sizeof said,
                 "station %d port %d has no declared type, so waiting values "
                 "have no shape to become", station, port);
        return said;
    }
    if (atomic_load(&sl->kind) != IN_PORT_RING) {
        snprintf(said, sizeof said,
                 "station %d port %d is not a buffer, so nothing can be "
                 "waiting in it", station, port);
        return said;
    }
    if (!text)
        return "waiting values with no text";

    where_t w = { station, port };
    const char *p = text;

    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (!*p)
            break;

        unsigned char *bytes = calloc(1, (size_t)sl->elem_size);
        if (!bytes)
            return "out of memory reading a waiting value";

        char *owned = NULL;
        const char *end = p;
        port_text_to_bytes_ending(sl, p, bytes, &owned, &w, &end);

        /* Through the ordinary door, so the readiness check runs and
         * the station wakes exactly as it would have. `owned` is a
         * string's characters and is not freed, for the same reason a
         * delivered argument's are not: the value handed on is a
         * pointer, and whatever it points at has to outlive every box
         * that might read it.
         *
         * **Nothing is checked about whether it landed**, because a
         * buffer with nowhere to put a value grows a page rather than
         * refusing one — so a delivery cannot fail for want of room.
         * What comes back says whether a *task* became due, which is
         * a fact about the station and not about this value. Reading
         * it as success was the first thing written here and it was
         * wrong for every value that did not complete a station,
         * which is most of the values a capture holds.
         */
        map_deliver_value(m, station, port, bytes);
        free(bytes);

        p = end;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
        if (*p == ',') {
            p++;
            continue;
        }
        if (!*p)
            break;
        snprintf(said, sizeof said,
                 "station %d port %d: expected ',' or the end of the waiting "
                 "values, and found '%c'", station, port, *p);
        return said;
    }
    return NULL;
}
/* }}} */

/* {{{ static_write_under_lock() */
/*
 * The copy itself, as something the delivery path can be asked to do
 * while it holds the station's mutex (issue 210d). It exists as a
 * separate function only because that is how the work is handed over.
 *
 * **It does not compare, and must not.** Writing the value a port
 * already holds still counts as a write, so the station is asked to
 * run again. That is not an oversight to be optimized away later:
 *
 * A write is a *statement* — the value is now this — rather than a
 * report of a difference. In a graph of stations wired through
 * constants, "recompute with this" is what the caller asked for, and
 * whether the bytes happen to match what was there is a fact about the
 * previous value, which the caller said nothing about. Skipping would
 * make the same call do two different things depending on history.
 *
 * And a comparison would not even be reliable. A struct arrives here
 * as raw bytes, holes included, so two writes meaning the same value
 * can differ in padding the author never touched — and would then be
 * treated as a change, while a genuine change that happened to leave
 * the compared bytes alone would not be. A test that is wrong in both
 * directions is worse than no test.
 */
typedef struct {
    in_port_t  *port;
    const void *bytes;
    int         size;
} static_write_t;

static void static_write_under_lock(void *ctx)
{
    static_write_t *j = ctx;
    memcpy(j->port->constant, j->bytes, (size_t)j->size);
}
/* }}} */

/* {{{ map_in_port_static_write() */
void map_in_port_static_write(map_t *m, int station, int port,
                           const void *bytes, int size)
{
    where_t w = { station, port };

    if (station < 0 || station >= m->n_stations)
        die_static(&w, "writing to a station outside the table");
    station_t *s = map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        die_static(&w, "writing to a port the box does not have");
    in_port_t *sl = &s->in_ports[port];
    if (!sl->constant_set)
        die_static(&w, "writing to a port that has never held a constant — "
                       "give it one as text first, so its shape is known");
    if (size != sl->elem_size)
        die_static(&w, "writing a value of the wrong size for this port");

    /* The copy happens under the station's own mutex — the one the
     * claim already takes. A struct half-overwritten while a claim is
     * copying it would yield fields from two different worlds, which
     * for anything wider than a machine word is not theoretical.
     *
     * **And it happens inside the same hold as the readiness check**
     * (issue 210d). Writing does not consume anything, so a station
     * that could already run runs again — which is how a value
     * computed once propagates through everything downstream of it.
     * Doing the copy and the check as two acquisitions would leave a
     * gap between the value changing and the question being asked, so
     * the copy is handed to the check to perform.
     *
     * Without a pool there is nothing to start, and the copy still has
     * to happen — a map being built is written into before it runs. */
    static_write_t job = { sl, bytes, size };
    if (m->pool) {
        map_station_start_after(m, station, static_write_under_lock, &job);
        /*
         * **And then keep asking** (issue 712). The call above did the
         * write and one readiness check inside a single lock hold,
         * which is what closes the gap between the value changing and
         * the question being asked. But one check starts one task, and
         * a station may have a buffer with work stacked up in it that
         * was waiting for exactly this constant — thirty values on one
         * port and nothing on the other. Asking once would start one
         * of them and strand twenty-nine.
         *
         * A station with no buffer is not asked again, because the
         * call above already started it and nothing there accumulates.
         */
        map_station_keep_starting(m, station);
    } else {
        pthread_mutex_lock(&s->mutex);
        static_write_under_lock(&job);
        pthread_mutex_unlock(&s->mutex);
    }
}
/* }}} */

/* ==================================================================
 *
 * 042 — reading a description
 *
 * Was src/042-loader.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/042-loader.c"
/*
 * 042-loader.c — the moment the two halves of a program meet.
 *
 * What this is: the loader (issues 602–605). The binary holds a
 * list of boxes and no map; the file holds a map and no code.
 * This file walks the parsed description twice — create everything,
 * then connect everything — checks every wire against the emitted sizes
 * at the first moment both ends are known, validates what only the
 * whole map can show, and seeds the first tasks.
 *
 * How it does it, in general terms: declaration order must not
 * matter (an arrow may point at a station declared further down),
 * which is the entire reason there are two passes. The name lookup
 * table lives only as long as loading. Validation failures are
 * collected and printed together before stopping — someone fixing a
 * new map wants the whole list — and the error messages carry
 * station names and type names, because they are the surface a
 * person actually touches.
 */


/* A description on disk becomes a program by being compiled, which
 * is the same door a box source goes through (issue 311d). */



/* A box added while some earlier process ran; see 073-latebox.h. It
 * is declared here rather than included, because the loader needs one
 * function from that file and nothing else it offers. */
static const box_place_t *late_recover_box(const char *name);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ die_load() */
static void die_load(const char *path, int line, const char *station,
                     const char *what)
{
    /*
     * **Exit 65, meaning the input was malformed** (issue 106), rather
     * than aborting. A core dump says nothing about a mistyped box
     * name, and a shell script that wants to tell "the file was
     * wrong" from "the machine ran out of memory" could not, because
     * both arrived as the same abnormal death.
     */
    char said[768];
    if (station)
        snprintf(said, sizeof said, "map %s:%d: station '%s': %s",
                 path, line, station, what);
    else
        snprintf(said, sizeof said, "map %s:%d: %s", path, line, what);
    sora_stop_now(NULL, SORA_EXIT_BAD_FILE, said);
}
/* }}} */




/* {{{ static char *read_whole_file() */
/*
 * A description, as text. Small by nature — a description names
 * stations and wires, and a program with a thousand of either is
 * still a few tens of kilobytes — so it is read whole rather than
 * streamed, and the caller frees it.
 */
static char *read_whole_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        die_load(path, 0, NULL, "cannot be opened");

    if (fseek(f, 0, SEEK_END) != 0)
        die_load(path, 0, NULL, "cannot be measured");
    long n = ftell(f);
    if (n < 0)
        die_load(path, 0, NULL, "cannot be measured");
    rewind(f);

    char *text = malloc((size_t)n + 1);
    if (!text)
        die_load(path, 0, NULL, "out of memory reading a description");
    size_t got = fread(text, 1, (size_t)n, f);
    text[got] = '\0';
    fclose(f);
    return text;
}
/* }}} */

/* {{{ static int build_from_file() */
/*
 * **A description on disk becomes the calls it describes, and then
 * those calls are made** (issue 311d).
 *
 * Nothing here reads the description. It is handed to the compiler —
 * the same generator and the same C compiler the build used — which
 * turns it into a function that builds it, and that function is
 * called. So there is one way a description becomes a program, and
 * this is a caller of it rather than a second implementation.
 *
 * **What that costs is a compiler invocation**, roughly a tenth of a
 * second, where walking the description cost nothing. It is paid
 * deliberately: the alternative was keeping a second way to turn a
 * description into a program, which is exactly the thing this family
 * of changes exists to remove. A program that never reads a
 * description at run time never pays it, and a program built from its
 * own descriptions never reads one.
 *
 * **And the boxes are not compiled.** They are already here; what is
 * compiled is the description, and the code that comes back binds to
 * the station-builders this program published.
 */
static void build_from_file(map_t *m, const char *path, map_instance_t *out)
{
    char *text = read_whole_file(path);
    const map_build_t *built = late_compile_map(text);
    free(text);

    if (!built)
        die_load(path, 0, NULL, "could not be compiled into this program");

    if (!out) {
        built->build(m, NULL, 0);
        return;
    }

    /*
     * **The row says how many stations before anything is built**,
     * which is what lets the table be the right size on the first and
     * only build. Asking the built function would mean building, and
     * building twice would make two copies of the description.
     */
    out->count = built->n_stations;
    out->station = calloc((size_t)(out->count > 0 ? out->count : 1),
                          sizeof *out->station);
    if (!out->station)
        die_load(path, 0, NULL, "out of memory instantiating a map");

    built->build(m, out->station, out->count);
}
/* }}} */

/* {{{ map_load_file() */
/* {{{ static int marked_incomplete() */
/*
 * **An artifact that says it lost work** (issue 712). A capture taken
 * while workers were still inside boxes never got their results, and
 * it says so at the top rather than leaving it to be noticed.
 *
 * Read from the text here rather than from the description, because
 * the marker is a comment — the parser drops comments on the floor, as
 * it should, since a comment is by definition not part of what a file
 * says about a program. This is a fact about the *file*, and the file
 * is what asks it.
 */
static int marked_incomplete(const char *text)
{
    return strstr(text, "# INCOMPLETE CAPTURE") != NULL;
}
/* }}} */

static map_t *load_file(const char *path, int n_workers, int salvaging);

/* {{{ map_load_file() / map_load_salvage() */
/*
 * **Reading a description back is refused when it says it lost work**,
 * unless the caller asks for salvage (issue 712). A program picked up
 * from an incomplete capture is quietly missing results somebody
 * computed, and quietly is the part this engine refuses everywhere: a
 * fallback is a warning and a warning is an error.
 *
 * Salvaging is a different act, and having a different name for it is
 * the point — whoever calls it has said out loud that they know what
 * is missing.
 */
map_t *map_load_file(const char *path, int n_workers)
{
    return load_file(path, n_workers, 0);
}

map_t *map_load_salvage(const char *path, int n_workers)
{
    return load_file(path, n_workers, 1);
}

static map_t *load_file(const char *path, int n_workers, int salvaging)
{
    /*
     * An empty table, grown one station at a time as the description
     * is built (issue 211). It used to count the station lines and
     * allocate exactly that many, which meant reading a map was a
     * different act from adding a station to a running program — and
     * under one construction surface it should not be.
     */
    /* Asked of the file before anything is built from it, so a
     * refusal costs no compiler invocation and leaves nothing behind
     * to clean up. */
    if (!salvaging) {
        char *text = read_whole_file(path);
        int lossy = marked_incomplete(text);
        free(text);
        if (lossy)
            die_load(path, 0, NULL,
                     "says at the top that it is an incomplete capture — "
                     "work was still running when it was written and its "
                     "results were never delivered. Read it with the "
                     "salvage door if that is understood and wanted");
    }

    map_t *m = map_create_empty();

    build_from_file(m, path, NULL);

    /* The pool exists before the seed so the seed has somewhere to
     * push, but its workers stay parked until the caller releases —
     * the seeding window issue 102 built. */
    map_start(m, n_workers);

    /*
     * **Reading a file no longer validates or seeds; it asks for the
     * program to be brought up, the same as anybody else would**
     * (issue 212). The checks and the seed were the last thing the
     * loader could do that nothing else could, and with them moved
     * there is no state called *still loading* left for it to be in.
     */
    const char *no = map_bring_up(m);
    if (no)
        die_load(path, 0, NULL, no);

    /*
     * **Whether starting nothing is a fault is the caller's to say**,
     * and this caller says yes.
     *
     * Bringing a program up is repeatable, so seeding nothing is
     * perfectly ordinary — a program brought up, grown by one
     * station, and brought up again seeds nothing the second time and
     * should not be scolded for it. But a *file* somebody asked to be
     * run is a different promise: if no station can start without
     * waiting for a value, and no value can arrive because nothing is
     * running to send one, then the program does nothing at all, and
     * saying so is more use than starting it.
     *
     * **Unless the program has a declared entrance**, in which case
     * waiting is exactly what it is supposed to do (issue 213). This
     * refusal means "nothing can start and nothing can arrive, so
     * this program will do nothing at all" — and a declared entrance
     * is a station something outside delivers to, which makes the
     * second half of that false.
     */
    int has_entrance = 0;
    for (int i = 0; i < m->n_stations; i++)
        if (map_station(m, i)->door == DOOR_IN)
            has_entrance = 1;

    if (map_seed_count(m) == 0 && !has_entrance)
        die_load(path, 0, NULL,
                 "nothing to seed — every station waits for a buffered "
                 "value, so the map cannot ever start");

    return m;
}
/* }}} */

/* {{{ map_instantiate_file() */
/*
 * **Bring a description inside a program that already exists** (issue
 * 217) — the operation this whole file turns out to have been, with
 * the program fixed at "a fresh empty one".
 *
 * **It is instantiating a template, not merging two programs.** There
 * is no second running program being picked up and carried, no handle
 * that becomes invalid, no table stitched onto another table. There
 * is a *description* and there is a table with some number of
 * stations in it; this builds new stations for the description's
 * stations and wires them the way the description says. One
 * description can be instantiated as many times into one program as
 * anybody likes, with nothing shared between the copies — separate
 * stations, separate buffers, separate constants.
 *
 * **So no wire is rewritten.** The description says its third station
 * feeds its fifth; that becomes wherever the third landed feeding
 * wherever the fifth landed. Nothing that already exists is
 * renumbered, so the invariant this engine rests on — an index means
 * what it meant — is not approached, let alone bent.
 *
 * **Legal at any moment**, because every operation it is made of is:
 * adding a station, naming one, placing a box, configuring a port,
 * drawing a wire. A program with workers in flight gains a subgraph
 * the same way it gains a station.
 *
 * The caller gets a handle it can find the instance's doors through,
 * and **the doors are all it should want**. A parent wiring into an
 * interior station of an instance is reaching inside, which is the
 * thing the marks exist to stop happening by accident.
 */
map_instance_t map_instantiate_file(map_t *m, const char *path)
{
    /*
     * **Where the stations landed comes back from the built function
     * itself**, because nothing else can know. Adding a station hands
     * back a freed place before it grows the table, so a program that
     * has had removals gets whatever holes exist in whatever order,
     * and the parent wants the doors in the order the description
     * declared them rather than in table order.
     */
    map_instance_t in;
    build_from_file(m, path, &in);
    return in;
}
/* }}} */

/* {{{ map_instance_door() / map_instance_free() */
/*
 * **The nth station of this instance facing that way**, or -1.
 *
 * This is the whole of what a parent is entitled to know about
 * something it brought inside itself. It could reach any of the
 * instance's stations through the handle — the translation table is
 * right there — and doing so would be reaching inside a thing whose
 * author may rename or restructure anything that is not a door.
 *
 * A program may have several of each, so the nth rather than the
 * only. They come back in the order the description declared them,
 * which is the one order a description can be said to have.
 */
static int map_instance_door(map_t *m, const map_instance_t *in,
                             int facing, int nth)
{
    int seen = 0;
    for (int i = 0; i < in->count; i++) {
        station_t *s = map_station(m, in->station[i]);
        if (s->call && s->door == facing && seen++ == nth)
            return in->station[i];
    }
    return -1;
}

int map_instance_entrance(map_t *m, const map_instance_t *in, int nth)
{
    return map_instance_door(m, in, DOOR_IN, nth);
}

int map_instance_result(map_t *m, const map_instance_t *in, int nth)
{
    return map_instance_door(m, in, DOOR_OUT, nth);
}

/*
 * The handle goes; the stations stay. Nothing in the running program
 * refers to this — it was the reader's note to itself about where
 * things landed, and a parent keeps it only for as long as it is
 * still deciding what to wire.
 */
void map_instance_free(map_instance_t *in)
{
    free(in->station);
    in->station = NULL;
    in->count = 0;
}
/* }}} */

/* {{{ map_add_part() */
/*
 * **Adding a box and adding a map are one operation** (issue 217).
 *
 * A map is a list of boxes and the wiring between them; a box is a
 * list of one. That is the whole difference, and once it is said that
 * way the two stop being different acts — adding a map means walking
 * its list, instantiating each of its boxes and then connecting them
 * the way it says, and adding a box means walking a list of length
 * one and connecting nothing.
 *
 * **What comes back is the same kind of thing either way.** A part is
 * where values go in and where they come out. For a map those are the
 * stations it declared as doors. **For a single box they are the same
 * station**, because a box's own input ports are its way in and its
 * own output port is its way out — a box is a map of one station
 * whose doors are itself.
 *
 * That is what makes a handle safe to hand around without ever taking
 * it apart: everything that consumes one takes it whole.
 *
 * **Which kind it is, is resolved rather than guessed.** A box lives
 * in the binary and a description lives on disk, so both are looked
 * for. Finding both is refused as ambiguous rather than settled by an
 * order nobody can see; finding neither is refused naming both places
 * that were searched.
 */
const char *map_add_part(map_t *m, const char *what, map_part_t *out)
{
    static _Thread_local char said[512];

    if (!what || !*what)
        return "adding a part with no name";

    const box_place_t *box = box_place_find(what);
    FILE *described = fopen(what, "r");
    if (described)
        fclose(described);

    if (box && described) {
        snprintf(said, sizeof said,
                 "'%s' is both a box in this binary and a description on "
                 "disk — say which by using a path that is not also a box "
                 "name", what);
        return said;
    }

    if (box) {
        /* A list of one. Its doors are itself. */
        int at = map_add_station(m);
        if (at < 0)
            return "the station table would not grow";
        map_place_box(m, at, what, STATION_PLAIN);
        out->entrance = at;
        out->result = at;
        return NULL;
    }

    if (described) {
        map_instance_t in = map_instantiate_file(m, what);
        out->entrance = map_instance_entrance(m, &in, 0);
        out->result = map_instance_result(m, &in, 0);
        map_instance_free(&in);
        if (out->result < 0) {
            snprintf(said, sizeof said,
                     "'%s' declares no way out, so nothing can be taken "
                     "from it", what);
            return said;
        }
        return NULL;
    }

    snprintf(said, sizeof said,
             "'%s' is neither a box compiled into this program nor a "
             "description that can be read from disk", what);
    return said;
}
/* }}} */

/* {{{ map_connect_parts() */
/*
 * **A wire from one part's way out to another part's way in**, which
 * is the only wire a composing caller ever needs to draw (issue 217).
 *
 * For two single boxes this is the ordinary wire, because a box's
 * doors are itself. For two maps it crosses what used to be a seam
 * and there is nothing there to cross — after instantiation there are
 * stations with indices, the way there always were.
 *
 * The port numbers are the ones a wire has always had: which output
 * port of the producing station, and which input port of the
 * receiving one. A comparator's three outcomes are reachable this way
 * exactly as before.
 */
const char *map_connect_parts(map_t *m, map_part_t from, int from_port,
                              map_part_t to, int to_port)
{
    static _Thread_local char said[256];

    if (from.result < 0) {
        snprintf(said, sizeof said,
                 "wiring out of a part that has no way out");
        return said;
    }
    if (to.entrance < 0) {
        snprintf(said, sizeof said,
                 "wiring into a part that declares no way in — a program "
                 "that takes no arguments cannot be fed");
        return said;
    }
    return map_wire(m, from.result, from_port, to.entrance, to_port);
}
/* }}} */

/* {{{ map_seed_count() */
int map_seed_count(map_t *m)
{
    return m->seeded;
}
/* }}} */

/* ==================================================================
 *
 * 050 — reports and the observer
 *
 * Was src/050-observe.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/050-observe.c"
/*
 * 050-observe.c — the engine, saying out loud what it already knew.
 *
 * What this is: the reporting half of phase 7 (issues 701, 702).
 * Growth counts and high-water marks have been kept since phase 2;
 * run counts ride the delivery path; timing, when compiled in, comes
 * from the shims and the delivery path. This file only reads and
 * formats — nothing here decides anything.
 *
 * How it does it, in general terms: walks the station table under no
 * lock but the slots' own for depths (stale-by-a-moment numbers are
 * the nature of observing a live machine), sorts through a dispatch
 * table of orderings, and optionally emits on a timer from a small
 * thread that is not a worker and pushes nothing, so termination
 * stays sound.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Growth past this many *pages* at shutdown is shouted, per the
 * project's rule that a warning is an error nobody has decided
 * about yet. Reported by the diagnostics, not documented as a
 * constant anywhere else — this is where the number lives.
 *
 * **It was four doublings and is now sixteen pages, which is the same
 * backlog said in the new units** (issue 210e). Four doublings meant a
 * port sixteen times its starting depth; sixteen equal pages mean a
 * port seventeen times it. Leaving the number at four would have
 * turned a warning about a runaway producer into one that fires the
 * moment a consumer is briefly slow, and a warning that cries wolf is
 * one people learn to scroll past — which costs more than the warning
 * was ever worth.
 */
#define GROWTH_SHOUT_THRESHOLD 16

/* {{{ station_label() */
static const char *station_label(map_t *m, int i, char *fallback, size_t n)
{
    if (m->station_names && m->station_names[i])
        return m->station_names[i];
    snprintf(fallback, n, "station %d", i);
    return fallback;
}
/* }}} */

/* {{{ map_report_buffers() */
void map_report_buffers(map_t *m, FILE *out)
{
    fprintf(out, "buffers:\n");
    int spoke = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_t *sl = &s->in_ports[j];
            if (sl->kind != IN_PORT_RING)
                continue;
            if (sl->growths == 0 && sl->high_water <= 1)
                continue;
            char fallback[32];
            /* "Pages" rather than "times", because growth stopped
             * doubling and started appending (issue 210e). The two
             * numbers now say different things than they used to: the
             * count is how many pages were added beyond the first,
             * and the depth is a sum across all of them rather than
             * one allocation's size. Saying *pages* is what stops a
             * reader converting the count into a doubling in their
             * head and getting a wildly wrong idea of the backlog. */
            fprintf(out,
                    "  %s.%d: grew %d page%s to %d slots, high water %d\n",
                    station_label(m, i, fallback, sizeof fallback), j,
                    sl->growths, sl->growths == 1 ? "" : "s",
                    sl->capacity, sl->high_water);
            spoke = 1;
        }
    }
    if (!spoke)
        fprintf(out, "  every port stayed shallow; no imbalance to report\n");

    if (m->pool) {
        int capacity, high_water, growths;
        pool_queue_stats(m->pool, &capacity, &high_water, &growths);
        fprintf(out,
                "  the task ring: grew %d time%s to %d entries, high water %d\n"
                "  (port piles mean uneven inputs; ring piles mean consumers\n"
                "   slower than producers — two different diagnoses)\n",
                growths, growths == 1 ? "" : "s", capacity, high_water);
    }
}
/* }}} */

/* {{{ the three orderings — a dispatch table of comparators */
static map_t *sorting_map;   /* qsort has no context argument */

/* Time attributable to a station's own work. This used to add the
 * gather time charged to it as a puller, because a station that
 * pulled paid for its upstream's run on its own thread and hiding
 * that would have mis-ranked it (issue 702). Nothing pulls now
 * (issue 210), so the box's own time is the whole of it. */
static long station_time(const station_t *s)
{
    return s->box_ns;
}

static int by_time(const void *a, const void *b)
{
    const station_t *sa = map_station(sorting_map, *(const int *)a);
    const station_t *sb = map_station(sorting_map, *(const int *)b);
    return (station_time(sb) > station_time(sa))
         - (station_time(sb) < station_time(sa));
}

static int by_contention(const void *a, const void *b)
{
    const station_t *sa = map_station(sorting_map, *(const int *)a);
    const station_t *sb = map_station(sorting_map, *(const int *)b);
    return (sb->mutex_wait_ns > sa->mutex_wait_ns)
         - (sb->mutex_wait_ns < sa->mutex_wait_ns);
}

static int by_count(const void *a, const void *b)
{
    const station_t *sa = map_station(sorting_map, *(const int *)a);
    const station_t *sb = map_station(sorting_map, *(const int *)b);
    return (sb->runs > sa->runs) - (sb->runs < sa->runs);
}

static int (*const orderings[REPORT_ORDER_COUNT])(const void *, const void *) = {
    [REPORT_BY_TIME]       = by_time,
    [REPORT_BY_CONTENTION] = by_contention,
    [REPORT_BY_COUNT]      = by_count,
};
/* }}} */

/* {{{ map_report_stations() */
void map_report_stations(map_t *m, FILE *out, int order)
{
    if (order < 0 || order >= REPORT_ORDER_COUNT) {
        fprintf(stderr, "observe: no such report ordering\n");
        abort();
    }

    static const char *const order_names[REPORT_ORDER_COUNT] = {
        "by time inside boxes", "by mutex contention", "by run count",
    };
    fprintf(out, "stations, %s:\n", order_names[order]);

    int indices[m->n_stations > 0 ? m->n_stations : 1];
    for (int i = 0; i < m->n_stations; i++)
        indices[i] = i;
    sorting_map = m;
    qsort(indices, (size_t)m->n_stations, sizeof indices[0], orderings[order]);

    for (int rank = 0; rank < m->n_stations; rank++) {
        int i = indices[rank];
        station_t *s = map_station(m, i);
        char fallback[32];
        fprintf(out, "  %-12s runs %-7ld produced %-7ld",
                station_label(m, i, fallback, sizeof fallback),
                (long)s->runs, (long)s->produced);
#ifdef SORA_STATS
        fprintf(out, " box %8.2fms  waited %8.2fms",
                s->box_ns / 1e6, s->mutex_wait_ns / 1e6);
#endif
        fprintf(out, "\n");
    }
#ifndef SORA_STATS
    fprintf(out, "  (times compiled out; build with -DSORA_STATS to see them)\n");
#endif
}
/* }}} */

/* {{{ sora_stats_box_time() */
/*
 * Called by every generated shim when SORA_STATS is compiled in.
 *
 * It used to reach a process-wide "active map" pointer, the same way a
 * box's statics write did — and that pointer was the singleton, the
 * thing that made a process able to run only one map at a time. Issue
 * 405 removed the statics write's need for it, which left this as its
 * only remaining user: one optional measurement hook holding the whole
 * engine's ability to compose.
 *
 * So the time rides out on the task instead. The shim charges it to a
 * field the pool carries and never reads, and the delivery walk — which
 * has both the map and the finished task in hand — moves it onto the
 * station afterwards. The pool stays ignorant of stations, and nothing
 * anywhere is process-wide.
 */
void sora_stats_box_time(task_t *t, long ns)
{
    if (t)
        t->box_ns += ns;
}
/* }}} */

/* {{{ the observer thread */
static void *observer_main(void *arg)
{
    map_t *m = arg;
    while (__atomic_load_n(&m->observer_running, __ATOMIC_ACQUIRE)) {
        FILE *out = fopen(m->observer_path, "a");
        if (out) {
            fprintf(out, "--- observation ---\n");
            map_report_buffers(m, out);
            map_report_stations(m, out, REPORT_BY_COUNT);
            fclose(out);
        }
        usleep((useconds_t)m->observer_interval_ms * 1000);
    }
    return NULL;
}

void map_observe_start(map_t *m, const char *path, int interval_ms)
{
    if (interval_ms <= 0) {
        /* Refuse rather than default: an engine writing diagnostics
         * nobody reads is a background thread doing nothing useful
         * (issue 701). Asking for zero means you did not want it. */
        fprintf(stderr, "observe: a non-positive interval — if you do not "
                        "want observation, do not start it\n");
        abort();
    }
    if (m->observer_running) {
        fprintf(stderr, "observe: already observing\n");
        abort();
    }
    m->observer_path = strdup(path);
    m->observer_interval_ms = interval_ms;
    m->observer_running = 1;
    pthread_create(&m->observer, NULL, observer_main, m);
}

void map_observe_stop(map_t *m)
{
    if (!m->observer_running)
        return;
    __atomic_store_n(&m->observer_running, 0, __ATOMIC_RELEASE);
    pthread_join(m->observer, NULL);
    free(m->observer_path);
    m->observer_path = NULL;
}
/* }}} */

/* {{{ map_report_shutdown() */
/*
 * The loud parting word (issue 701): any port that grew past the
 * threshold gets named at teardown, because a map that works while
 * one buffer quietly absorbs a mismatch forever is a map with a
 * design problem nothing else will surface.
 */
void map_report_shutdown(map_t *m)
{
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_t *sl = &s->in_ports[j];
            if (sl->kind == IN_PORT_RING && sl->growths >= GROWTH_SHOUT_THRESHOLD) {
                char fallback[32];
                fprintf(stderr,
                        "observe: %s.%d grew %d pages (to %d slots, high water "
                        "%d) — a producer outran a sibling input the whole "
                        "run; memory absorbed it, and someone should decide\n",
                        station_label(m, i, fallback, sizeof fallback), j,
                        sl->growths, sl->capacity, sl->high_water);
            }
        }
    }
}
/* }}} */

/* 050's private macros end with 050. */
#undef GROWTH_SHOUT_THRESHOLD

/* ==================================================================
 *
 * 051 — a live map written back out
 *
 * Was src/051-dump.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/051-dump.c"
/*
 * 051-dump.c — the loaded map, written back out as a map.
 *
 * What this is: issue 703. The station table rendered in the map
 * file format, so that loading a map and dumping it produces a file
 * equivalent to the one that went in. Round-tripping is the point:
 * if the dump and the original ever disagree, one of them is wrong,
 * and the disagreement is a loader bug nothing else would catch.
 * Once rewiring exists, the file on disk stops describing the
 * program — this becomes the only accurate description of what is
 * actually running.
 *
 * How it does it, in general terms: walks the table, never any
 * remembered text (except the statics entries' load-time text, which
 * is the one place bytes cannot be turned back into words — the
 * first-pass report carries that gap). Derived facts the file format
 * cannot say — types, sizes, indices, the gather depth — ride as
 * comments beside the lines that parse.
 */



#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ kind_letter() */
static char kind_letter(unsigned char kind)
{
    static const char letters[STATION_KIND_COUNT] = { 'p', 'c', 'i' };
    return kind < STATION_KIND_COUNT ? letters[kind] : '?';
}
/* }}} */

/* {{{ map_dump() */
void map_dump(map_t *m, FILE *out)
{
    /*
     * **Every station needs a name**, because a station line begins
     * with one and a file that begins a line with nothing does not
     * read back.
     *
     * This used to ask whether the map had *any* names, which was the
     * same question while reading a file was the only way to build a
     * program — the loader named all of them or none. A program built
     * by calling the construction surface can be named a station at a
     * time, and can have a station added after the rest were named
     * (issue 212), so the question is now asked per station.
     */
    for (int i = 0; i < m->n_stations; i++) {
        if (!map_station(m, i)->call)
            continue;   /* an empty place is not a station */
        if (i < m->n_named && m->station_names && m->station_names[i])
            continue;
        fprintf(stderr,
                "dump: station %d has no name — a station line begins with "
                "one, so this program cannot be written as a file that reads "
                "back\n", i);
        abort();
    }

    /*
     * **The names written out are made unique, and the ones on the
     * program are left alone** (issue 217).
     *
     * A station name is an arbitrary label the engine never reads;
     * two stations in one program may share one and nothing about the
     * program is worse for it. Bringing one description inside another
     * twice produces exactly that — two copies of every station the
     * description names, including its doors.
     *
     * A *file* cannot have two, and the reason is not fussiness: an
     * arrow is written as a destination name, so a file with two
     * `gate` lines cannot say which `gate` an arrow means. The parser
     * refuses one, correctly.
     *
     * So the disambiguation happens here, where it is needed, and
     * nothing is lost by it — a label carrying no meaning can be
     * spelled differently without the program changing. What comes
     * back from reading such a file is the same graph with different
     * labels on some of its stations, which is the same program by
     * every measure this project has.
     *
     * The suffix separator is a character the parser will accept
     * inside a name and never confuse for anything else. It cannot be
     * a dot: an arrow destination is split on its *last* dot to find
     * the port, so `gate.2` would read as station `gate`, port 2.
     */
    char **written = calloc((size_t)(m->n_stations > 0 ? m->n_stations : 1),
                            sizeof *written);
    if (!written) {
        fprintf(stderr, "dump: out of memory naming stations\n");
        abort();
    }
    for (int i = 0; i < m->n_stations; i++) {
        if (!map_station(m, i)->call)
            continue;
        const char *want = m->station_names[i];
        char candidate[128];
        snprintf(candidate, sizeof candidate, "%s", want);
        for (int attempt = 2; ; attempt++) {
            int taken = 0;
            for (int j = 0; j < i; j++)
                if (written[j] && strcmp(written[j], candidate) == 0)
                    taken = 1;
            if (!taken)
                break;
            snprintf(candidate, sizeof candidate, "%s~%d", want, attempt);
        }
        written[i] = strdup(candidate);
        if (!written[i]) {
            fprintf(stderr, "dump: out of memory naming stations\n");
            abort();
        }
    }

    /*
     * **What did not finish, said rather than inferred** (issue 712).
     *
     * A capture taken while workers are mid-task loses their inputs
     * and never delivers their results, and a file that did not say so
     * would be a program quietly missing work somebody computed. That
     * is the standing rule everywhere in this engine: a fallback is a
     * warning and a warning is an error, so the artifact says what it
     * lost.
     *
     * It comes first, before anything a reader would take as fact,
     * because a header found halfway down is a header somebody has
     * already read past.
     */
    if (m->pool) {
        int workers = pool_worker_count(m->pool);
        int busy = 0;
        for (int i = 0; i < workers; i++)
            if (pool_worker_station(m->pool, i) >= 0)
                busy++;
        if (busy > 0) {
            fprintf(out, "# INCOMPLETE CAPTURE\n");
            fprintf(out, "# %d task%s still running and did not finish:\n",
                    busy, busy == 1 ? " was" : "s were");
            for (int i = 0; i < workers; i++) {
                int at = pool_worker_station(m->pool, i);
                if (at < 0)
                    continue;
                const char *who = (at < m->n_named && m->station_names
                                   && m->station_names[at])
                                ? m->station_names[at] : NULL;
                if (who)
                    fprintf(out, "#   %s  (station %d)\n", who, at);
                else
                    fprintf(out, "#   station %d\n", at);
            }
            fprintf(out, "# Their input values are lost and their results "
                         "were never\n");
            fprintf(out, "# delivered. Everything below is otherwise "
                         "accurate.\n");
        }
    }

    fprintf(out, "# dumped from the live station table — what the engine is\n");
    fprintf(out, "# actually running, which is not necessarily what any file\n");
    fprintf(out, "# said. derived facts appear as comments.\n");

    /*
     * There is no statics section any more, and its absence is the
     * dump getting *more* accurate rather than less.
     *
     * The file format's statics section is notation — a way to write a
     * value down once while describing a map and point ports at it by
     * number. The engine used to keep that table alive, so the dump
     * echoed the text each entry was given and wrote the numbers back.
     * That had a hole in it, admitted in its own comment: a value
     * changed while the program ran was not re-serialized, so the dump
     * printed what the file had said rather than what the engine was
     * holding.
     *
     * With each value living on the port that reads it (issue 401),
     * every constant is written out beside its port, from its bytes,
     * by the formatter that mirrors the reader. What comes out is what
     * is actually there — including anything a runtime write changed
     * — which is the whole reason the dump exists.
     *
     * Two ports that shared an entry in the original file dump as two
     * ports each holding their own copy, because that is what they now
     * are. A file that goes in with sharing comes out without it, and
     * reloading gives the same program: the sharing was never
     * observable in behaviour, only in notation.
     */
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        /* Announced like every other line kind (issue 607), so that a
         * station's name never sits where a keyword sits and no word
         * is ever both. */
        fprintf(out, "\nstation %s ", written[i]);
        /*
         * The station knows its own name (issue 311b): the generated
         * placement function wrote it as a literal. This used to scan
         * every box record for one whose call site matched — the
         * emitted table read backwards, a linear search to answer a
         * question the station could just have been told.
         *
         * **A station placed by hand with no name given has none**,
         * and saying so is more honest than inventing a spelling: a
         * program built that way is not described on disk either, so
         * there is nothing a station line could truthfully say. The
         * marker is deliberately not a legal box name, so a dump
         * carrying one cannot be read back in silence.
         */
        /* The door, if it is one (issues 209, 213). A program whose
         * doors did not survive being written down could not be
         * composed after a round trip, which is most of what naming
         * them was for. */
        const char *door = s->door == DOOR_IN  ? " entry"
                         : s->door == DOOR_OUT ? " result"
                         : "";

        /*
         * **Whichever form is unambiguous** (issue 311a). A station
         * carries the box's full address — the file it lives in and
         * the function within it — and the dump writes the bare
         * function name when that resolves to the same box, or the
         * whole address when it does not.
         *
         * Not tidiness. A box compiled while the program ran lives at
         * a serial-numbered path in a scratch directory that belongs
         * to *that* process, so writing its address down produces a
         * file naming somewhere nothing will be next time. The bare
         * name is what a later process can act on: recovery looks for
         * a source saved under the box's own name, which is exactly
         * what makes a grown program's dump reloadable.
         */
        const char *written_as = s->box_name;
        if (written_as) {
            const char *colon = strrchr(written_as, ':');
            if (colon) {
                const char *bare = colon + 1;
                const box_place_t *by_bare = box_place_find(bare);
                const box_place_t *by_address = box_place_find(written_as);
                if (by_bare && by_bare == by_address)
                    written_as = bare;
            }
        }

        /*
         * **Where an iterator had got to** (issue 712), written only
         * when it says something: zero is where one starts, and every
         * other kind of station has no position to be in. The format
         * writes exceptions, and a cursor at the beginning is not one.
         */
        char at[16] = "";
        if (s->kind == STATION_ITERATOR && s->cursor != 0)
            snprintf(at, sizeof at, " @%d", s->cursor);

        fprintf(out, "%s %c%s%s   # station %d\n",
                written_as ? written_as : "?placed-by-hand?",
                kind_letter(s->kind), door, at, i);

        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_t *sl = &s->in_ports[j];

            /*
             * A starting depth, written only when it differs from the
             * default (issue 210b). The format writes exceptions, and
             * a port at the default depth is not one — saying `x10` on
             * every line would be noise a reader learns to skip.
             *
             * It comes before the source because the inline value form
             * runs to the end of the line, so nothing can follow it.
             */
            char depth[32] = "";
            if (sl->capacity != IN_PORT_DEFAULT_CAPACITY)
                snprintf(depth, sizeof depth, "x%d ", sl->capacity);

            switch (sl->kind) {
            case IN_PORT_STATIC: {
                /* The value itself, spoken from its bytes rather than
                 * echoed from remembered text (issue 401) — so a
                 * constant a runtime write changed dumps as what it
                 * now is, which the old table-and-number form could
                 * not do.
                 *
                 * Asked for its length first and then written, because
                 * a struct constant has no useful upper bound and a
                 * fixed buffer would quietly truncate exactly the
                 * values most worth reading. */
                int wanted = in_port_constant_text(sl, NULL, 0);
                char *text = malloc((size_t)wanted + 1);
                if (!text) {
                    fprintf(stderr, "dump: out of memory writing a constant\n");
                    abort();
                }
                in_port_constant_text(sl, text, wanted + 1);
                fprintf(out, "  in %d %s= %s   # %s, %d bytes\n", j, depth,
                        text, sl->type_name ? sl->type_name : "?",
                        sl->elem_size);
                free(text);
                break;
            }
            case IN_PORT_RING: {
                /*
                 * **Values waiting in the buffer, if any** (issue
                 * 712). This is the difference between a schematic and
                 * an image: what the program is shaped like, versus
                 * what it currently holds. A port with work queued in
                 * it writes that work down, so the program can be
                 * picked up again rather than only rebuilt.
                 *
                 * In brackets, because braces already mean a struct
                 * value and a queue is a different kind of thing —
                 * several values where a constant has one.
                 */
                int waiting = in_port_waiting_text(sl, NULL, 0);
                if (waiting > 0) {
                    char *held = malloc((size_t)waiting + 1);
                    if (!held) {
                        fprintf(stderr, "dump: out of memory writing "
                                        "waiting values\n");
                        abort();
                    }
                    in_port_waiting_text(sl, held, waiting + 1);
                    fprintf(out, "  in %d %s[%s]   # %s, %d bytes, %d "
                                 "waiting\n",
                            j, depth, held,
                            sl->type_name ? sl->type_name : "?",
                            sl->elem_size, atomic_load(&sl->held));
                    free(held);
                    break;
                }
                /* The default source; the format writes only
                 * exceptions, but the derived facts still deserve
                 * saying — and a non-default depth is an exception, so
                 * it gets a line of its own rather than a comment. */
                if (depth[0])
                    /* The depth alone, with no source after it. It
                     * used to write a dash here, which reads back as
                     * a port with *no source* — so a program with a
                     * deepened buffer could be written down and not
                     * read in again. */
                    fprintf(out, "  in %d %s  # buffer, %s, %d bytes\n",
                            j, depth, sl->type_name ? sl->type_name : "?",
                            sl->elem_size);
                else
                    fprintf(out,
                            "  # port %d: buffer, %s, %d bytes, %d slots\n",
                            j, sl->type_name ? sl->type_name : "?",
                            sl->elem_size, sl->capacity);
                break;
            }
            case IN_PORT_NONE:
                /*
                 * A port with no source at all, written as a bare dash
                 * (issue 210b). It is a state and not a value, so the
                 * station holding one simply never becomes ready.
                 *
                 * This used to be a comment saying the format had no
                 * word for it, which kept the dump honest at the cost
                 * of the round trip: a half-built program was one of
                 * the things a dump could not promise to reload. It
                 * can now.
                 */
                fprintf(out, "  in %d %s-   # no source yet, %s, %d bytes\n",
                        j, depth, sl->type_name ? sl->type_name : "?",
                        sl->elem_size);
                break;
            }
        }

        /* Written in array order, which is the order the wires were
         * drawn, so dump -> load -> dump produces the same text
         * without anybody arranging it (issue 214). Nothing in the
         * running engine reads that order or means anything by it. */
        int out_port_index = 0;
        for (out_port_t *p = s->out_ports; p; p = p->next, out_port_index++) {
            dest_set_t *set = out_port_dests(p);
            for (int di = 0; set && di < set->n; di++)
                fprintf(out, "  out %d - %s.%d\n", out_port_index,
                        written[set->items[di].station],
                        set->items[di].port);
        }
    }

    for (int i = 0; i < m->n_stations; i++)
        free(written[i]);
    free(written);
}
/* }}} */

/* ==================================================================
 *
 * 052 — changing a running program
 *
 * Was src/052-rewire.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/052-rewire.c"
/*
 * 052-rewire.c — changing the shape while it runs.
 *
 * What this is: issue 704, the feature the whole design has been
 * quietly preparing for. Wires hold station indices rather than
 * addresses; stations never move; a destination list is an immutable
 * array published by one write, so a delivery walk reads it without a
 * lock. Each of those was chosen partly for this moment, and this
 * file is the debt being redeemed.
 *
 * The third of those used to read "delivery snapshots destination
 * lists under the station's mutex", which is what it did before issue
 * 214 — a lock and a copy proportional to fan-out on every value the
 * engine moved. The preparation survived the mechanism being
 * replaced by a better one.
 *
 * A fourth preparation was here and is gone: the gather cycle check
 * ran when a connection was made rather than when it was traversed,
 * and repointing a gather wire at runtime was this file's third
 * operation. The pull path was removed in issue 210, so what remains
 * is connecting and disconnecting.
 *
 * How it does it, in general terms: one rewiring lock makes edge
 * validation and list mutation a single operation — two threads each
 * adding an individually legal edge can produce an illegal pair, so
 * the check and the insertion are never separated. List surgery
 * additionally happens under the owning station's mutex, the same
 * lock delivery snapshots under, so no walker can be left holding a
 * freed node.
 *
 * Refusal behaviour, decided rather than defaulted: a refusal is
 * handed back and the caller decides. A loader that dies serves its
 * author, but a running engine that dies because a control surface
 * sent one bad instruction takes the plant down with it.
 *
 * There are three faces on that now (issue 212). One returns the
 * reason, which is what lets somebody reading a file collect every
 * mistake and present them together; one prints it and returns -1,
 * which is what live editing's callers already expected; one stops
 * the program, which is what construction wants. The cost of the
 * middle one — a caller can ignore the -1 — is weighed in the
 * first-pass report, and the first face is the answer to it.
 */




#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ said() */
/*
 * A refusal that travels upward instead of being printed where it
 * happened (issue 212). The caller decides what to do with it —
 * collect it with the others in a file, hand it to a control surface,
 * or stop the program.
 *
 * Per thread, because two threads may be editing two different maps
 * and a shared buffer would let one overwrite the other's complaint.
 * Valid until this thread's next refusal.
 */
static const char *said(const char *what)
{
    /* Wide enough for the longest refusal below, which names a
     * station, a port and a kind. A message that arrived
     * truncated would be one somebody had to guess the end of. */
    static _Thread_local char kept[256];
    snprintf(kept, sizeof kept, "%s", what);
    return kept;
}
/* }}} */

/* {{{ station_kind_out_port_limit() */
static int station_kind_out_port_limit(unsigned char kind)
{
    static const int limits[STATION_KIND_COUNT] = {
        [STATION_PLAIN] = 1, [STATION_COMPARATOR] = 3, [STATION_ITERATOR] = 0,
    };
    return kind < STATION_KIND_COUNT ? limits[kind] : 1;
}
/* }}} */

/* {{{ map_wire() */
/*
 * **Draw a wire, at any moment** (issue 212) — while a program is
 * being assembled, or on a running one with workers in flight. There
 * is one implementation and it applies every rule, because the rules
 * were never about *when*: a sink has nothing to wire from whether or
 * not the pool has started, and a destination that is not a buffer
 * has nowhere to put a value either way.
 *
 * There used to be two. Construction had its own, which checked less
 * — it did not ask whether the destination was a buffer, and it did
 * not check the widths — and stopped the program when refused;
 * runtime editing had another, which checked everything and printed.
 * Two sets of rules that were supposed to agree, and one of them
 * quietly weaker, is how a program becomes buildable from a file and
 * unbuildable by hand.
 *
 * Returns NULL when the wire was drawn, or a sentence saying why not.
 * The string is valid until this thread's next refusal.
 */
const char *map_wire(map_t *m, int from_station, int port,
                     int to_station, int to_port)
{
    pthread_mutex_lock(&m->rewire_mutex);

    /* Every load-time rule, per edge (issue 604 made callable). */
    if (from_station < 0 || from_station >= m->n_stations
        || to_station < 0 || to_station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("a station index outside the table");
    }
    station_t *from = map_station(m, from_station);
    station_t *to = map_station(m, to_station);
    /* Only construction used to ask this, and it is the one rule the
     * runtime path was missing rather than the other way round: an
     * empty place in the table has no ports to wire and no size to
     * check against, so every question below it would be asked of
     * nothing. */
    if (!from->call || !to->call) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("wiring a station that has no box placed yet — place, "
                    "then wire");
    }
    if (from->out_size == 0) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("wiring from a sink — nothing comes out of it");
    }
    int limit = station_kind_out_port_limit(from->kind);
    if (port < 0 || (limit > 0 && port >= limit)) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("a port index beyond what this station kind can mean");
    }
    if (to_port < 0 || to_port >= to->n_in_ports) {
        /*
         * Named the way the loader's own copy of this check named it
         * (issue 210g), because that copy is going: which station the
         * arrow lands on, what its box is called, and how many ports
         * that box has. "A destination port the box does not have"
         * describes the fault without naming anything anybody can go
         * and look at, which is the same complaint that moved the
         * buffer refusal's wording earlier in this function.
         */
        char who[64];
        if (m->station_names && to_station < m->n_named
            && m->station_names[to_station])
            snprintf(who, sizeof who, "%s", m->station_names[to_station]);
        else
            snprintf(who, sizeof who, "%d", to_station);
        char message[224];
        snprintf(message, sizeof message,
                 "an arrow lands on %s.%d, but '%s' has %d port%s (its "
                 "parameters%s)",
                 who, to_port, to->box_name ? to->box_name : "?",
                 to->n_in_ports, to->n_in_ports == 1 ? "" : "s",
                 to->kind == STATION_COMPARATOR ? ", plus the threshold" : "");
        pthread_mutex_unlock(&m->rewire_mutex);
        return said(message);
    }
    in_port_t *dest = &to->in_ports[to_port];
    /*
     * **A static destination is legal now** (issue 405): a value
     * arriving there overwrites the constant rather than queueing,
     * which is how a constant gets computed at startup rather than
     * written down. What remains refused is a port with *no source*,
     * which has nothing to overwrite and nowhere to queue — the
     * arriving value would have nowhere to go at all.
     */
    if (dest->kind != IN_PORT_RING && dest->kind != IN_PORT_STATIC) {
        /* Named by station and port, because "the destination port"
         * is not something anybody can go and look at. The whole-map
         * check said it this way and this refusal now arrives first,
         * so it had better say as much. */
        char who[64];
        if (m->station_names && to_station < m->n_named
            && m->station_names[to_station])
            snprintf(who, sizeof who, "%s", m->station_names[to_station]);
        else
            snprintf(who, sizeof who, "%d", to_station);
        char message[224];
        snprintf(message, sizeof message,
                 "an arrow lands on %s.%d, but that port is %s, not a buffer "
                 "— the value would have nowhere to go",
                 who, to_port, in_port_kind_name(dest->kind));
        pthread_mutex_unlock(&m->rewire_mutex);
        return said(message);
    }
    /*
     * The wire check, by **width** rather than by type name (issue
     * 309). Identical layouts under different names now wire, which
     * is the capability this buys; same-width types of different
     * layouts also wire, which is the accepted cost, stated as a
     * non-guarantee in 058.
     *
     * The names still ride along in the message, because "4 bytes
     * against 4 bytes" is not a sentence anybody can act on — and
     * both widths ride along beside them, because "box returns vec4,
     * port takes stats" does not say why those disagree.
     */
    {
        /*
         * **Asked of every wire, with nothing standing in front of
         * it** (issue 210g). This block used to be reached only when
         * both stations had input port arrays, and the source's array
         * has nothing to do with the question: a station with no
         * inputs at all — a box that takes nothing and returns a
         * value, which is how most programs start — could be wired
         * into a port of any width whatsoever and nothing complained.
         *
         * It was invisible while the loader kept a width check of its
         * own, because every program read from a file went through
         * that one first. Taking the loader's copy away is what made
         * the hole in this one show, which is the argument for having
         * one of these rather than two: the second check was not
         * redundancy, it was concealment.
         *
         * The destination's array needs no guarding either. A port
         * index past the end was refused a few lines above, and a
         * station with no ports refuses every index there is.
         *
         * **The width decides, and only the width** (issue 309). Two
         * boxes may spell one shape differently and mean the same
         * data, so a wire is legal when both sides count the same
         * bytes — comparing the names would refuse a connection that
         * is perfectly sound, which is the whole reason names stopped
         * being what a wire is checked against.
         */
        if (from->out_size != dest->elem_size) {
            /*
             * **The refusal names both ends by position and by size,
             * and no type name appears in it** (issues 311b, 311c).
             *
             * It used to fetch the two spellings and say "box returns
             * int (4 bytes), port takes double (8 bytes)". That reads
             * well and it is the wrong shape for what this engine
             * actually checks. A name is not what makes a wire legal
             * or illegal — the width is — so a message built around
             * names invites somebody to go and look at the names,
             * which are not the disagreement. Two types with one
             * layout and different names wire perfectly; two with one
             * name could not disagree.
             *
             * What does disagree is a *place* and a *count*, on each
             * end. So the message gives both: which station and which
             * port, and how many bytes it has. The reader is pointed
             * at the two things that are actually in conflict, and at
             * where to go and change one of them.
             *
             * It also stops this being the last thing on the refusal
             * path that reaches back into a box record for a
             * spelling, which is one fewer reason for those records
             * to exist.
             */
            char from_who[64], to_who[64];
            if (m->station_names && from_station < m->n_named
                && m->station_names[from_station])
                snprintf(from_who, sizeof from_who, "%s",
                         m->station_names[from_station]);
            else
                snprintf(from_who, sizeof from_who, "%d", from_station);
            if (m->station_names && to_station < m->n_named
                && m->station_names[to_station])
                snprintf(to_who, sizeof to_who, "%s",
                         m->station_names[to_station]);
            else
                snprintf(to_who, sizeof to_who, "%d", to_station);

            char message[224];
            snprintf(message, sizeof message,
                     "%s output %d produces %d bytes and %s input %d takes "
                     "%d bytes",
                     from_who, port, from->out_size,
                     to_who, to_port, dest->elem_size);
            pthread_mutex_unlock(&m->rewire_mutex);
            return said(message);
        }
    }

    /* The surgery, under the owning station's mutex so no delivery
     * snapshot is mid-walk. Ports are created empty up to the index,
     * exactly as the loader would. */
    pthread_mutex_lock(&from->mutex);
    while (from->n_out_ports <= port) {
        out_port_t *fresh = calloc(1, sizeof *fresh);
        if (!fresh) {
            pthread_mutex_unlock(&from->mutex);
            pthread_mutex_unlock(&m->rewire_mutex);
            return said("out of memory for a port");
        }
        out_port_t **link = &from->out_ports;
        while (*link)
            link = &(*link)->next;
        *link = fresh;
        from->n_out_ports++;
    }
    out_port_t *p = station_out_port(from, port);

    /*
     * A whole new set, published by one write (issue 214). Walkers
     * already inside the old one keep walking it and are not
     * disturbed; the old set is filed rather than freed, because one
     * of them may be in it right now.
     */
    dest_set_t *old = out_port_dests(p);
    dest_set_t *fresh_set = dest_set_build(old, to_station, to_port, -1, -1);
    atomic_store_explicit(&p->dests, fresh_set, memory_order_release);
    pthread_mutex_unlock(&from->mutex);
    map_retire(m, old, free);

    pthread_mutex_unlock(&m->rewire_mutex);
    return NULL;
}
/* }}} */

/* {{{ map_unwire() */
/*
 * **Cut one wire, at any moment.** NULL when it came out, or a
 * sentence saying why not — the same shape as drawing one, for the
 * same reason: a caller reading a description collects every mistake
 * and presents them together.
 *
 * **There used to be a third face on these operations and it is
 * gone** (issue 106). It printed the refusal and returned minus one,
 * which was chosen deliberately in 704: a loader that dies serves its
 * author, while a running engine that dies for one bad control
 * instruction takes the plant down with it. The cost was booked at
 * the time as a debt in plain words — *a caller can ignore a return
 * value* — and an ignored refusal leaves a program running that
 * somebody believes they just edited.
 *
 * Two faces remain: one hands the refusal back so a caller can
 * collect it, and one stops the program. Neither can be ignored into
 * a half-built program.
 */
const char *map_unwire(map_t *m, int from_station, int port,
                       int to_station, int to_port)
{
    pthread_mutex_lock(&m->rewire_mutex);
    if (from_station < 0 || from_station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("a station index outside the table");
    }
    station_t *from = map_station(m, from_station);

    pthread_mutex_lock(&from->mutex);
    out_port_t *p = station_out_port(from, port);
    dest_set_t *old = out_port_dests(p);
    dest_set_t *fresh_set = NULL;
    int found = 0;
    for (int i = 0; old && i < old->n; i++)
        if (old->items[i].station == to_station
            && old->items[i].port == to_port) {
            found = 1;
            break;
        }
    if (found) {
        fresh_set = dest_set_build(old, -1, -1, to_station, to_port);
        atomic_store_explicit(&p->dests, fresh_set, memory_order_release);
    }
    pthread_mutex_unlock(&from->mutex);
    pthread_mutex_unlock(&m->rewire_mutex);

    if (!found)
        return said("no such wire to remove");

    /* Filed, not freed: a walker may be inside the old set right now
     * (issue 214). A value already on its way down the removed wire
     * is delivered, which is indistinguishable from having been
     * delivered a moment earlier and is fine (issue 704). */
    map_retire(m, old, free);

    return NULL;
}
/* }}} */

/* {{{ map_disconnect() */
/*
 * The same operation, for a caller that wants a refusal to stop the
 * program (issue 106).
 */
void map_disconnect(map_t *m, int from_station, int port,
                    int to_station, int to_port)
{
    const char *no = map_unwire(m, from_station, port, to_station, to_port);
    if (no)
        sora_stop_now(m, SORA_EXIT_BAD_CALL, no);
}
/* }}} */


/* {{{ removed_parts_t / reclaim_station() */
/*
 * A removed station's parts, reclaimed by the scrapyard once nobody
 * can still be inside a task built from it.
 *
 * The station record itself stays in the table — it is what an index
 * means, and indices are what wires are made of. What goes is
 * everything hanging off it, and the shim, whose absence is what says
 * the place is free for the next station.
 */
typedef struct removed_parts {
    station_t  *station;
    out_port_t *out_ports;
    in_port_t  *in_ports;
    int         n_in_ports;
    char       *name;
} removed_parts_t;

static void reclaim_station(void *p)
{
    removed_parts_t *r = p;

    out_port_t *port = r->out_ports;
    while (port) {
        free(out_port_dests(port));
        out_port_t *next = port->next;
        free(port);
        port = next;
    }
    for (int i = 0; i < r->n_in_ports; i++) {
        in_port_free_pages(&r->in_ports[i]);
        in_port_constant_free(&r->in_ports[i]);
    }
    free(r->in_ports);
    free(r->name);

    /* Last, and this is the moment the place becomes free: everything
     * that reads a station checks the shim first. */
    station_t *s = r->station;
    s->out_ports = NULL;
    s->n_out_ports = 0;
    s->in_ports = NULL;
    s->n_in_ports = 0;
    s->out_size = 0;
    s->compare = NULL;
    s->cursor = 0;
    s->call = NULL;
    atomic_store_explicit(&s->removed, 0, memory_order_release);

    free(r);
}
/* }}} */

/* {{{ map_remove_station() */
const char *map_remove_station(map_t *m, int station)
{
    pthread_mutex_lock(&m->rewire_mutex);

    if (station < 0 || station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("removing a station outside the table");
    }
    station_t *s = map_station(m, station);
    if (!s->call || atomic_load_explicit(&s->removed, memory_order_acquire)) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("removing a station that is not there");
    }

    /*
     * Marked first, so nothing new starts from it while the wires are
     * being cut. Values already on their way are discarded when they
     * arrive, which is what this engine already does with a value
     * that has nowhere to go.
     */
    pthread_mutex_lock(&s->mutex);
    atomic_store_explicit(&s->removed, 1, memory_order_release);
    pthread_mutex_unlock(&s->mutex);

    /*
     * Every wire that names this station, cut before the station
     * goes. A wire lives only as a destination record on some
     * station's output port, so this walk finds all of them — and
     * because it happens first, nothing stale can survive to be
     * followed afterwards. That is what makes reusing the place safe
     * without a version on every wire.
     */
    for (int i = 0; i < m->n_stations; i++) {
        station_t *other = map_station(m, i);
        if (!other->call)
            continue;
        pthread_mutex_lock(&other->mutex);
        for (out_port_t *p = other->out_ports; p; p = p->next) {
            dest_set_t *old = out_port_dests(p);
            if (!old)
                continue;
            int names_it = 0;
            for (int d = 0; d < old->n; d++)
                if (old->items[d].station == station)
                    names_it = 1;
            if (!names_it)
                continue;
            /* Rebuilt without every wire to this station, in one new
             * set rather than one per wire, so a walker sees the
             * before or the after and never a partial cut. */
            dest_set_t *fresh =
                calloc(1, sizeof *fresh
                          + (size_t)(old->n > 0 ? old->n : 1)
                            * sizeof(destination_t));
            if (!fresh) {
                pthread_mutex_unlock(&other->mutex);
                pthread_mutex_unlock(&m->rewire_mutex);
                return said("out of memory rebuilding a destination set");
            }
            int out = 0;
            for (int d = 0; d < old->n; d++)
                if (old->items[d].station != station)
                    fresh->items[out++] = old->items[d];
            fresh->n = out;
            atomic_store_explicit(&p->dests, fresh, memory_order_release);
            map_retire(m, old, free);
        }
        pthread_mutex_unlock(&other->mutex);
    }

    /*
     * Its parts handed to the scrapyard, which frees them and clears
     * the record once nobody can still be inside a task built from
     * this station. Nothing is detached here: a task being built
     * right now reads the port count and the return size, and they
     * have to still be there.
     */
    removed_parts_t *parts = calloc(1, sizeof *parts);
    if (!parts) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("out of memory removing a station");
    }
    parts->station = s;
    parts->out_ports = s->out_ports;
    parts->in_ports = s->in_ports;
    parts->n_in_ports = s->n_in_ports;
    if (m->station_names) {
        parts->name = m->station_names[station];
        m->station_names[station] = NULL;
    }

    map_retire(m, parts, reclaim_station);
    pthread_mutex_unlock(&m->rewire_mutex);
    return NULL;
}
/* }}} */

/* ==================================================================
 *
 * 074 — boxes and maps compiled at run time
 *
 * Was src/074-latebox.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/074-latebox.c"
/*
 * 074-latebox.c — a box arriving after the program started, from inside.
 *
 * What this is: the five steps that turn C source into a placeable
 * box while a program runs — save it, generate, compile, load, add —
 * and the growable half of the table stations are placed from.
 *
 * How it does it, in general terms: by running the same two programs
 * the build runs, as programs. The generator turns a box source into
 * a source for the generator; the compiler turns that into a shared object;
 * the dynamic linker loads it and hands back the arrays it defines.
 * Nothing here re-implements any of that, which is the point — a box
 * added late goes through the identical path a box added early did,
 * so there is one way for a box to come into existence rather than
 * two that must agree.
 *
 * **The table grows by adding a block and never moves a row.** The
 * generated array is the first block and is const; rows added later
 * live in blocks of their own. That is the same shape everything else
 * growable in this engine uses, for the same reason: a reader
 * resolving a row is never disturbed, because nothing it is looking
 * at moves.
 *
 * Unloading exists but is asked for rather than automatic: a caller
 * names a box and its library is closed, after checking that no
 * station in the map is still placed from it. Nothing sweeps or
 * refcounts, so a library nobody asks about stays for the life of the
 * process — bounded by how often somebody adds code, and stated rather
 * than hidden. Doing it automatically means waiting until no worker is
 * inside the code being freed, which is the retire-sweep-free
 * mechanism issue 214 builds for destination arrays and issue 216
 * needs for stations; it should be built once and shared by all three
 * rather than three times.
 *
 * **Libraries are opened globally** (issue 311d), so a box arriving
 * later can bind to one that arrived earlier instead of carrying its
 * own copy. That is what makes this an iterative compiler rather than
 * a sequence of unrelated compilations, and it is why the previous
 * paragraph matters more than it used to: something bound to may still
 * be bound to.
 */


#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* {{{ the build's own answers, baked in */
/*
 * Which compiler built this binary, where the generator is, and where
 * the headers generated code includes live. Defaults exist only so
 * this file compiles outside the project's Makefile; a real build
 * always defines all four.
 */
#ifndef SORA_CC
#define SORA_CC "cc"
#endif
#ifndef SORA_GENERATOR
#define SORA_GENERATOR "generate"
#endif
#ifndef SORA_INCLUDE
#define SORA_INCLUDE "."
#endif
#ifndef SORA_INCLUDE_LIBS
#define SORA_INCLUDE_LIBS "."
#endif
#ifndef SORA_RAM_SHARED
#define SORA_RAM_SHARED "/dev/shm/minimal-soramech"
#endif
#ifndef SORA_RAM_EXEC
#define SORA_RAM_EXEC "/tmp/minimal-soramech"
#endif
/* }}} */

/* {{{ struct late_block */
/*
 * One dlopen's worth of rows. The arrays belong to the loaded object
 * and live as long as it does, which is forever — see the note about
 * unloading at the top of this file.
 */
typedef struct late_block {
    struct late_block *next;
    /* The placement functions this object brought with it (issue
     * 311b). A box compiled while the program runs has to be
     * placeable the same way as one compiled into it, which means the
     * same generated function doing the writing. */
    const box_place_t *places;
    int                n_places;
    /* And the source it was compiled from, as text (issue 311d). The
     * generator emits this for every object it writes, so a loaded
     * one carries its own C exactly as the program's own generated
     * file does. Nothing is copied and nothing is allocated: these
     * point into the loaded object and live as long as it does.
     *
     * Two things read it. A person or a capture wanting to write out
     * what a grown program is now made of, which cannot be answered
     * from the build alone once boxes have arrived. And the check that
     * refuses to compile the same source twice — same path, same
     * bytes, already here. */
    const box_source_t *sources;
    int                 n_sources;
    void              *handle;
} late_block_t;

static late_block_t *late_head;   /* newest first */
static int           late_total;
static int           late_serial; /* names the scratch files apart */
/* }}} */

/* {{{ late_source_dir() / late_library_dir() */
/*
 * **Two tiers, and which goes where is not arbitrary.** The project
 * keeps RAM-backed scratch in two places: `/dev/shm` for artifacts
 * that are read — logs, text, anything a person or a reloader looks
 * at — and `/tmp` for anything that gets executed. `/dev/shm` is
 * commonly mounted so that nothing on it may be executed at all, so a
 * shared object written there compiles fine and then cannot be
 * loaded, failing with a message about mapping a segment that says
 * nothing about the real cause.
 *
 * So the source text goes to the read tier and the compiled library
 * goes to the execute tier. Found by putting them both in the wrong
 * one, which is the useful kind of mistake: the rule existed and the
 * reason for it had to be rediscovered.
 */
const char *late_source_dir(void)
{
    return SORA_RAM_SHARED "/late-boxes";
}

static const char *late_library_dir(void)
{
    return SORA_RAM_EXEC "/late-boxes";
}
/* }}} */

/* {{{ late_box_count() / late_box_at() */
int late_box_count(void)
{
    return late_total;
}

const box_place_t *late_box_at(int i)
{
    /* Blocks are newest first, so walking them in order and counting
     * down gives the caller oldest-first, which is the order boxes
     * were added and the only order that means anything. */
    int remaining = late_total - i;
    for (late_block_t *b = late_head; b; b = b->next) {
        if (remaining <= b->n_places)
            return &b->places[remaining - 1];
        remaining -= b->n_places;
    }
    return NULL;
}
/* }}} */

/* {{{ late_place_find() */
/*
 * The placement function for a box that arrived after the program
 * started. Newest first, for the same reason the box lookup is: a
 * name added twice resolves to the newer one, and the older code is
 * still loaded and still callable by anything already placed.
 */
static const box_place_t *late_place_find(const char *name);

static const box_place_t *late_place_find(const char *name)
{
    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_places; i++)
            /* The same rule the compiled-in rows are searched by
             * (issue 311a), so a bare name, a basename and a path all
             * mean here what they mean there. */
            if (box_place_matches(&b->places[i], name))
                return &b->places[i];
    return NULL;
}
/* }}} */

/* {{{ late_source_text() */
/*
 * The C a late-arriving source was compiled from, by the path it was
 * compiled under. Newest first, for the same reason the box lookup is:
 * a path compiled twice reports the newer text, which is what somebody
 * asking "what is running now" means by the question.
 *
 * Full path first and basename second, matching how a box is
 * addressed, so that a person can type what they can see.
 */
const char *late_source_text(const char *path)
{
    if (!path || !*path)
        return NULL;

    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_sources; i++)
            if (strcmp(b->sources[i].path, path) == 0)
                return b->sources[i].text;

    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_sources; i++) {
            const char *slash = strrchr(b->sources[i].path, '/');
            const char *base = slash ? slash + 1 : b->sources[i].path;
            if (strcmp(base, path) == 0)
                return b->sources[i].text;
        }
    return NULL;
}
/* }}} */


/* {{{ static int ensure_dir() */
static int ensure_dir(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return 0;
    fprintf(stderr, "latebox: cannot create %s: %s\n", path, strerror(errno));
    return -1;
}
/* }}} */

/* {{{ static int write_text() */
static int write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "latebox: cannot write %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    size_t n = strlen(text);
    int ok = fwrite(text, 1, n, f) == n;
    if (fclose(f) != 0)
        ok = 0;
    if (!ok) {
        fprintf(stderr, "latebox: cannot write %s: %s\n",
                path, strerror(errno));
        remove(path);
        return -1;
    }
    return 0;
}
/* }}} */

/* {{{ static int run() */
/*
 * Runs a command and reports whether it succeeded. The child's own
 * output goes wherever ours goes, deliberately: when a box source
 * does not compile, the message worth reading is the compiler's,
 * with its line numbers and its carets, not a summary this file
 * invented.
 */
static int run(const char *command)
{
    int rc = system(command);
    if (rc == 0)
        return 0;
    if (rc < 0)
        fprintf(stderr, "latebox: cannot run '%s': %s\n",
                command, strerror(errno));
    return -1;
}
/* }}} */

/* {{{ static void close_library() */
static void close_library(void *handle)
{
    dlclose(handle);
}
/* }}} */

/* {{{ late_unload_box() */
int late_unload_box(map_t *m, const char *name)
{
    if (!m || !name || !*name) {
        fprintf(stderr, "latebox: asked to unload nothing\n");
        return -1;
    }

    /* Find the block holding it, and refuse outright if the name
     * belongs to a box the program was built with — that code is part
     * of the binary and there is nothing to close. */
    late_block_t **link = &late_head;
    late_block_t *found = NULL;
    for (; *link; link = &(*link)->next) {
        for (int i = 0; i < (*link)->n_places; i++)
            if (strcmp((*link)->places[i].name, name) == 0) {
                found = *link;
                break;
            }
        if (found)
            break;
    }
    if (!found) {
        fprintf(stderr, "latebox: '%s' was not added while this program ran, "
                        "so there is nothing to unload\n", name);
        return -1;
    }

    /*
     * Refused while any station places any box in this block. The
     * block is the unit that gets closed, so one placed box in it
     * keeps the whole thing — which is right, because they arrived in
     * one library and leave in one.
     */
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        /*
         * **By the name the station was placed as** (issue 311b),
         * rather than by comparing shim pointers. The record that
         * held those pointers is gone; a station carries the name
         * literal its own placement function wrote, which is the same
         * fact arrived at from the other side.
         *
         * It is conservative in exactly one direction, and that
         * direction is the safe one: two blocks holding a box of the
         * same name would each refuse to unload while the other's
         * station stands. Refusing an unload that could have gone
         * ahead costs a library staying loaded; allowing one that
         * could not is the crash this check exists to prevent.
         */
        for (int b = 0; b < found->n_places; b++)
            if (s->box_name && strcmp(s->box_name,
                                      found->places[b].address) == 0) {
                fprintf(stderr,
                        "latebox: station %d places '%s', so its code cannot "
                        "be unloaded — remove the station first\n",
                        i, found->places[b].address);
                return -1;
            }
    }

    /*
     * Unlinked first, so nothing can find it by name from here on and
     * no station can be placed from it after this point. Then the
     * library goes to the scrapyard: a worker may be *inside* this
     * code right now, and the counter that answers that is the same
     * one a replaced destination set uses (issue 214).
     */
    late_total -= found->n_places;
    *link = found->next;
    void *handle = found->handle;
    free(found);
    map_retire(m, handle, close_library);
    return 0;
}
/* }}} */

/* {{{ late_recover_box() */
/*
 * Compile a box back into existence from the source it left behind.
 *
 * A box added while a program ran saved its source under its own
 * name, so a **fresh process** loading a dump of that program can
 * find it. Without this, a dump taken after somebody added code
 * describes a program that cannot be rebuilt — which would make the
 * dump a record of something unreproducible, and the whole value of a
 * dump is that it says what is actually there.
 *
 * **It announces itself.** Recovering is doing something the caller
 * did not ask for, on the strength of a file found lying about, and
 * this project treats a silent fallback as an error. So it says which
 * box it is recovering and where the source came from, every time.
 *
 * Returns the row, or null when there is no source to recover from —
 * which is the ordinary case of a genuinely misspelled name, and the
 * caller's message for that is the best one in the program.
 */
static const box_place_t *late_recover_box(const char *name);

static const box_place_t *late_recover_box(const char *name)
{
    if (!name || !*name)
        return NULL;

    char path[512];
    snprintf(path, sizeof path, "%s/%s.c", late_source_dir(), name);

    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    rewind(f);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    char *text = malloc((size_t)size + 1);
    if (!text) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, f);
    text[got] = '\0';
    fclose(f);

    fprintf(stderr, "latebox: '%s' was not built in; recovering it from %s\n",
            name, path);

    int added = late_compile_source(text);
    free(text);
    if (added < 0) {
        fprintf(stderr, "latebox: '%s' could not be recovered from its own "
                        "saved source\n", name);
        return NULL;
    }
    return box_place_find(name);
}
/* }}} */

/* {{{ static int ensure_path_dirs() */
/*
 * Create every directory leading to a file path, the way `mkdir -p`
 * does. Needed because the sources a program carries are filed under
 * the paths the build knew them by — `src/boxes/029-demo-boxes.c` —
 * and writing them back out under those same paths is what lets the
 * generator resolve a description's box names exactly as it did at
 * build time. Flattening them would work until two directories held a
 * file of the same name, which is the case the addressing rules exist
 * for in the first place.
 */
static int ensure_path_dirs(const char *path)
{
    char work[1024];
    size_t n = strlen(path);
    if (n >= sizeof work) {
        fprintf(stderr, "latebox: path too long: %s\n", path);
        return -1;
    }
    memcpy(work, path, n + 1);

    for (char *p = work + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(work, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "latebox: cannot create %s: %s\n",
                    work, strerror(errno));
            return -1;
        }
        *p = '/';
    }
    return 0;
}
/* }}} */

/* {{{ static int spill_sources() */
/*
 * Write every source this program is made of into a directory, under
 * the path it was compiled as. The generator is then pointed at that
 * directory as its root, so what it reads is byte for byte what this
 * program was built from and every name resolves the way it did then.
 *
 * **This is a copy of text, not of code.** Nothing here is compiled;
 * the sources exist so the generator can answer "which box does this
 * line mean" with the same answer it gave at build time, and the
 * emitted file that results carries none of them.
 *
 * Both halves of what a program is made of go out — what the build
 * compiled in and what has arrived since — because a description may
 * name either, and the distinction is not one a person writing a
 * description should have to know about.
 *
 * Returns how many were written, or -1.
 */
static int spill_sources(const char *dir, const char **paths, int cap)
{
    int n = 0;
    char full[1024];

    for (int i = 0; i < sora_n_box_sources; i++) {
        if (n >= cap)
            break;
        if (snprintf(full, sizeof full, "%s/%s",
                     dir, sora_box_sources[i].path) >= (int)sizeof full) {
            fprintf(stderr, "latebox: path too long: %s\n",
                    sora_box_sources[i].path);
            return -1;
        }
        if (ensure_path_dirs(full) != 0)
            return -1;
        if (write_text(full, sora_box_sources[i].text) != 0)
            return -1;
        paths[n] = strdup(full);
        if (!paths[n]) {
            fprintf(stderr, "latebox: out of memory\n");
            return -1;
        }
        n++;
    }

    /* And what has arrived since. Oldest blocks last in this walk, so
     * a path compiled twice is written by the newest first and then
     * overwritten by the older — which would be backwards, so the
     * newest wins by being written last. Walking the list in reverse
     * is not worth the bookkeeping: skipping a path already written is
     * the same answer and reads as what it is. */
    for (late_block_t *b = late_head; b; b = b->next) {
        for (int i = 0; i < b->n_sources; i++) {
            if (n >= cap)
                break;
            int already = 0;
            for (int k = 0; k < n && !already; k++)
                if (strstr(paths[k], b->sources[i].path))
                    already = 1;
            if (already)
                continue;
            if (snprintf(full, sizeof full, "%s/%s",
                         dir, b->sources[i].path) >= (int)sizeof full) {
                fprintf(stderr, "latebox: path too long: %s\n",
                        b->sources[i].path);
                return -1;
            }
            if (ensure_path_dirs(full) != 0)
                return -1;
            if (write_text(full, b->sources[i].text) != 0)
                return -1;
            paths[n] = strdup(full);
            if (!paths[n]) {
                fprintf(stderr, "latebox: out of memory\n");
                return -1;
            }
            n++;
        }
    }
    return n;
}
/* }}} */

/* {{{ static int gather_missing_boxes() */
/*
 * **A description may name a box this program does not hold**, and
 * getting it is the ordinary path rather than a rescue (issue 311d).
 *
 * A program that grew and then wrote itself down names boxes that
 * arrived after it started. Whoever reads that description back —
 * usually a different process, which was never told those boxes exist
 * — has to compile them before the description can be compiled
 * against them.
 *
 * The compiler is asked what the description references, because
 * reading text is its job and the engine does not do it any more.
 * Each name is looked for; anything missing is recovered from the
 * source that was filed under it when it was first compiled, and
 * compiled in. Only then is the description itself compiled, by which
 * point every name answers.
 *
 * A name that answers to nothing anywhere is left alone deliberately.
 * The refusal belongs to the compiler, which will name the
 * description and the line — and this cannot, because it does not
 * know which line asked.
 */
static void gather_missing_boxes(const char *map_path, const char *list_path)
{
    char cmd[2048];
    snprintf(cmd, sizeof cmd, "%s --map-boxes %s > %s",
             SORA_GENERATOR, map_path, list_path);
    if (run(cmd) != 0)
        return;   /* the compiler will say what is wrong with it */

    FILE *f = fopen(list_path, "r");
    if (!f)
        return;

    char name[256];
    while (fgets(name, sizeof name, f)) {
        size_t n = strlen(name);
        while (n > 0 && (name[n - 1] == '\n' || name[n - 1] == '\r'))
            name[--n] = '\0';
        if (n == 0)
            continue;
        if (box_place_find(name))
            continue;
        late_recover_box(name);
    }
    fclose(f);
}
/* }}} */

/* {{{ late_spill_sources() — issue 712 */
/*
 * **Every source this program is made of, written out under the paths
 * it was compiled as**, so that a captured program can be built again
 * somewhere else.
 *
 * A program that grew boxes while it ran is made of more than its
 * build compiled: those arrived as text, were compiled, and are as
 * much part of what the program *is* as anything the build put in.
 * A description of such a program names boxes whose source exists
 * nowhere on the machine that reads it.
 *
 * So a capture that is meant to stand alone is a **directory**: the
 * description, and beside it every source it names, at the paths the
 * description addresses them by. Building it needs the engine, which
 * is what building anything with this engine needs — not a new
 * dependency, the same one a consumer already has.
 *
 * Returns how many sources were written, or -1.
 */
int late_spill_sources(const char *dir)
{
    enum { MAX_SPILLED = 256 };
    const char *paths[MAX_SPILLED];
    int n = spill_sources(dir, paths, MAX_SPILLED);
    for (int i = 0; i < n; i++)
        free((void *)paths[i]);
    return n;
}
/* }}} */

/* {{{ late_compile_map() */
const map_build_t *late_compile_map(const char *map_text)
{
    if (!map_text || !*map_text) {
        fprintf(stderr, "latebox: an empty description describes nothing\n");
        return NULL;
    }

    const char *dir = late_source_dir();
    const char *libdir = late_library_dir();
    if (ensure_dir(SORA_RAM_SHARED) != 0 || ensure_dir(dir) != 0)
        return NULL;
    if (ensure_dir(SORA_RAM_EXEC) != 0 || ensure_dir(libdir) != 0)
        return NULL;

    int serial = late_serial++;
    char map_path[512], src_root[512], gen_path[512], lib_path[512];
    char cmd[8192];
    snprintf(map_path, sizeof map_path, "%s/map-%d-%d.map",
             dir, (int)getpid(), serial);
    snprintf(src_root, sizeof src_root, "%s/sources-%d-%d",
             dir, (int)getpid(), serial);
    snprintf(gen_path, sizeof gen_path, "%s/built-%d-%d.c",
             dir, (int)getpid(), serial);
    snprintf(lib_path, sizeof lib_path, "%s/built-%d-%d.so",
             libdir, (int)getpid(), serial);

    /* Saved before anything is done with it, for the same reason a box
     * source is: a dump may be taken at any moment, including while
     * something is going wrong, and a failure path is the worst
     * possible time to discover something needed saving. */
    if (write_text(map_path, map_text) != 0)
        return NULL;
    if (ensure_dir(src_root) != 0)
        return NULL;

    /*
     * Anything the description names that is not here yet is compiled
     * in first, so that spilling the sources below writes it out with
     * the rest and the compiler can resolve every name.
     */
    char list_path[512];
    snprintf(list_path, sizeof list_path, "%s/names-%d-%d.txt",
             dir, (int)getpid(), serial);
    gather_missing_boxes(map_path, list_path);

    enum { MAX_SPILLED = 256 };
    const char *spilled[MAX_SPILLED];
    int n_spilled = spill_sources(src_root, spilled, MAX_SPILLED);
    if (n_spilled <= 0) {
        fprintf(stderr, "latebox: this program carries no source text, so a "
                        "description's box names cannot be resolved\n");
        return NULL;
    }

    /*
     * **--external-boxes is the whole difference from compiling a
     * box.** It says the boxes are already in the process that will
     * load this, so the emitted file declares the functions that build
     * their stations rather than defining them, and carries no second
     * copy of anything.
     */
    int at = snprintf(cmd, sizeof cmd,
                      "%s %s --root=%s --map=%s --external-boxes",
                      SORA_GENERATOR, gen_path, src_root, map_path);
    for (int i = 0; i < n_spilled && at < (int)sizeof cmd; i++)
        at += snprintf(cmd + at, sizeof cmd - (size_t)at, " %s", spilled[i]);
    if (at >= (int)sizeof cmd) {
        fprintf(stderr, "latebox: too many sources to name on one command "
                        "line\n");
        return NULL;
    }
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the generator refused %s\n", map_path);
        return NULL;
    }

    snprintf(cmd, sizeof cmd,
             "%s -std=gnu11 -O2 -fPIC -shared -I%s -I%s -o %s %s",
             SORA_CC, SORA_INCLUDE, SORA_INCLUDE_LIBS, lib_path, gen_path);
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the compiler refused the code generated "
                        "for %s\n", map_path);
        return NULL;
    }

    /* Globally, like a box, so a description compiled after this one
     * can bind to anything this one brought. */
    void *handle = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        /* The failure worth naming: a description naming a box this
         * program does not hold arrives here as an unresolved symbol,
         * and the message names it. */
        fprintf(stderr, "latebox: cannot load %s: %s\n", lib_path, dlerror());
        return NULL;
    }

    const map_build_t *builds = dlsym(handle, "sora_map_builds");
    const int *count = dlsym(handle, "sora_n_map_builds");
    if (!builds || !count || *count <= 0) {
        fprintf(stderr, "latebox: %s builds no description — the generator "
                        "emitted something unexpected\n", lib_path);
        dlclose(handle);
        return NULL;
    }
    return &builds[0];
}
/* }}} */

/* {{{ late_compile_source() */
int late_compile_source(const char *c_source)
{
    if (!c_source || !*c_source) {
        fprintf(stderr, "latebox: asked to compile nothing\n");
        return -1;
    }

    const char *dir = late_source_dir();
    const char *libdir = late_library_dir();
    if (ensure_dir(SORA_RAM_SHARED) != 0 || ensure_dir(dir) != 0)
        return -1;
    if (ensure_dir(SORA_RAM_EXEC) != 0 || ensure_dir(libdir) != 0)
        return -1;

    int serial = late_serial++;
    char box_path[512], gen_path[512], lib_path[512], cmd[2048];
    snprintf(box_path, sizeof box_path, "%s/box-%d-%d.c",
             dir, (int)getpid(), serial);
    snprintf(gen_path, sizeof gen_path, "%s/emitted-%d-%d.c",
             dir, (int)getpid(), serial);
    snprintf(lib_path, sizeof lib_path, "%s/box-%d-%d.so",
             libdir, (int)getpid(), serial);

    /* The source is saved **before** anything is done with it, and
     * that ordering is the decision rather than an accident: a dump
     * may be taken at any moment, including while something is going
     * wrong, and a failure path is the worst possible time to
     * discover that something needed saving. The artifact exists
     * before anybody needs it. */
    if (write_text(box_path, c_source) != 0)
        return -1;

    snprintf(cmd, sizeof cmd, "%s %s %s", SORA_GENERATOR, gen_path, box_path);
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the generator refused %s\n", box_path);
        return -1;
    }

    /* Position-independent and shared, with the engine's headers
     * reachable because generated code includes them. The compiler is
     * the one that built this binary, which is what makes its answer
     * to sizeof the same answer. */
    snprintf(cmd, sizeof cmd,
             "%s -std=gnu11 -O2 -fPIC -shared -I%s -I%s -o %s %s",
             SORA_CC, SORA_INCLUDE, SORA_INCLUDE_LIBS, lib_path, gen_path);
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the compiler refused the generated "
                        "generated source for %s\n", box_path);
        return -1;
    }

    /*
     * **Opened globally, so that the next arrival can bind to this
     * one** (issue 311d step 7). Privately was the old setting, and it
     * meant every arrival was an island: a second one naming a
     * function the first had already compiled had to carry its own
     * copy, because it could not see the first one's.
     *
     * Global costs nothing here and needs no table. When a later
     * shared object names a function it does not define, the dynamic
     * linker binds it against what is already loaded — which is a
     * lookup by name that the operating system already maintains for
     * every process, that this project does not have to write, test,
     * or keep in step with anything.
     *
     * Measured before it was relied on: a second object naming a
     * function it does not define binds straight to the first object's
     * copy, with this process uninvolved.
     *
     * What it means for names is worth stating plainly, because global
     * scope is usually where somebody gets hurt: two arrivals defining
     * the same symbol resolve to the first. That is correct here
     * rather than dangerous, because generated symbols carry the box's
     * full path, so two boxes only collide when they are the same box
     * from the same file — and a box is forbidden to remember anything
     * between calls, so two copies of one box are indistinguishable.
     */
    void *handle = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "latebox: cannot load %s: %s\n", lib_path, dlerror());
        return -1;
    }

    /*
     * **One pair of symbols now, where there were two** (issue 311b).
     * The generated file used to define a table of box records beside
     * the placement functions, and this fetched both — the records to
     * learn what the new box was, the placements to be able to put one
     * anywhere. The records are gone: every number they held is
     * written straight onto a station by the placement function, from
     * a `sizeof` the compiler folded, so there was nothing in them
     * anybody read twice.
     */
    const box_place_t *places = dlsym(handle, "box_places");
    const int *count = dlsym(handle, "n_box_places");
    if (!places || !count) {
        fprintf(stderr, "latebox: %s defines no placement functions — the "
                        "generator emitted something unexpected\n", lib_path);
        dlclose(handle);
        return -1;
    }
    if (*count <= 0) {
        fprintf(stderr, "latebox: %s defines no boxes; a source with only "
                        "static helpers has nothing to place\n", box_path);
        dlclose(handle);
        return -1;
    }

    late_block_t *block = calloc(1, sizeof *block);
    if (!block) {
        fprintf(stderr, "latebox: out of memory\n");
        dlclose(handle);
        return -1;
    }
    block->places   = places;
    block->n_places = *count;
    block->handle   = handle;

    /* The source text the object carries (issue 311d). Absent is not
     * an error the way absent placement functions are: an object built
     * by an older generator has boxes but no text, and refusing to
     * load it would trade a working box for a missing document. What
     * it costs is that this source cannot be written back out, and the
     * lookup answers NULL rather than pretending. */
    const box_source_t *sources = dlsym(handle, "sora_box_sources");
    const int *n_sources = dlsym(handle, "sora_n_box_sources");
    if (sources && n_sources && *n_sources > 0) {
        block->sources   = sources;
        block->n_sources = *n_sources;
    }

    /* Published last, and by one write, so a reader walking the list
     * either sees this block complete or does not see it at all. */
    block->next = late_head;
    late_head = block;
    late_total += *count;

    /*
     * A copy per box, named for the box, so a **later process** can
     * find the source from a name alone — which is what makes a dump
     * taken after this reloadable. The serial-numbered file above is
     * what was handed over; these are how it is found again.
     *
     * One source may define several boxes, so each gets its own copy
     * of the whole thing. Copies rather than links because a link into
     * the scratch tier is one more thing that can be half there.
     */
    for (int i = 0; i < *count; i++) {
        char by_name[512];
        snprintf(by_name, sizeof by_name, "%s/%s.c", dir, places[i].name);
        if (write_text(by_name, c_source) != 0)
            fprintf(stderr, "latebox: '%s' is loaded but its source could "
                            "not be filed under its own name; a dump taken "
                            "now will not reload in a fresh process\n",
                    places[i].name);
    }

    return *count;
}
/* }}} */

/* ==================================================================
 *
 * 092 — signals, capture, and the end
 *
 * Was src/092-stopping.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
#line 1 "/mnt/mtwo/programming/ai-playground/minimal-soramech/src/092-stopping.c"
/*
 * 092-stopping.c — every way a program ends except the happy one.
 *
 * What this is: issue 106, from inside. Interface and reasoning in
 * 091-stopping.h.
 *
 * How it does it, in general terms: **nothing here is a signal
 * handler.** The three signals are blocked in every thread and the
 * thread that started the program calls sigwait, which hands back a
 * signal number as an ordinary value to a thread running ordinary
 * code. Every restriction on what a handler may call stops applying,
 * which is why the reports below are free to take locks and format
 * text — except the one that deliberately does neither.
 *
 * The gathering happens on the waiting thread itself rather than as a
 * task somebody hopes gets scheduled. That is what lets it work on a
 * program whose every worker is wedged: the queue guarantees nothing
 * to anybody, and the one piece of work that must happen cannot be
 * the one piece of work standing in line.
 */




#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * The signal the pool raises when the work runs out.
 *
 * A real-time signal rather than one of the two conventional
 * user-defined ones, because those two are exactly what a box someone
 * brings to this engine is most likely to be using already. Nothing
 * in this project sends SIGRTMIN for any other reason, and a program
 * that does can say so — the number is read from a variable, not
 * baked into the comparisons below.
 */
static int finished_signal;

/* Where a report goes. Opened during preparation and never after,
 * because a dying program cannot answer for a failed open. */
static int  report_fd = -1;
static char report_where[512];

/* How many interrupts have arrived. The second one is an escape
 * hatch and takes no other path with it. */
static int interrupts;

/* {{{ static void escape_now() */
/*
 * The only signal handler in this file, installed for the length of
 * one gather and doing the one thing a handler is unarguably allowed
 * to do. See the second-interrupt case in sora_wait for why it has to
 * exist at all.
 */
static void escape_now(int sig)
{
    (void)sig;
    _exit(SORA_EXIT_INTERRUPTED);
}
/* }}} */

/* {{{ static void say() */
/*
 * One line into the report. Formatted into a stack buffer and written
 * with one call, because **write is the only file operation available
 * on every path in this file** — including the one forbidden to take
 * a lock, and a buffered stream takes one.
 */
static void say(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

static void say(const char *fmt, ...)
{
    if (report_fd < 0)
        return;
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n > (int)sizeof line - 1)
        n = (int)sizeof line - 1;
    ssize_t wrote = write(report_fd, line, (size_t)n);
    (void)wrote;   /* a dying program cannot do anything about a short write */
}
/* }}} */

/* {{{ sora_prepare() */
void sora_prepare(const char *report_path)
{
    finished_signal = SIGRTMIN;

    /*
     * Blocked in this thread, and therefore in every thread made
     * after it, because a thread inherits the mask of whoever created
     * it. That is what makes "every thread" true without visiting any
     * of them — and it is why this must be called before the pool
     * exists rather than after.
     */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, finished_signal);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        fprintf(stderr, "stopping: could not block the signals this "
                        "program answers\n");
        exit(SORA_EXIT_NO_RESOURCE);
    }

    if (report_path && *report_path) {
        snprintf(report_where, sizeof report_where, "%s", report_path);
    } else {
        /*
         * The project's RAM-backed scratch tier, which a reboot
         * empties. That is right for something read while debugging
         * and useless as a post-mortem after the machine came back;
         * the core dump is what covers the second case.
         */
        mkdir(SORA_RAM_SHARED, 0777);
        snprintf(report_where, sizeof report_where,
                 "%s/stopping-%d.txt", SORA_RAM_SHARED, (int)getpid());
    }

    report_fd = open(report_where, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (report_fd < 0) {
        /*
         * Loud, and not fatal. A program that cannot open its report
         * can still run and can still stop; what it cannot do is
         * explain itself afterwards, and somebody should be told that
         * now rather than discovering an empty file later.
         */
        fprintf(stderr, "stopping: cannot open %s for the report (%s) — "
                        "this program will stop without explaining itself\n",
                report_where, strerror(errno));
    }
}
/* }}} */

/* {{{ sora_report_path() */
const char *sora_report_path(void)
{
    return report_where;
}
/* }}} */

/* {{{ static void report_without_locks() */
/*
 * **Maximum evidence, no cooperation** — the report for a program
 * that may be holding a lock nobody will ever release.
 *
 * Nothing here takes a lock, walks a list, or follows a pointer that
 * another thread could be freeing. What it reads is atomic counters
 * that are always on, and one integer per worker saying which station
 * that worker was inside. Stations are named by index rather than by
 * name, because the names are an array somebody may be growing right
 * now and reading it would be the crash this report exists to
 * explain.
 *
 * A shelf pointer is safe to follow: the table grows by adding
 * shelves and nothing already placed ever moves, so the shelf array
 * only ever gains entries. Reading a count that is one behind means
 * missing the newest station, which is a smaller wrong than not
 * reporting at all.
 */
static void report_without_locks(map_t *m)
{
    say("== the program stopped on demand, taking no locks ==\n");
    if (!m) {
        say("(no program)\n");
        return;
    }

    int n = m->n_stations;
    say("stations: %d\n", n);
    for (int i = 0; i < n; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        say("  station %d: %ld run, %ld produced\n", i,
            (long)atomic_load_explicit(&s->runs, memory_order_relaxed),
            (long)atomic_load_explicit(&s->produced, memory_order_relaxed));
    }

    if (m->pool) {
        int workers = pool_worker_count(m->pool);
        say("workers: %d\n", workers);
        for (int i = 0; i < workers; i++) {
            int at = pool_worker_station(m->pool, i);
            if (at < 0)
                say("  worker %d: between tasks\n", i);
            else
                say("  worker %d: inside station %d\n", i, at);
        }
    }
    say("== a core dump follows; every thread's stack is in it, "
        "including the box that is not returning ==\n");
}
/* }}} */

/* {{{ static void report_everything() */
/*
 * **The full picture**, gathered on the thread that received the
 * signal. Somebody is standing there and wants to know what happened,
 * so this is free to take every lock it likes — the thread holding
 * this number is running ordinary code, not a handler.
 *
 * It reports the two backlogs separately, because they mean opposite
 * things: values stuck on a station's inputs mean one branch of the
 * graph outran another, while tasks stuck in the queue mean the
 * consumers are slower than the producers.
 */
static void report_everything(map_t *m)
{
    say("== the program was interrupted ==\n");
    if (!m) {
        say("(no program)\n");
        return;
    }

    if (m->pool)
        say("tasks queued and never run: %d\n", pool_queued(m->pool));

    say("\n-- stations --\n");
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        char who[64];
        if (m->station_names && i < m->n_named && m->station_names[i])
            snprintf(who, sizeof who, "%s", m->station_names[i]);
        else
            snprintf(who, sizeof who, "%d", i);
        say("  %s: %ld run, %ld produced\n", who,
            (long)atomic_load_explicit(&s->runs, memory_order_relaxed),
            (long)atomic_load_explicit(&s->produced, memory_order_relaxed));
        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_t *sl = &s->in_ports[j];
            if (atomic_load_explicit(&sl->kind, memory_order_relaxed)
                != IN_PORT_RING)
                continue;
            say("    port %d: %d waiting, %d deepest, grown %d times\n",
                j, map_in_port_depth(m, i, j), sl->high_water, sl->growths);
        }
    }

    if (m->pool) {
        say("\n-- workers --\n");
        int workers = pool_worker_count(m->pool);
        for (int i = 0; i < workers; i++) {
            int at = pool_worker_station(m->pool, i);
            if (at < 0) {
                say("  worker %d: between tasks\n", i);
                continue;
            }
            char who[64];
            if (m->station_names && at < m->n_named && m->station_names[at])
                snprintf(who, sizeof who, "%s", m->station_names[at]);
            else
                snprintf(who, sizeof who, "%d", at);
            say("  worker %d: inside %s\n", i, who);
        }
    }

    /*
     * And the program itself, written as a map file. Somebody
     * diagnosing a program that was edited while it ran needs the
     * shape it had at the end, not the shape the file on disk
     * describes — those stopped being the same thing the moment a
     * program could be built while running.
     */
    say("\n-- the program as it stands --\n");
    FILE *f = fdopen(dup(report_fd), "a");
    if (f) {
        map_dump(m, f);
        fclose(f);
    }
}
/* }}} */

/* {{{ sora_wait() */
int sora_wait(map_t *m)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, finished_signal);

    if (m && m->pool)
        pool_signal_when_finished(m->pool, finished_signal);

    for (;;) {
        int sig = 0;
        if (sigwait(&set, &sig) != 0)
            continue;

        if (sig == finished_signal) {
            /* The ordinary ending. The last sleeper already broadcast
             * shutdown; this only collects the threads. */
            if (m && m->pool)
                pool_join(m->pool);
            return SORA_EXIT_FINISHED;
        }

        if (sig == SIGTERM) {
            /*
             * **Wind down.** Shut the one door the outside can push
             * work through and go back to waiting, so the program
             * ends exactly the way it would have ended on its own —
             * the last-sleeper rule, unmodified, triggered early.
             *
             * No diagnostics. Nobody asked for any, and a supervisor
             * stopping a healthy program does not want a report it
             * did not request.
             */
            if (m)
                atomic_store_explicit(&m->closing, 1, memory_order_release);
            continue;
        }

        if (sig == SIGINT) {
            if (++interrupts > 1)
                _exit(SORA_EXIT_INTERRUPTED);

            /*
             * **A second one skips everything, and making that true
             * needs the only handler in this file.**
             *
             * Ctrl+C twice has to always work. Everywhere else here,
             * the signals stay blocked and this thread asks for them
             * — which is what frees the reports below to take locks
             * and format text. But a thread that is *gathering* is
             * not asking, so a second interrupt arriving during the
             * gather would sit pending until the gather finished,
             * and the gather is exactly the thing that may never
             * finish: it takes a station's mutex, and the reason
             * somebody is pressing ctrl+C twice may be that a mutex
             * is held by something that will never release it.
             *
             * So for the length of the gather, and only then, SIGINT
             * is unblocked with a handler that does the one thing a
             * handler is unarguably allowed to do. **_exit is on the
             * short list of async-signal-safe calls**; it runs
             * nobody's cleanup, which is precisely what is being
             * escaped.
             *
             * Without this the escape hatch is nominal: it would work
             * only when the report was going to succeed anyway, which
             * is when nobody needs it.
             */
            struct sigaction escape;
            memset(&escape, 0, sizeof escape);
            escape.sa_handler = escape_now;
            sigemptyset(&escape.sa_mask);
            sigaction(SIGINT, &escape, NULL);
            sigset_t just_int;
            sigemptyset(&just_int);
            sigaddset(&just_int, SIGINT);
            pthread_sigmask(SIG_UNBLOCK, &just_int, NULL);

            /*
             * Stop starting new things — a worker inside a box
             * finishes it, because there is no safe way to interrupt
             * executing C — and then gather everything, here, on this
             * thread. Not as a task: a program whose every worker is
             * wedged has no thread free to run one, which is exactly
             * when this report matters most.
             */
            if (m && m->pool)
                pool_stop(m->pool);
            report_everything(m);
            return SORA_EXIT_INTERRUPTED;
        }

        if (sig == SIGQUIT) {
            /*
             * **No cooperation at all.** Do not stop the pool, do not
             * wait for any thread, and above all do not take a single
             * lock — the reason this signal arrived may be that a
             * lock is held by something that will never release it.
             * Then abort, which leaves a core, so a debugger sees
             * every thread's stack including the box that is not
             * returning.
             */
            report_without_locks(m);
            abort();
        }
    }
}
/* }}} */

/* {{{ static int write_capture() */
static int write_capture(map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "capture: cannot write %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    map_dump(m, f);
    if (fclose(f) != 0) {
        fprintf(stderr, "capture: cannot finish writing %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}
/* }}} */

/* {{{ sora_capture() */
int sora_capture(map_t *m, const char *path)
{
    if (!m || !path || !*path) {
        fprintf(stderr, "capture: needs a program and somewhere to put it\n");
        return -1;
    }

    /*
     * **Shut the door first, then let it drain.** The entrance is the
     * only way anything outside pushes work in, so closing it is the
     * whole of what "stop accepting new work" can mean here — and with
     * nothing new arriving, the last-sleeper rule ends the program the
     * way it would have ended on its own. Nothing is told to hurry and
     * no task is discarded.
     */
    atomic_store_explicit(&m->closing, 1, memory_order_release);

    if (m->pool) {
        /* Released in case nobody has: a pool whose workers are still
         * parked at the starting gate never drains, and both of these
         * are safe to call again. */
        pool_release(m->pool);
        pool_join(m->pool);
    }

    return write_capture(m, path);
}
/* }}} */

/* {{{ static int write_capture_report() */
/*
 * **What a person wants to know about the program they just put
 * down**, beside the artifact rather than inside it.
 *
 * Separate on purpose. The description is read by a machine and has to
 * mean exactly one thing; this is read by somebody deciding whether
 * the capture was worth taking, and answers questions the description
 * deliberately does not — how hard each station worked, which buffers
 * ran deep, and which boxes were not there when the program started.
 *
 * **Nothing here is measured for this.** Every number was already
 * being kept: run counts and produced counts by issue 702, buffer
 * depths and growths by 701, the late arrivals by 310. A report that
 * needed its own instrumentation would be a report that changed what
 * it was reporting on.
 */
static int write_capture_report(map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "capture: cannot write %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    fprintf(f, "# what this program had done when it was put down.\n");
    fprintf(f, "# beside the description, not inside it: this is for a\n");
    fprintf(f, "# person, and the description is for a machine.\n\n");

    long total_runs = 0;
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        total_runs += atomic_load_explicit(&s->runs, memory_order_relaxed);
    }
    fprintf(f, "stations: %d, tasks run: %ld\n\n", m->n_stations, total_runs);

    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        if (!s->call)
            continue;
        const char *who = (i < m->n_named && m->station_names
                           && m->station_names[i])
                        ? m->station_names[i] : "?";
        fprintf(f, "%s (station %d): %ld run, %ld made due elsewhere\n",
                who, i,
                (long)atomic_load_explicit(&s->runs, memory_order_relaxed),
                (long)atomic_load_explicit(&s->produced,
                                           memory_order_relaxed));

        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_t *sl = &s->in_ports[j];
            if (atomic_load_explicit(&sl->kind, memory_order_relaxed)
                != IN_PORT_RING)
                continue;
            int held = atomic_load_explicit(&sl->held, memory_order_relaxed);
            if (held == 0 && sl->high_water == 0 && sl->growths == 0)
                continue;
            /*
             * A buffer that grew is one input side outrunning another,
             * which is the single most useful thing this report says:
             * it names where a program was unbalanced, and it says so
             * in slots rather than in a judgement.
             */
            fprintf(f, "    port %d: %d waiting, %d deepest, "
                       "%d slots, grown %d time%s\n",
                    j, held, sl->high_water,
                    atomic_load_explicit(&sl->capacity, memory_order_relaxed),
                    sl->growths, sl->growths == 1 ? "" : "s");
        }
    }

    /*
     * **Which boxes were not there when the program started.** This is
     * the half of what a program is made of that no build knows about,
     * and the reason a whole capture is a directory rather than a file.
     */
    int late = late_box_count();
    fprintf(f, "\nboxes that arrived while it ran: %d\n", late);
    for (int i = 0; i < late; i++) {
        const box_place_t *row = late_box_at(i);
        if (row)
            fprintf(f, "    %s\n", row->address);
    }

    if (m->pool) {
        int workers = pool_worker_count(m->pool);
        int busy = 0;
        for (int i = 0; i < workers; i++)
            if (pool_worker_station(m->pool, i) >= 0)
                busy++;
        fprintf(f, "\nworkers: %d, still inside a box when written: %d\n",
                workers, busy);
        if (busy > 0)
            fprintf(f, "the description is an incomplete capture and says "
                       "so at its top.\n");
    }

    if (fclose(f) != 0) {
        fprintf(stderr, "capture: cannot finish writing %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}
/* }}} */

/* {{{ sora_capture_whole() */
int sora_capture_whole(map_t *m, const char *dir)
{
    if (!m || !dir || !*dir) {
        fprintf(stderr, "capture: needs a program and somewhere to put it\n");
        return -1;
    }

    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "capture: cannot create %s: %s\n",
                dir, strerror(errno));
        return -1;
    }

    /*
     * The sources first, then the description. A reader meeting a
     * half-written capture should find a directory that is missing its
     * description rather than one whose description names sources that
     * are not there — the first is obviously incomplete and the second
     * looks whole and is not.
     */
    if (late_spill_sources(dir) < 0)
        return -1;

    char path[1024];
    if (snprintf(path, sizeof path, "%s/program.map", dir)
        >= (int)sizeof path) {
        fprintf(stderr, "capture: path too long: %s\n", dir);
        return -1;
    }
    if (sora_capture(m, path) != 0)
        return -1;

    /* The report last, because it is the only part nothing depends on
     * — a capture missing its report is still a program somebody can
     * build. */
    char report[1024];
    if (snprintf(report, sizeof report, "%s/report.txt", dir)
        >= (int)sizeof report) {
        fprintf(stderr, "capture: path too long: %s\n", dir);
        return -1;
    }
    return write_capture_report(m, report);
}
/* }}} */

/* {{{ sora_capture_now() */
int sora_capture_now(map_t *m, const char *path)
{
    if (!m || !path || !*path) {
        fprintf(stderr, "capture: needs a program and somewhere to put it\n");
        return -1;
    }
    /* Nothing is shut and nothing is waited for. Whatever a worker is
     * inside stays there, and the artifact says which stations those
     * are. */
    return write_capture(m, path);
}
/* }}} */

/* {{{ sora_stop_now() */
void sora_stop_now(map_t *m, int exit_code, const char *why)
{
    fprintf(stderr, "%s\n", why ? why : "an invalid operation");
    fflush(stderr);

    /*
     * Say everything that can be said on the way out. This is a
     * program being told to do something it refuses, which means
     * somebody is editing it and will want to know what state it was
     * in — the same report an interrupt writes, for the same reason.
     */
    if (m && m->pool)
        pool_stop(m->pool);
    say("== an invalid operation ended this program ==\n%s\n",
        why ? why : "an invalid operation");
    report_everything(m);

    _exit(exit_code);
}
/* }}} */
