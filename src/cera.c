/*
 * cera.c — the engine, entire.
 *
 * One translation unit holding every part of the runtime: the thread
 * pool, the station table, the delivery path, constants, the reader,
 * the reports, the parts that change a running program, the parts that
 * compile new code into one, and the parts that end one.
 *
 * Everything not declared in cera.h is static, so the names this file
 * publishes are exactly the names that file lists.
 *
 * Twelve components, in reading order: the joints the components use to
 * reach each other, then the runtime. The numbers are positions in that
 * order. Markers are maintained by scripts/113-refold.lua.
 *
 * What each call does is in cera.info.md.
 */
#include "cera.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ The joints — how the engine reaches itself */
/* ==================================================================
 *
 * The joints — how the engine reaches itself
 *
 * Private to this file. The components below call each other in both
 * directions — delivery reaches a slot the station layer defines, and
 * the station layer builds a task delivery owns — so their declarations
 * come first.
 * ================================================================== */

/*
 * One event onto the trail, or nothing at all.
 *
 * Under CERA_WATCH this is a call; without it the arguments are not
 * even evaluated, so an unwatched program carries no emitting rather
 * than a branch that is usually false.
 */
#ifdef CERA_WATCH
/* {{{ watch_emit() */
static void watch_emit(cera_map_t *m, int kind,
                       uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                       uint64_t ns);
/* }}} */
#define CERA_EMIT(m, kind, a, b, c, d, ns) \
    watch_emit((m), (kind), (uint32_t)(a), (uint32_t)(b), \
               (uint32_t)(c), (uint32_t)(d), (uint64_t)(ns))
#else
#define CERA_EMIT(m, kind, a, b, c, d, ns) ((void)0)
#endif

/* {{{ cera_fail() */
/*
 * The one way the engine ends a program it refuses to continue.
 * `cera_fail` exits with the code it is given; `cera_bug` aborts,
 * leaving a core. Defined with the rest of the ending.
 */
static void cera_fail(int exit_code, const char *fmt, ...);
/* }}} */

/* {{{ cera_bug() */
static void cera_bug(const char *fmt, ...);
/* }}} */

/* {{{ out_port_dests() */
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
static cera_dest_set_t *out_port_dests(const cera_out_port_t *p);
/* }}} */

/* {{{ dest_set_build() */
static cera_dest_set_t *dest_set_build(const cera_dest_set_t *from, int add_station,
                           int add_port, int drop_station, int drop_port);
/* }}} */

/* {{{ map_deliver() */
/*
 * The delivery walk: the pool's finish hook. Takes a finished task,
 * chooses the outgoing port by the station's kind, and walks that
 * port's destinations delivering the output value to each.
 */
static void map_deliver(void *ctx, cera_task_t *t);
/* }}} */

/* {{{ in_port_slot() */
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
 * one named state to another, returning whether this caller won it. A
 * transition from a state the slot is not in fails.
 *
 * Two callers race for one slot and exactly one wins. The loser is not
 * blocked and does not retry in place — it goes and looks at another
 * slot.
 */
static void *in_port_slot(const cera_in_port_t *sl, int index);
/* }}} */

/* {{{ in_port_slot_move() */
static int   in_port_slot_move(const cera_in_port_t *sl, int index, int from, int to);
/* }}} */

/* {{{ slot_move_at() */
/*
 * The same transition on a slot the caller has already located. An
 * ordinal would cost a walk down the page list per slot examined; the
 * scan already holds the address.
 */
static int   slot_move_at(void *slot, int elem_size, int from, int to);
/* }}} */

/* {{{ slot_state_at() */
/*
 * Reading a slot's state, and setting it without a compare-and-swap.
 *
 * Only for transitions whose mover is already unique: a claimer under
 * the station's mutex taking a *ready* slot (other claimers excluded by
 * the lock, and a writer never touches a ready one), or an owner moving
 * a slot it holds in *reserved* or *claimed*. Everywhere else — a
 * writer racing another writer for an empty slot — use the
 * compare-and-swap above.
 */
static int   slot_state_at(const void *slot, int elem_size);
/* }}} */

/* {{{ in_port_waiting_text() */
/*
 * Every value waiting in this port's buffer, written down as text,
 * comma separated. Returns how many characters it wanted — ask with
 * no room, allocate, ask again — and zero when nothing is waiting.
 *
 * The order is slot order, which promises nothing: a port has no head
 * and no tail, and docs/058-guarantees.md promises nothing about the
 * order values leave one.
 */
static int in_port_waiting_text(const cera_in_port_t *sl, char *out, int room);
/* }}} */

/* {{{ in_port_add_page() */
/*
 * Growing a ring buffer, which is also how it gets its first page.
 * `in_port_add_page` appends one page of `page_slots` empty slots and
 * adds them to the capacity; nothing already there moves. Callers hold
 * the station's mutex, so two threads meeting a full buffer add one
 * page between them rather than one each.
 *
 * `in_port_free_pages` drops the whole list: teardown, and the one
 * moment a port's page size may change — its starting depth, settable
 * only while the port is empty.
 */
static cera_in_port_page_t *in_port_add_page(cera_in_port_t *sl);
/* }}} */

/* {{{ in_port_free_pages() */
static void            in_port_free_pages(cera_in_port_t *sl);
/* }}} */

/* {{{ in_port_kind_name() */
/*
 * What a port's tag is called, in the words a person would use. A
 * refusal names which of the three it found: "not a buffer" covers two
 * situations with different fixes — a static already holds a value and
 * has no room to queue another, an unconfigured port is one nobody has
 * finished wiring. One table, so a fourth tag is a row.
 */
static const char *in_port_kind_name(unsigned char kind);
/* }}} */

/* {{{ station_out_port() */
/* The port at an index, or null if never wired — which delivery
 * reads as "discard". */
static cera_out_port_t *station_out_port(cera_station_t *s, int index);
/* }}} */

/* {{{ station_out_port_make() */
/* The same, creating it and everything before it. The caller holds
 * the station's mutex, because this appends to a list a delivery walk
 * may be reading. */
static cera_out_port_t *station_out_port_make(cera_station_t *s, int index);
/* }}} */

/* {{{ in_port_constant_free() */
/* A port's constant and, for a string, the characters it points at.
 * Owned by the port and freed with the map. */
static void in_port_constant_free(cera_in_port_t *sl);
/* }}} */

/* {{{ in_port_constant_text() */
/*
 * A port's constant, turned back into the text a map file would use.
 * Writes at most `room` bytes including the terminator, and returns
 * how many characters it wanted — so a caller can tell it was cut
 * short.
 *
 * The exact mirror of the reader that walks a field table turning text
 * into bytes. No text is retained anywhere — the value lives on the
 * port as bytes — so the bytes are what gets spoken.
 */
static int in_port_constant_text(const cera_in_port_t *sl, char *out, int room);
/* }}} */

/* {{{ task_build() */
/*
 * The claimed buffer carries one value per port, of every kind:
 * statics are claimed under the station's mutex beside the ring pops,
 * so nothing is left to resolve here.
 */
static cera_task_t *task_build(cera_map_t *m, int station_index,
                   const unsigned char *claimed, int port);
/* }}} */

/* {{{ box_place_matches() */
/*
 * Whether one row is what a name refers to. Three forms, one rule: a
 * bare function name, a basename and a function, or a path and a
 * function. The compiled-in rows and the rows that arrived while the
 * program ran are searched separately and must agree about what a
 * name means.
 */
static int box_place_matches(const cera_box_place_t *row, const char *name);
/* }}} */

/* {{{ slot_set_at() */
static void  slot_set_at(void *slot, int elem_size, int to);
/* }}} */

/* {{{ map_retire() */
/*
 * The scrapyard. A thing the engine has stopped using cannot be freed
 * at once, because a worker may still be reading it; it is filed, and
 * freed when no worker can still be inside the epoch it was filed in.
 * Three things go through it: a replaced destination set, a removed
 * station's ports and buffers, and the compiled code of a box nobody
 * places any more.
 *
 * map_retire sweeps before filing, so a program that changes shape
 * forever reclaims as it goes.
 */
static void        map_retire(cera_map_t *m, void *p, void (*free_fn)(void *));
/* }}} */

/* {{{ map_scrap_sweep() */
static void        map_scrap_sweep(cera_map_t *m);
/* }}} */

/* {{{ map_scrap_count() */
static int         map_scrap_count(cera_map_t *m);
/* }}} */

/* {{{ map_scrap_free_all() */
static void        map_scrap_free_all(cera_map_t *m);
/* }}} */

/*
 * A joint no caller inside the engine has; only a white-box test, and
 * those compile as one unit with this file. Built on its own this file
 * genuinely does not use them, and the compiler says so.
 */
#define CERA_TEST_ONLY __attribute__((unused))

/* ================================================================== */

/* }}} */

/* {{{ 012 — the pool */
/* ==================================================================
 *
 * 012 — the pool
 *
 * Was libs/012-pool.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * The thread pool: one queue, many workers, no boss.
 *
 * Work arrives as task structs pushed into a ring; worker threads take
 * them out and run them. Nothing decides who does what — whoever is
 * free takes whatever is oldest.
 *
 * One mutex guards the ring, the count of sleeping workers, and the
 * stop flag, so "is there work" and "is anyone awake" are answered
 * under a single lock and cannot tear. The ring holds pointers out to
 * heap-allocated tasks and nothing holds pointers into the ring, so it
 * may be reallocated to twice its size whenever it fills: growth moves
 * the shelf, never the boxes on it.
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>   /* raising the finished signal */

/*
 * Small on purpose: growth is cheap, and the tests want it to happen.
 * Raising this hides the mechanism rather than improving anything.
 */
#define POOL_INITIAL_CAPACITY 8

/* {{{ type worker_t */
/*
 * One worker thread's identity: which pthread it is, which index it
 * answers to, and the pool it belongs to. The statistics are keyed on
 * the index.
 */
typedef struct worker {
    pthread_t thread;
    int       index;
    cera_pool_t   *pool;
} worker_t;
/* }}} */

/* {{{ struct pool_epoch */
/*
 * A worker's epoch, padded to a cache line. Two workers bumping their
 * own counters must not share a line, or an uncontended write becomes
 * a contended one.
 */
struct pool_epoch {
    _Atomic uint64_t v;
    /*
     * **Which station this worker is inside**, or -1. A number the
     * pool ferries and never interprets, written at the same two
     * moments as the epoch beside it and by the same thread — one more
     * write to a line already being written and already nobody else's.
     *
     * **A number rather than the task's address.** The signal reports
     * say which station each worker was in while taking no locks and
     * touching nothing that could have been freed. A stale integer is
     * a wrong answer; a stale pointer is a crash inside the thing that
     * exists to explain a crash.
     */
    _Atomic int32_t  station;
    char pad[64 - sizeof(_Atomic uint64_t) - sizeof(_Atomic int32_t)];
};
/* }}} */

/* {{{ struct pool */
struct pool {
    /* The ring. `slots` holds pointers out to tasks; capacity is the
     * array length; head is the oldest task, tail the next free slot.
     * One slot is always left empty so head==tail means empty and
     * never means full. All four are guarded by `mutex`. */
    cera_task_t **slots;
    int      capacity;
    int      head;
    int      tail;

    pthread_mutex_t mutex;

    /* Woken whenever a task is pushed or the pool is stopping.
     * Sleeping workers wait here. */
    pthread_cond_t wake;

    /* Workers park here at creation until cera_pool_release. */
    pthread_cond_t start_gate;
    int            released;

    worker_t *workers;
    int       n_workers;

    /* How many workers are registered asleep, exact because it is only
     * ever touched under `mutex`. Termination is decided from it. */
    int sleeping;

    /* Set once, by the last sleeper finding the queue truly empty,
     * or by cera_pool_destroy on a never-released pool. Every worker that
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
     * bumping theirs never write the same line. Odd while inside a
     * task, even while not. Sixty-four bits, so a counter cannot wrap
     * back to a snapshot within a run and read as unchanged.
     */
    struct pool_epoch *epochs;

    cera_pool_finish_t finish;
    void         *finish_ctx;

    /* Measurements for the phase 1 demo: the queue's own story. */
    int high_water;
    int growths;

    /*
     * **A signal to raise when this pool finishes by itself**, or 0.
     *
     * The last worker to fall asleep decides a program is over. The
     * thread that wants to know is already waiting for a signal in
     * order to be told to stop, so the end of the work arrives at that
     * same waiting point, told apart by its number.
     *
     * Opt-in. The default action for most signals kills the process,
     * so a pool that raised one unasked would end every program that
     * does not expect it.
     */
    int finished_signal;
};
/* }}} */

/* {{{ queue_count() */
/* How many tasks the ring currently holds. Caller holds the mutex. */
static int queue_count(cera_pool_t *p)
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
 * Double the ring. Caller holds the mutex.
 *
 * A wrapped ring stores its oldest entries at the high end and its
 * newest at the low end, so the wrapped portion is copied to read
 * contiguously again; doubling without unwrapping would leave a hole
 * in the middle.
 *
 * The ring holds pointers out to tasks and nothing holds pointers into
 * the ring: the shelf moves, the boxes on it do not.
 */
static void queue_grow(cera_pool_t *p)
{
    int held = queue_count(p);
    int new_capacity = p->capacity * 2;
    cera_task_t **fresh = malloc((size_t)new_capacity * sizeof *fresh);
    if (!fresh) {
        cera_fail(CERA_EXIT_NO_RESOURCE, "pool: queue growth to %d entries failed: out of memory\n",
                new_capacity);
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

/* {{{ cera_pool_push() */
void cera_pool_push(cera_pool_t *p, cera_task_t *t)
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

    /* Wake every sleeper rather than a chosen one. Whoever arrives
     * first takes the task. */
    pthread_cond_broadcast(&p->wake);

    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* {{{ cera_pool_pop() */
cera_task_t *cera_pool_pop(cera_pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    cera_task_t *t = NULL;
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

/* {{{ this_worker_index */
/*
 * Each worker thread notes its own index here at startup. Any code
 * running on that thread — a box, a shim, the statistics — can ask
 * which worker it is on. Threads that are not workers read -1.
 */
static __thread int this_worker_index = -1;
/* }}} */

/* {{{ cera_pool_worker_index() */
int cera_pool_worker_index(void)
{
    return this_worker_index;
}
/* }}} */

/* {{{ cera_pool_worker_epoch() */
uint64_t cera_pool_worker_epoch(cera_pool_t *p, int worker)
{
    if (!p || !p->epochs || worker < 0 || worker >= p->n_workers)
        return 0;
    return atomic_load_explicit(&p->epochs[worker].v, memory_order_acquire);
}
/* }}} */

/* {{{ worker_main() */
/*
 * The run loop: one loop under the mutex, dropped only while actually
 * running a task.
 *
 *   - stop set        -> return, letting cera_pool_join collect us.
 *   - queue non-empty -> pop, unlock, run, deliver, free, relock.
 *   - queue empty     -> register asleep. If that registration makes
 *                        every worker asleep and nobody outside can
 *                        still push, look one last time and declare
 *                        the program finished. Otherwise wait.
 *
 * The check-queue and register-asleep steps happen inside one hold of
 * the mutex, and the wait releases that same mutex atomically, so a
 * worker cannot be preempted between deciding to sleep and registering
 * — the lost-wakeup window does not exist here.
 */
static void *worker_main(void *arg)
{
    worker_t *w = arg;
    cera_pool_t *p = w->pool;
    int finished_here = 0;

    this_worker_index = w->index;

    pthread_mutex_lock(&p->mutex);

    /* Park at the starting gate until cera_pool_release. Seeding
     * happens while everyone is parked here, which is what keeps
     * "nothing pushes from outside after startup" true for maps. */
    while (!p->released && !p->stop)
        pthread_cond_wait(&p->start_gate, &p->mutex);

    for (;;) {
        if (p->stop)
            break;

        if (p->head != p->tail) {
            /* Work exists: take the oldest and run it with the lock
             * dropped, so the queue stays open while user code runs. */
            cera_task_t *t = p->slots[p->head];
            p->head = (p->head + 1) % p->capacity;
            pthread_mutex_unlock(&p->mutex);

            /*
             * The epoch, bumped around **the whole task** rather than
             * around any one part of it. Odd means this
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
             * this is still running. Ferried, not read. */
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
         * lock, which is what keeps the count exact. */
        p->sleeping++;

        if (p->sleeping == p->n_workers && p->outside == 0) {
            /* Every worker is now asleep and nobody outside can
             * push. This thread's registration completed the count,
             * so it looks one final time. Under this
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
             * out. Raised after the lock is released, at
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
     * the process rather than to a thread. Every thread
     * blocks these signals, so it stays pending until whoever is
     * waiting asks for it — which is exactly the handoff wanted, and
     * needs no thread identity to be recorded anywhere.
     */
    if (finished_here)
        kill(getpid(), finished_here);
    return NULL;
}
/* }}} */

/* {{{ cera_pool_signal_when_finished() */
void cera_pool_signal_when_finished(cera_pool_t *p, int signo)
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

/* {{{ cera_pool_stop() */
/*
 * **Stop starting new things**, which is what halting honestly means
 * here. A worker inside a box finishes that box, because
 * there is no safe way to interrupt executing C; a worker looking for
 * work finds the flag instead and returns.
 *
 * Whatever is still queued stays queued and is never run. That is
 * visible rather than hidden: tearing the pool down afterwards says
 * how many were left.
 */
void cera_pool_stop(cera_pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    p->stop = 1;
    pthread_cond_broadcast(&p->wake);
    pthread_cond_broadcast(&p->start_gate);
    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* {{{ cera_pool_finished() */
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
 * The rule that prevents it: make the promise before opening the gate.
 * This is how somebody finds out they did not.
 */
int cera_pool_finished(cera_pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    int done = p->stop;
    pthread_mutex_unlock(&p->mutex);
    return done;
}
/* }}} */

/* {{{ cera_pool_queued() */
int cera_pool_queued(cera_pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    int n = queue_count(p);
    pthread_mutex_unlock(&p->mutex);
    return n;
}
/* }}} */

/* {{{ cera_pool_worker_station() */
/*
 * **Readable from any thread, holding nothing.** This is the one
 * measurement the report written under a held lock is allowed to
 * take, so it must be a plain load of a plain number and never a
 * dereference of anything.
 */
int cera_pool_worker_station(cera_pool_t *p, int worker)
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
 * the CERAMIC_WORKERS environment variable is next, because tests
 * want one worker and race hunts want two; failing both, one worker
 * per online processor.
 */
static int decide_worker_count(int n_workers)
{
    if (n_workers > 0)
        return n_workers;

    const char *env = getenv("CERAMIC_WORKERS");
    if (env && *env) {
        int n = atoi(env);
        if (n <= 0) {
            cera_fail(CERA_EXIT_BAD_CALL, "pool: CERAMIC_WORKERS is '%s', not a positive number\n", env);
        }
        return n;
    }

    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online < 1) {
        cera_fail(CERA_EXIT_NO_RESOURCE, "pool: could not count online processors\n");
    }
    return (int)online;
}
/* }}} */

/* {{{ cera_pool_create() */
cera_pool_t *cera_pool_create(int n_workers, cera_pool_finish_t finish, void *finish_ctx)
{
    cera_pool_t *p = calloc(1, sizeof *p);
    if (!p) {
        cera_fail(CERA_EXIT_NO_RESOURCE, "pool: allocation failed\n");
    }

    p->capacity = POOL_INITIAL_CAPACITY;
    p->slots = malloc((size_t)p->capacity * sizeof *p->slots);
    if (!p->slots) {
        cera_fail(CERA_EXIT_NO_RESOURCE, "pool: queue allocation failed\n");
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
        cera_fail(CERA_EXIT_NO_RESOURCE, "pool: out of memory for the worker epochs\n");
    }
    /* A worker inside no station says so. Zeroed memory would claim
     * every worker is inside station zero, which is a real station
     * and therefore a lie rather than an absence. */
    for (int i = 0; i < p->n_workers; i++)
        atomic_store_explicit(&p->epochs[i].station, -1,
                              memory_order_relaxed);

    p->workers = calloc((size_t)p->n_workers, sizeof *p->workers);
    if (!p->workers) {
        cera_fail(CERA_EXIT_NO_RESOURCE, "pool: worker table allocation failed\n");
    }

    for (int i = 0; i < p->n_workers; i++) {
        p->workers[i].index = i;
        p->workers[i].pool = p;
        int err = pthread_create(&p->workers[i].thread, NULL,
                                 worker_main, &p->workers[i]);
        if (err != 0) {
            cera_fail(CERA_EXIT_NO_RESOURCE, "pool: creating worker %d failed: %s\n",
                    i, strerror(err));
        }
    }

    return p;
}
/* }}} */

/* {{{ cera_pool_release() */
void cera_pool_release(cera_pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    p->released = 1;
    pthread_cond_broadcast(&p->start_gate);
    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* {{{ cera_pool_join() */
void cera_pool_join(cera_pool_t *p)
{
    if (p->joined)
        return;
    p->joined = 1;
    for (int i = 0; i < p->n_workers; i++) {
        int err = pthread_join(p->workers[i].thread, NULL);
        if (err != 0) {
            cera_fail(CERA_EXIT_NO_RESOURCE, "pool: joining worker %d failed: %s\n",
                    i, strerror(err));
        }
    }
}
/* }}} */

/* {{{ cera_pool_submitter_register() */
void cera_pool_submitter_register(cera_pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    p->outside++;
    pthread_mutex_unlock(&p->mutex);
}
/* }}} */

/* {{{ cera_pool_submitter_unregister() */
void cera_pool_submitter_unregister(cera_pool_t *p)
{
    pthread_mutex_lock(&p->mutex);
    p->outside--;
    if (p->outside < 0) {
        cera_fail(CERA_EXIT_BAD_CALL, "pool: submitter unregistered more times than registered\n");
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

/* {{{ cera_pool_destroy() */
void cera_pool_destroy(cera_pool_t *p)
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

    cera_pool_join(p);

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

/* {{{ cera_pool_worker_count() */
int cera_pool_worker_count(cera_pool_t *p)
{
    return p->n_workers;
}
/* }}} */

/* {{{ cera_pool_queue_stats() */
void cera_pool_queue_stats(cera_pool_t *p, int *capacity, int *high_water, int *growths)
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

/* }}} */

/* {{{ 019 — the station table */
/* ==================================================================
 *
 * 019 — the station table
 *
 * Was src/019-station.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
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
 * the mechanism. The depth itself is CERA_IN_PORT_DEFAULT_CAPACITY, declared
 * beside the record in the header, where a reader looks for the port's
 * contract. A port can ask for a different depth, so two places have
 * to agree on what "unless somebody says otherwise" is.
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
     * **An invalid operation ends the program**, with an exit code
     * saying which kind of fault it was: one a caller can correct and
     * retry.
     *
     * There is no map to hand over here, because this helper is
     * reached from places that hold one and places that do not, and a
     * report about the wrong program is worse than no report. What
     * dies with a report is the path that knows which program it was
     * editing.
     */
    char said[512];
    snprintf(said, sizeof said, "map construction: %s", what);
    cera_stop_now(NULL, CERA_EXIT_BAD_CALL, said);
}
/* }}} */

/* {{{ fail_resource() */
/*
 * The other kind, and the distinction is the point: out
 * of memory is not something a caller can correct and retry, so
 * anything that retries on failure has to be able to tell the two
 * apart in code rather than in prose.
 */
static void fail_resource(const char *what)
{
    char said[512];
    snprintf(said, sizeof said, "map construction: %s", what);
    cera_stop_now(NULL, CERA_EXIT_NO_RESOURCE, said);
}
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
 * A walk down a short list, and the cost paging charges.
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
static cera_in_port_page_t *in_port_page_at(const cera_in_port_t *sl, int page)
{
    cera_in_port_page_t *pg = sl->pages;
    while (page-- > 0 && pg)
        pg = atomic_load_explicit(&pg->next, memory_order_acquire);
    return pg;
}
/* }}} */

/* {{{ in_port_slot() */
static void *in_port_slot(const cera_in_port_t *sl, int index)
{
    cera_in_port_page_t *pg = in_port_page_at(sl, index / sl->page_slots);
    if (!pg) {
        /* An ordinal past the last page means the caller computed a
         * position the port does not have. Nothing should be able to:
         * the scan bounds itself by the capacity, and the capacity is
         * the sum of the pages. Stopping is right because continuing
         * would read whatever follows the list. */
        cera_fail(CERA_EXIT_BAD_CALL, "station: slot %d asked for on a port that has "
                        "%d\n", index, sl->capacity);
    }
    return pg->slots + (size_t)(index % sl->page_slots) * (size_t)sl->stride;
}
/* }}} */

/* {{{ in_port_add_page() */
/*
 * One more page on the end. Used both to give a port its first page
 * and to grow it, because they are the same act — which
 * is the shape the station table already uses one level up.
 */
static cera_in_port_page_t *in_port_add_page(cera_in_port_t *sl)
{
    /* The port does not know which station owns it, so the station
     * emits this one; here is only where it happens. */
    /* Zeroed rather than merely allocated, because a slot's state is
     * part of it and empty is zero — a fresh page has to
     * be a page of *empty* slots, or the first reader to reach it
     * would find whatever the allocator left behind and believe it. */
    cera_in_port_page_t *pg = calloc(1, sizeof *pg
                                + (size_t)sl->page_slots * (size_t)sl->stride);
    if (!pg) fail_resource("out of memory for a page of a ring buffer");

    if (!sl->pages) {
        sl->pages = pg;
    } else {
        cera_in_port_page_t *last = sl->pages;
        cera_in_port_page_t *next;
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
static void in_port_free_pages(cera_in_port_t *sl)
{
    cera_in_port_page_t *pg = sl->pages;
    while (pg) {
        cera_in_port_page_t *next = atomic_load_explicit(&pg->next,
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
 * and therefore already holds the address. Going back
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

/* {{{ slot_state_at() */
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
/* }}} */

/* {{{ slot_set_at() */
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
CERA_TEST_ONLY static int in_port_slot_move(const cera_in_port_t *sl, int index, int from, int to)
{
    return slot_move_at(in_port_slot(sl, index), sl->elem_size, from, to);
}
/* }}} */

/* {{{ add_shelf() */
/*
 * One more shelf, and its pointer written into the short array that
 * names them. That array holds addresses rather than mutexes, so
 * growing it by reallocation is safe — the same kind of copy the
 * pool's ring already does. No station record is ever copied.
 */
static int add_shelf(cera_map_t *m)
{
    cera_station_t *shelf = calloc(CERA_STATIONS_PER_SHELF, sizeof *shelf);
    if (!shelf)
        return -1;
    cera_station_t **shelves = realloc(m->shelves,
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

/* {{{ cera_map_create_empty() */
cera_map_t *cera_map_create_empty(void)
{
    cera_map_t *m = calloc(1, sizeof *m);
    if (!m) fail_resource("out of memory for the map");
    pthread_mutex_init(&m->rewire_mutex, NULL);
    pthread_mutex_init(&m->scrap_mutex, NULL);
    return m;
}
/* }}} */

/* {{{ cera_map_create() */
cera_map_t *cera_map_create(int n_stations)
{
    if (n_stations <= 0)
        fail("a map needs at least one station");

    cera_map_t *m = calloc(1, sizeof *m);
    if (!m) fail_resource("out of memory for the map");

    pthread_mutex_init(&m->rewire_mutex, NULL);
    pthread_mutex_init(&m->scrap_mutex, NULL);

    /*
     * Shelves enough for what was asked for. Asking for a
     * count up front is now a convenience rather than a commitment:
     * the table grows a shelf at a time afterwards, and nothing
     * already placed ever moves.
     *
     * Reserved directly rather than by calling cera_map_add_station in a
     * loop, because that call hands back the first place nobody has
     * filled — which is the same place every time until somebody
     * fills it. Reserving N places and filling them is a different
     * act from asking for somewhere to put one thing.
     */
    while (n_stations > m->n_shelves * CERA_STATIONS_PER_SHELF)
        if (add_shelf(m) < 0)
            fail_resource("out of memory for the station table");
    atomic_store_explicit(&m->n_stations, n_stations, memory_order_release);

    return m;
}
/* }}} */

/* {{{ cera_map_add_station() */
int cera_map_add_station(cera_map_t *m)
{
    /* Exclusive, under the lock every other structural change already
     * takes. Two threads each finding the same free place,
     * or each deciding the shelves are full, would otherwise hand two
     * callers one index. The hand-raising ring the note asked for is
     * not built and is moot: adding a shelf is one allocation and one
     * pointer write, so there is no long stretch for anybody to raise a
     * hand during. `strategems/raise-your-hand.md` keeps the pattern
     * and the lesson that displaced it. */
    pthread_mutex_lock(&m->rewire_mutex);

    /*
     * A freed place first. Removing a station is what removes the
     * wires to it, so a place whose shim is clear holds
     * nothing stale and can simply be taken. A program that adds and
     * removes forever therefore reaches a steady size rather than
     * climbing.
     */
    int count = atomic_load_explicit(&m->n_stations, memory_order_acquire);
    for (int i = 0; i < count; i++) {
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call && !atomic_load_explicit(&s->removed,
                                              memory_order_acquire)) {
            pthread_mutex_unlock(&m->rewire_mutex);
            return i;
        }
    }

    if (count >= m->n_shelves * CERA_STATIONS_PER_SHELF && add_shelf(m) < 0) {
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

/* {{{ cera_map_place() */
void cera_map_place(cera_map_t *m, int station, cera_task_call_t shim, int kind,
               int n_in_ports, const int *elem_sizes, int out_size)
{
    if (station < 0 || station >= m->n_stations)
        fail("placing a box at a station index outside the table");
    if (kind < 0 || kind >= CERA_STATION_KIND_COUNT)
        fail("placing a box of a kind that does not exist");
    if (n_in_ports < 0)
        fail("a station cannot have a negative number of slots");

    cera_station_t *s = cera_map_station(m, station);
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
        cera_in_port_t *sl = &s->in_ports[i];
        if (elem_sizes[i] <= 0)
            fail("a port's element size must be positive");
        /* Every port starts life as a ring buffer — the default the
         * map format also assumes. Becoming a static, or
         * having its source taken away, is a conversion applied
         * afterwards; neither one frees what is allocated here.
         * */
        sl->kind = CERA_IN_PORT_RING;
        sl->elem_size = elem_sizes[i];
        sl->stride = slot_stride(sl->elem_size);
        /* The first page, which is the same act as growing
         * : a port with one page and a port with nine differ
         * only in how many times this has happened. */
        sl->pages = NULL;
        sl->capacity = 0;
        sl->page_slots = CERA_IN_PORT_DEFAULT_CAPACITY;
        in_port_add_page(sl);
        sl->read_hint = 0;
        sl->write_hint = 0;
        sl->held = 0;

        /* The other storage, allocated at the same moment and for the
         * same reason: a port has room for both a
         * buffer and a constant whatever it is currently for, so
         * changing which one is in effect is a field write and never
         * an allocation. Zeroed, so a port whose constant has not been
         * set holds zeroes rather than whatever was there — though
         * nothing reads it until constant_set says somebody wrote it. */
        sl->constant = calloc(1, (size_t)sl->elem_size);
        if (!sl->constant) fail_resource("out of memory for a port's constant");
        sl->constant_string = NULL;
        sl->constant_set = 0;

        /* Not a door until something says otherwise, and said
         * explicitly because zero is a perfectly good argument
         * number. */
        sl->argument = CERA_NOT_A_DOOR;
    }

    CERA_EMIT(m, CERA_WATCH_ADDED, station, kind, n_in_ports, out_size, 0);
}
/* }}} */

/* {{{ station_label_into() */
/*
 * The name a map file gave a station, or its index when nothing gave
 * it one. A program built by calling the surface has no names, and a
 * complaint that says "?" about it is one nobody can act on.
 */
static void station_label_into(cera_map_t *m, int i, char *out, size_t room)
{
    if (m->station_names && i < m->n_named && m->station_names[i])
        snprintf(out, room, "%s", m->station_names[i]);
    else
        snprintf(out, room, "%d", i);
}
/* }}} */

/* {{{ no_such_port_into() */
/*
 * **One sentence for "that port does not exist", written once**, where
 * every caller reaches it. It names the box, counts its ports, and
 * remembers that a comparator carries one more than its parameter list
 * shows. A caller reading a file prefixes it with the file and line.
 *
 * The threshold clause is why this cannot be a format string at each
 * call site: a comparator's last port is not one of the box's
 * parameters, so a reader counting parameters in the box source finds
 * one fewer than the refusal names and concludes the engine is
 * confused.
 */
static void no_such_port_into(cera_map_t *m, int station, int port,
                              char *out, size_t room)
{
    cera_station_t *s = cera_map_station(m, station);
    char who[64];
    station_label_into(m, station, who, sizeof who);
    snprintf(out, room,
             "station '%s' has no port %d — '%s' has %d port%s (its "
             "parameters%s)",
             who, port, s->box_name ? s->box_name : "?", s->n_in_ports,
             s->n_in_ports == 1 ? "" : "s",
             s->kind == CERA_STATION_COMPARATOR ? ", plus the threshold" : "");
}
/* }}} */

/* {{{ cera_map_in_port_start_depth() */
/*
 * **A refusal travels rather than stopping here.** A caller reading a
 * file collects every mistake in it and presents them together, and it
 * cannot collect what killed the process.
 */
const char *cera_map_in_port_start_depth(cera_map_t *m, int station, int port, int slots)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said,
                 "station %d is outside the table", station);
        return said;
    }
    cera_station_t *s = cera_map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        no_such_port_into(m, station, port, said, sizeof said);
        return said;
    }
    /* One slot is a legitimate depth: a slot carries its own state, so
     * no spare is held back and every slot is usable. */
    if (slots < 1) {
        snprintf(said, sizeof said,
                 "a ring buffer needs at least one slot, and %d was asked for",
                 slots);
        return said;
    }

    cera_in_port_t *sl = &s->in_ports[port];
    if (sl->held != 0) {
        char who[64];
        station_label_into(m, station, who, sizeof who);
        snprintf(said, sizeof said,
                 "%s.%d already holds values — this is a starting depth, and "
                 "the start has been and gone", who, port);
        return said;
    }

    /* The starting depth sets the **page size**, not merely the first
     * page's size. Every page a port ever adds is this
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

/* {{{ cera_map_in_port_convert() */
void cera_map_in_port_convert(cera_map_t *m, int station, int port, int kind)
{
    /* A case of the one configuration operation, kept as
     * a name because "convert this port" is what callers already say.
     * Passing no text means *the value it had before*, which is why
     * becoming a static again works and becoming one for the first
     * time is refused here. */
    const char *no = cera_map_configure_port(m, station, port, kind, NULL);
    if (no)
        fail(no);
}
/* }}} */

/* {{{ cera_map_configure_port() */
/*
 * **The one operation that says where a port's values come from**
 *: a station, a port, a source, and — when the source is
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
 * survives being converted away exactly as waiting values do
 *  — and which is refused when there is no such value, because
 * the tag would then be in effect over storage nobody ever wrote.
 *
 * **Returns a refusal rather than stopping the program**, so that a
 * caller reading a file can collect every mistake in it and present
 * them together instead of one per run.
 *
 * One thing still stops the program: text that does not parse. The
 * reader dies where the malformed value is, naming the station, the
 * port and the field, and moving that onto this return path belongs
 * with the rest of the refusal policy rather than being half done
 * here.
 */
const char *cera_map_configure_port(cera_map_t *m, int station, int port,
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
    cera_station_t *s = cera_map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        /* The loader's wording, which named the box and remembered
         * the comparator's threshold, moved down here so that every
         * caller gets it and nobody keeps a second copy. */
        no_such_port_into(m, station, port, said, sizeof said);
        return said;
    }
    if (source < 0 || source >= CERA_IN_PORT_KIND_COUNT) {
        snprintf(said, sizeof said,
                 "there is no such source for a port");
        return said;
    }

    if (source == CERA_IN_PORT_STATIC) {
        if (text) {
            /* Binding parses the text and sets the tag together, so a
             * value that will not parse never leaves the port in a
             * state that claims to hold one. */
            cera_map_in_port_static_text(m, station, port, text);
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

/* {{{ cera_map_check_sources() */
/*
 * **Every parameter needs somewhere to get a value**.
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
 * could do without was proposed and refused, because it
 * would have been the only exemption to the rule that a station runs
 * when every one of its slots holds a value. So this is unqualified:
 * a port with no source is an error, full stop.
 */
const char *cera_map_check_sources(cera_map_t *m)
{
    static _Thread_local char said[512];
    int used = 0, found = 0;

    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        /* An empty place in the table is not a station. */
        if (!s->call)
            continue;
        for (int j = 0; j < s->n_in_ports; j++) {
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) != CERA_IN_PORT_NONE)
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

/* {{{ cera_map_name_station() */
/*
 * **What to call a station**, which is the sixth thing
 * construction has to be able to say.
 *
 * The engine never reads these — every wire is an index, and that is
 * deliberate. They exist so a program can be *written back out* as a
 * file that reads in again, and so a person watching a live view sees
 * something other than numbers. A program with no names still runs
 * perfectly; it simply cannot be described on disk.
 *
 * It is an operation like any other, so a program built by calling
 * this surface dumps the same as one read from a file. That
 * comparison is the proof that there is one construction path rather
 * than two agreeing by coincidence.
 *
 * The array grows with the table, since stations are added one at a
 * time rather than counted in advance.
 */
const char *cera_map_name_station(cera_map_t *m, int station, const char *name)
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

/* {{{ cera_map_station_set_cursor() */
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
const char *cera_map_station_set_cursor(cera_map_t *m, int station, int at)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    cera_station_t *s = cera_map_station(m, station);
    if (s->kind != CERA_STATION_ITERATOR) {
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

/* {{{ cera_map_designate_result() */
/*
 * **Say that this port is where one of the program's results leaves**
 * (issue 209a).
 *
 * It stays an ordinary output port: same routing, same fan-out, values
 * discarded when nothing is wired to it. The mark adds no rule at all
 * on its own — what it does is give an embedding caller a number to
 * ask for, so that registering somewhere to put the values becomes
 * possible. Until somebody registers, a marked port and an unmarked
 * one behave identically, which is why a program nobody collects from
 * cannot pile anything up.
 *
 * **The number is the result's identity**, chosen by whoever wrote the
 * map rather than derived from where the line sits in it. Reorder
 * every line and nothing changes; delete one and that result is gone
 * rather than silently becoming what the next one was.
 *
 * A program may have as many as it likes, on as many stations as it
 * likes, and they are **not synchronised with one another** — two
 * results are two stations on two threads at two unrelated moments.
 * That is stated wherever anybody has to act on it, because two arrays
 * filling side by side look like columns of a table and are not.
 */
const char *cera_map_designate_result(cera_map_t *m, int station, int port,
                                      int nth)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    cera_station_t *s = cera_map_station(m, station);
    if (!s->call) {
        snprintf(said, sizeof said,
                 "station %d has no box placed — place, then designate",
                 station);
        return said;
    }
    if (s->out_size == 0) {
        /* A station whose box returns nothing has no output port, so
         * there is nothing to leave by. Refused rather than
         * accepted-and-useless, because the mistake is almost
         * certainly the wrong station. */
        snprintf(said, sizeof said,
                 "station %d returns nothing, so it has no results to be "
                 "the source of", station);
        return said;
    }
    if (nth < 0) {
        snprintf(said, sizeof said,
                 "a result's number says which result it is, so it cannot "
                 "be negative");
        return said;
    }

    /* Under the station's own mutex: creating a port appends to a list
     * a delivery walk may be reading. */
    pthread_mutex_lock(&s->mutex);
    cera_out_port_t *p = station_out_port_make(s, port);
    if (p)
        p->result = nth;
    pthread_mutex_unlock(&s->mutex);

    if (!p)
        return "the output port would not be created";
    return NULL;
}
/* }}} */

/* {{{ cera_map_start_beside() */
cera_map_t *cera_map_start_beside(cera_map_t *parent)
{
    if (!parent->pool)
        fail("starting a program beside one that has not started itself");

    cera_map_t *m = cera_map_create_empty();
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

/* {{{ cera_map_designate_input() */
/*
 * **Say that this port is one of the program's arguments** (issue
 * 213a), which is the other door and the same design.
 *
 * Without a mark somewhere, a value gets into a running program
 * exactly one way: somebody holding it names a station and a port.
 * That works, and it means **the caller has to know the program's
 * insides** — rename an interior station and every caller breaks.
 * That is not encapsulation; the program has no surface, only
 * internals that happen to be reachable.
 *
 * **The mark is on the port, and an argument therefore costs
 * nothing.** It used to be on the station, and because a station
 * carries one value inward — a C function returns one thing — a map
 * taking three arguments needed three stations running the identity
 * function, each with its own mutex and ring buffer, each turning one
 * delivery into a task, a dispatch, a call that returns its argument,
 * a readiness check and a second delivery. All of that was the mark
 * having nowhere smaller to live.
 *
 * **A port that is both marked and wired is fed both ways**, and that
 * is legal. Being an argument is a fact about who may deliver here;
 * being wired is a fact about what already does. Command-line
 * delivery, which needs one value per argument in a fixed order, asks
 * for the ports that nothing feeds — derived, so nothing can go stale
 * and nothing has to be closed when an enclosing map wires in.
 *
 * A station may hold ports of both kinds. The old refusal — a station
 * cannot be both doors — existed because the mark was on the station
 * and a station is one thing; a port is a smaller thing, and a station
 * with an argument port and a result port is ordinary rather than a
 * mistake.
 */
const char *cera_map_designate_argument(cera_map_t *m, int station, int port,
                                        int nth)
{
    static _Thread_local char said[192];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    cera_station_t *s = cera_map_station(m, station);
    if (!s->call) {
        snprintf(said, sizeof said,
                 "station %d has no box placed — place, then designate",
                 station);
        return said;
    }
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said,
                 "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    if (nth < 0) {
        snprintf(said, sizeof said,
                 "an argument's number says which argument it is, so it "
                 "cannot be negative");
        return said;
    }

    s->in_ports[port].argument = nth;
    return NULL;
}
/* }}} */

/* {{{ port_is_fed() */
/* Whether any station's output port names this one as a destination.
 * The same sweep removal does, and for the same reason: a wire lives
 * on the producing side, and an input port carries nothing saying what
 * feeds it. */
static int port_is_fed(cera_map_t *m, int station, int port)
{
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call)
            continue;
        for (cera_out_port_t *p = s->out_ports; p; p = p->next) {
            cera_dest_set_t *set = out_port_dests(p);
            for (int d = 0; set && d < set->n; d++)
                if (set->items[d].station == station
                    && set->items[d].port == port)
                    return 1;
        }
    }
    return 0;
}
/* }}} */

/* {{{ argument_slots() */
/*
 * **The marked ports a command line fills**, in the order their
 * numbers say, and how many there are.
 *
 * A port that is both marked and wired is fed both ways and is *not* a
 * slot: a command line hands over one value per position, and a port
 * already receiving values from inside the program has no position to
 * be at. Skipped rather than refused, because it is a perfectly
 * ordinary thing for an enclosing map to have done.
 *
 * This is what replaced closing a door. Nothing is stored, so nothing
 * can go stale, and wiring into a marked port needs no bookkeeping at
 * all — the answer is recomputed from the wires that exist.
 */
static int argument_slots(cera_map_t *m, int *station, int *port, int room)
{
    int found = 0;
    for (int nth = 0; found < room; nth++) {
        int at = -1, which = -1;
        if (!cera_map_argument_at(m, nth, &at, &which))
            break;
        if (port_is_fed(m, at, which))
            continue;
        station[found] = at;
        port[found] = which;
        found++;
    }
    return found;
}
/* }}} */

/* {{{ cera_map_argument_at() */
/*
 * **Where the nth argument goes**, or zero when the program has no
 * such argument.
 *
 * Walked rather than indexed, because the numbers are the author's
 * and need not be dense or in table order — a map may write its
 * arguments in any sequence, and the whole point of numbering them is
 * that where the line sits does not matter.
 */
int cera_map_argument_at(cera_map_t *m, int nth, int *station, int *port)
{
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call)
            continue;
        for (int j = 0; j < s->n_in_ports; j++)
            if (s->in_ports[j].argument == nth) {
                if (station) *station = i;
                if (port)    *port = j;
                return 1;
            }
    }
    return 0;
}
/* }}} */

/* {{{ cera_map_result_at() */
/* The same question about the other direction. */
int cera_map_result_at(cera_map_t *m, int nth, int *station, int *port)
{
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call)
            continue;
        int j = 0;
        for (cera_out_port_t *p = s->out_ports; p; p = p->next, j++)
            if (p->result == nth) {
                if (station) *station = i;
                if (port)    *port = j;
                return 1;
            }
    }
    return 0;
}
/* }}} */

/* {{{ cera_map_deliver_argument() */
/*
 * **Deliver a value from outside the program**.
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
const char *cera_map_deliver_argument(cera_map_t *m, int station, int port,
                                 const void *value, int size)
{
    static _Thread_local char said[224];

    /*
     * **Shut, because somebody asked this program to wind down**.
     * The entrance is the only way anything outside puts
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
    cera_station_t *s = cera_map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said,
                 "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    if (s->in_ports[port].argument == CERA_NOT_A_DOOR) {
        snprintf(said, sizeof said,
                 "station %d port %d is not one of this program's arguments "
                 "— the outside may only deliver to a marked port", station,
                 port);
        return said;
    }
    if (size != s->in_ports[port].elem_size) {
        snprintf(said, sizeof said,
                 "that port takes %d bytes and %d were offered",
                 s->in_ports[port].elem_size, size);
        return said;
    }

    cera_map_deliver_value(m, station, port, value);
    return NULL;
}
/* }}} */

/* {{{ cera_map_collect() */
/*
 * **Where an embedding caller wants a result's values put** (issue
 * 209a), and the end of the pile that used to grow behind its back.
 *
 * What was here before held every value a marked station produced, in
 * an array that doubled whenever it filled, and shouted from the first
 * growth that *results are piling up and nobody is taking them*. That
 * warning was written instead of a fix: a program whose results nobody
 * drained grew until memory ran out, having been told so on the way.
 *
 * **Nothing is held unless somebody asks for it.** Registering is the
 * missing arrow. Before it, a marked output behaves exactly like any
 * other unwired output — the value is discarded — so the state the
 * warning described cannot happen.
 *
 * **The caller owns the memory.** An address, a count, and an element
 * size; the engine allocates nothing and therefore has nothing that
 * can grow.
 *
 * **Wire before starting.** This is not new discipline — it is the
 * rule the pool already enforces, that a standing promise is held from
 * before the workers are released until the last argument is in, and
 * the engine already has a message for somebody who got that backwards.
 * Registering after the values have started arriving loses the ones
 * that arrived first, silently, which is the shape this refuses to
 * have.
 */
const char *cera_map_collect(cera_map_t *m, int station, int port,
                             void *into, int room, int elem_size)
{
    static _Thread_local char said[224];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    cera_station_t *s = cera_map_station(m, station);
    if (!s->call) {
        snprintf(said, sizeof said,
                 "station %d has no box placed, so it produces nothing to "
                 "collect", station);
        return said;
    }
    if (!into || room <= 0) {
        snprintf(said, sizeof said,
                 "collecting into nowhere — an address and a count say "
                 "where the values go and how many fit");
        return said;
    }
    if (elem_size != s->out_size) {
        snprintf(said, sizeof said,
                 "that station produces %d bytes and the array holds %d "
                 "per slot", s->out_size, elem_size);
        return said;
    }

    pthread_mutex_lock(&s->mutex);
    cera_out_port_t *p = station_out_port_make(s, port);
    if (p) {
        p->into = into;
        p->room = room;
        p->elem_size = elem_size;
        atomic_store_explicit(&p->taken, 0, memory_order_release);
    }
    pthread_mutex_unlock(&s->mutex);

    if (!p)
        return "the output port would not be created";
    return NULL;
}
/* }}} */

/* {{{ cera_map_collected() */
/*
 * **How many values landed**, which is a count and not a position.
 *
 * There is no progress through a program to report — a map is
 * runnable or not, and nothing about it is ordered. What this number
 * distinguishes is the two ways a run ends: reaching the count means
 * the program produced at least everything that was asked for, and
 * falling short of it while the pool has finished means the program
 * ran dry and that was all there was.
 */
int cera_map_collected(cera_map_t *m, int station, int port)
{
    if (station < 0 || station >= m->n_stations)
        return 0;
    cera_station_t *s = cera_map_station(m, station);
    cera_out_port_t *p = station_out_port(s, port);
    if (!p || !p->into)
        return 0;
    int n = atomic_load_explicit(&p->taken, memory_order_acquire);
    return n > p->room ? p->room : n;
}
/* }}} */

/* {{{ cera_map_bring_up() */
const char *cera_map_bring_up(cera_map_t *m)
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
    const char *unsourced = cera_map_check_sources(m);
    if (unsourced)
        fprintf(stderr, "map: WARNING: %s — %s will not run until %s\n",
                unsourced,
                strchr(unsourced, ';') ? "those stations" : "that station",
                strchr(unsourced, ';') ? "they are finished"
                                       : "it is finished");

    /*
     * **Which ports arrows land on, worked out once for the whole
     * program**.
     *
     * Walk every destination in the program once, marking where each
     * one lands. The work is proportional to the stations plus the
     * wires rather than to the square of the stations, which matters
     * because a program brought inside another produces one table
     * holding both.
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
        cera_station_t *s = cera_map_station(m, i);
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
        cera_station_t *other = cera_map_station(m, k);
        for (cera_out_port_t *p = other->out_ports; p; p = p->next) {
            cera_dest_set_t *set = out_port_dests(p);
            for (int di = 0; set && di < set->n; di++) {
                int at = set->items[di].station;
                int port = set->items[di].port;
                if (at < 0 || at >= m->n_stations)
                    continue;
                cera_station_t *dest = cera_map_station(m, at);
                if (!dest->call || port < 0 || port >= dest->n_in_ports)
                    continue;
                landed[first_port[at] + port] = 1;
            }
        }
    }

    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call)
            continue;   /* an empty place is not a station */

        char who[64];
        station_label_into(m, i, who, sizeof who);

        const unsigned char *landed_on = landed + first_port[i];

        int has_ring = 0, any_arrow = 0;
        for (int j = 0; j < s->n_in_ports; j++) {
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) == CERA_IN_PORT_RING)
                has_ring = 1;
            any_arrow |= landed_on[j];
        }

        /* An arrow landing on a port with no source would have
         * nowhere to put its value at all — nothing to queue into and
         * nothing to overwrite.
         *
         * **A static destination is not one of these.** A value
         * arriving there overwrites the constant, which is how a
         * constant gets computed at startup rather than written down.
         * It is a property of the wire rather than of the box, so it
         * is visible in the map file. */
        for (int j = 0; j < s->n_in_ports; j++) {
            unsigned char k = atomic_load_explicit(&s->in_ports[j].kind,
                                                   memory_order_relaxed);
            if (landed_on[j] && k != CERA_IN_PORT_RING && k != CERA_IN_PORT_STATIC) {
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
         * delivers into. Warning about it would be
         * telling somebody that the thing they just declared might
         * not happen.
         */
        int any_argument = 0;
        for (int j = 0; j < s->n_in_ports; j++)
            if (s->in_ports[j].argument != CERA_NOT_A_DOOR)
                any_argument = 1;

        if (has_ring && !any_arrow && !any_argument)
            fprintf(stderr,
                    "map: WARNING: station %s has buffered inputs that no "
                    "arrow feeds — unless something outside delivers into "
                    "it, it will never run\n", who);
    }

    free(landed);
    free(first_port);

    /*
     * **The doors are numbered without gaps and without repeats**
     * (issues 213a, 209a), which is the whole of what numbering them
     * costs and the reason it is worth anything.
     *
     * A number is the door's identity, chosen by whoever wrote the map
     * rather than derived from where the line sits in it. That is only
     * true if the engine holds the author to it: two ports both
     * claiming to be argument one is a program with no answer to
     * "which one does the first value go to", and argument two with no
     * argument one is a command line that cannot be counted.
     *
     * Neither was detectable at all under the old scheme, where the
     * order was the order stations happened to sit in the table.
     *
     * **A program need no longer declare a result.** That requirement
     * existed so a program's interface would be *total* — so that "I
     * produce nothing" and "I forgot to say" were different
     * statements. An interface made of numbered ports is total by
     * being read: a map with no result mark produces nothing outward,
     * and that is a complete sentence needing no separate declaration.
     */
    /*
     * **Only the doors nobody inside has taken over count**, which is
     * what makes this safe under composition. One description
     * instantiated twice puts two ports in the table both marked
     * argument zero — and they are not two of the program's arguments,
     * they are each copy's own, and the enclosing map has wired both.
     * A port something feeds is not a way in from outside, so it is not
     * one of these numbers; the same derivation the command line uses.
     */
    for (int nth = 0; ; nth++) {
        int found = 0;
        for (int i = 0; i < m->n_stations; i++) {
            cera_station_t *s = cera_map_station(m, i);
            if (!s->call)
                continue;
            for (int j = 0; j < s->n_in_ports; j++)
                if (s->in_ports[j].argument == nth && !port_is_fed(m, i, j))
                    found++;
        }
        if (found > 1) {
            faults++;
            if (used < (int)sizeof said - 128)
                used += snprintf(said + used, sizeof said - (size_t)used,
                                 "%s%d ports each say they are argument %d, "
                                 "so there is no answer to which one a "
                                 "value goes to", used ? "; " : "",
                                 found, nth);
        }
        if (found == 0) {
            /* The end of the run, unless something numbered higher is
             * sitting past the gap — in which case the gap is the
             * fault, because an argument list with a hole in it cannot
             * be counted off a command line. */
            int beyond = 0;
            for (int i = 0; i < m->n_stations && !beyond; i++) {
                cera_station_t *s = cera_map_station(m, i);
                if (!s->call)
                    continue;
                for (int j = 0; j < s->n_in_ports; j++)
                    if (s->in_ports[j].argument > nth
                        && !port_is_fed(m, i, j))
                        beyond = 1;
            }
            if (beyond) {
                faults++;
                if (used < (int)sizeof said - 128)
                    used += snprintf(said + used,
                                     sizeof said - (size_t)used,
                                     "%snothing is argument %d, and "
                                     "something is numbered past it — an "
                                     "argument list cannot have a hole in "
                                     "it", used ? "; " : "", nth);
            }
            break;
        }
    }

    for (int nth = 0; ; nth++) {
        int found = 0, beyond = 0;
        for (int i = 0; i < m->n_stations; i++) {
            cera_station_t *s = cera_map_station(m, i);
            if (!s->call)
                continue;
            for (cera_out_port_t *p = s->out_ports; p; p = p->next) {
                /* A result wired onward is feeding something inside the
                 * program, which is what an enclosing map does to a
                 * sub-map's way out. Only the ones going nowhere are
                 * the program's own. */
                cera_dest_set_t *set = out_port_dests(p);
                if (set && set->n > 0)
                    continue;
                if (p->result == nth)   found++;
                if (p->result > nth)    beyond = 1;
            }
        }
        if (found > 1) {
            faults++;
            if (used < (int)sizeof said - 128)
                used += snprintf(said + used, sizeof said - (size_t)used,
                                 "%s%d ports each say they are result %d",
                                 used ? "; " : "", found, nth);
        }
        if (found == 0) {
            if (beyond) {
                faults++;
                if (used < (int)sizeof said - 128)
                    used += snprintf(said + used,
                                     sizeof said - (size_t)used,
                                     "%snothing is result %d, and something "
                                     "is numbered past it",
                                     used ? "; " : "", nth);
            }
            break;
        }
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
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call || s->seeded)
            continue;

        int has_ring = 0;
        for (int j = 0; j < s->n_in_ports; j++)
            if (atomic_load_explicit(&s->in_ports[j].kind,
                                     memory_order_relaxed) == CERA_IN_PORT_RING)
                has_ring = 1;
        if (has_ring)
            continue;

        /* Through the same door delivery uses — one way a task comes
         * into existence, not two. */
        s->seeded = 1;
        if (cera_map_station_try_start(m, i))
            m->seeded++;
    }

    CERA_EMIT(m, CERA_WATCH_UP, m->n_stations,
              m->pool ? cera_pool_worker_count(m->pool) : 0, m->seeded, 0, 0);
    return NULL;
}
/* }}} */

/* {{{ in_port_kind_name() */
static const char *in_port_kind_name(unsigned char kind)
{
    static const char *const names[CERA_IN_PORT_KIND_COUNT] = {
        [CERA_IN_PORT_RING]   = "a buffer",
        [CERA_IN_PORT_STATIC] = "a static value",
        [CERA_IN_PORT_NONE]   = "a port with no source yet",
    };
    return kind < CERA_IN_PORT_KIND_COUNT ? names[kind]
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
static cera_out_port_t *station_out_port(cera_station_t *s, int index)
{
    cera_out_port_t *p = s->out_ports;
    for (int i = 0; p && i < index; i++)
        p = p->next;
    return p;
}
/* }}} */

/* {{{ station_out_port_make() */
/*
 * The port at an index, creating it and every port before it if they
 * are not there yet. Two callers want this — drawing a wire and
 * marking a result — and having one of them build ports inline while
 * the other did it differently is how two ports end up meaning
 * slightly different things.
 *
 * **The caller holds the station's mutex.** Creating a port appends to
 * a list a delivery walk may be reading.
 *
 * A fresh port is not a door: `calloc` gives zero, and zero is a
 * perfectly good result number, so the mark is written explicitly.
 */
static cera_out_port_t *station_out_port_make(cera_station_t *s, int index)
{
    while (s->n_out_ports <= index) {
        cera_out_port_t *fresh = calloc(1, sizeof *fresh);
        if (!fresh)
            return NULL;
        fresh->result = CERA_NOT_A_DOOR;
        cera_out_port_t **link = &s->out_ports;
        while (*link)
            link = &(*link)->next;
        *link = fresh;
        s->n_out_ports++;
    }
    return station_out_port(s, index);
}
/* }}} */

/* {{{ out_port_dests() */
static cera_dest_set_t *out_port_dests(const cera_out_port_t *p)
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
static cera_dest_set_t *dest_set_build(const cera_dest_set_t *from, int add_station,
                           int add_port, int drop_station, int drop_port)
{
    int old_n = from ? from->n : 0;
    int n = old_n + (add_station >= 0 ? 1 : 0);
    cera_dest_set_t *set = calloc(1, sizeof *set + (size_t)(n > 0 ? n : 1)
                                              * sizeof(cera_destination_t));
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

/* {{{ nobody_can_hold() */
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
static int nobody_can_hold(cera_map_t *m, const struct scrap_item *it)
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
        uint64_t now = cera_pool_worker_epoch(m->pool, i);
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
static void map_scrap_sweep(cera_map_t *m)
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
static void map_retire(cera_map_t *m, void *p, void (*free_fn)(void *))
{
    if (!p)
        return;

    /* Sweep before filing, so the work happens exactly where the need
     * is created and a program that changes shape forever reclaims as
     * it goes. */
    map_scrap_sweep(m);

    int workers = m->pool ? cera_pool_worker_count(m->pool) : 0;
    uint64_t *snapshot = NULL;
    if (workers > 0) {
        snapshot = calloc((size_t)workers, sizeof *snapshot);
        if (!snapshot)
            fail_resource("out of memory retiring something");
        for (int i = 0; i < workers; i++)
            snapshot[i] = cera_pool_worker_epoch(m->pool, i);
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
CERA_TEST_ONLY static int map_scrap_count(cera_map_t *m)
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
static void map_scrap_free_all(cera_map_t *m)
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

/* {{{ cera_map_connect() */
void cera_map_connect(cera_map_t *m, int from_station, int port,
                 int to_station, int to_port)
{
    /*
     * A face on the one wiring operation, for a caller
     * that wants a refusal to stop the program.
     *
     * One implementation, one set of rules: a program built by hand
     * and the same program read from a file are refused or accepted
     * alike.
     */
    const char *no = cera_map_wire(m, from_station, port, to_station, to_port);
    if (no)
        fail(no);
}
/* }}} */

/* {{{ cera_map_start() */
void cera_map_start(cera_map_t *m, int n_workers)
{
    if (m->pool)
        fail("the map was already started");
    /* Delivery rides the pool's finish hook: after a worker runs a
     * task, the map decides where its output goes. This is the whole
     * of the pool's knowledge of the engine — one function pointer. */
    m->pool = cera_pool_create(n_workers, map_deliver, m);

    /* Nothing here holds process-wide state, so two maps can run side
     * by side and not see each other. Writing a static takes a real
     * address — a station and a port — from outside the graph, where
     * every other configuration change comes from. */
}
/* }}} */

/* {{{ cera_map_in_port_depth() */
int cera_map_in_port_depth(cera_map_t *m, int station, int port)
{
    cera_station_t *s = cera_map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        fail("asking the depth of a port that does not exist");
    cera_in_port_t *sl = &s->in_ports[port];

    /* A maintained count rather than index arithmetic: values are
     * claimed wherever they sit, so the distance between two indices
     * does not describe how many are waiting.
     *
     * A port that is not a buffer answers honestly. A static reports
     * whatever its slots carry; those values are waiting, and will be
     * served if it becomes a buffer again. */
    pthread_mutex_lock(&s->mutex);
    int depth = sl->held;
    pthread_mutex_unlock(&s->mutex);
    return depth;
}
/* }}} */

/* {{{ cera_map_destroy() */
void cera_map_destroy(cera_map_t *m)
{
    /*
     * The count that rides out on the closing event, and **the walk
     * that produces it, both inside the flag**.
     *
     * The arguments to an emit vanish when watching is compiled out,
     * but anything computed on the line above it does not: an unwatched
     * program was reading an atomic per station on every teardown to
     * feed an event it never sends. A watched program may be slower
     * than an unwatched one; an unwatched one pays nothing.
     */
#ifdef CERA_WATCH
    long total = 0;
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        if (s->call)
            total += atomic_load_explicit(&s->runs, memory_order_relaxed);
    }
    CERA_EMIT(m, CERA_WATCH_DONE, total, 0, 0, 0, 0);
#endif
    cera_watch_close(m);

    cera_map_observe_stop(m);
    /* A borrowed pool belongs to the program that made it, and other
     * programs may still be running on it. */
    if (m->pool && !m->pool_is_borrowed)
        cera_pool_destroy(m->pool);
    cera_map_report_shutdown(m);
    if (m->station_names) {
        /* Over what the array actually holds, not over the station
         * count: stations are added one at a time and the names grow
         * behind them, so the two are not always equal. */
        for (int i = 0; i < m->n_named; i++)
            free(m->station_names[i]);
        free(m->station_names);
    }

    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call) {
            pthread_mutex_destroy(&s->mutex);
            continue;
        }
        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_free_pages(&s->in_ports[j]);
            /* Both storages, because a port carries both whatever it
             * was being used for. */
            in_port_constant_free(&s->in_ports[j]);
        }
        free(s->in_ports);
        cera_out_port_t *p = s->out_ports;
        while (p) {
            free(out_port_dests(p));
            cera_out_port_t *next = p->next;
            free(p);
            p = next;
        }
        pthread_mutex_destroy(&s->mutex);
    }
    /* Everything a rewire replaced and left filed. By now the pool is
     * gone, so nothing can be walking any of it. */
    map_scrap_free_all(m);
    pthread_mutex_destroy(&m->scrap_mutex);
    pthread_mutex_destroy(&m->rewire_mutex);
    for (int i = 0; i < m->n_shelves; i++)
        free(m->shelves[i]);
    free(m->shelves);
    free(m);
}
/* }}} */

/* }}} */

/* {{{ 020 — delivery, readiness, routing */
/* ==================================================================
 *
 * 020 — delivery, readiness, routing
 *
 * Was src/020-delivery.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
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
 * There is no pull path; see
 * docs/implementation-notes/056-no-pull-path.md.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Timing exists only when CERA_STATS is compiled in —
 * a clock read per box on short boxes is real overhead, and a
 * measurement apparatus that cannot be removed is a tax. The macros
 * vanish entirely without the define.
 */
#ifdef CERA_STATS
#include <time.h>
/* {{{ stats_now_ns() */
static long stats_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}
/* }}} */
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
    cera_bug("delivery: %s (station %d)\n", what, station);
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Port motion. Callers hold the station's mutex.   */
/* ------------------------------------------------------------------ */

/* {{{ in_port_scan() */
/*
 * The scan. Start where the hint says, sweep forward,
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
static int in_port_scan(cera_in_port_t *sl, int *hint, int from, int to,
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
     * The pages are walked rather than indexed. Resolving
     * a slot's ordinal to a page costs a walk down the list, so doing
     * it per candidate would make a sweep quadratic in the number of
     * pages. It is done **once**, here, and the sweep then follows
     * `next` — one pointer hop per page boundary and plain pointer
     * arithmetic in between.
     */
    int page = start / sl->page_slots;
    int off  = start % sl->page_slots;
    cera_in_port_page_t *pg = sl->pages;
    for (int i = 0; i < page && pg; i++)
        pg = pg->next;

    for (int i = 0; i < cap && pg; i++) {
        void *slot = pg->slots + (size_t)off * (size_t)sl->stride;
        /*
         * Two ways to ask, and which one is right is a property of
         * the caller rather than of the slot.
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
             * recompute something this loop was holding.
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
 * One more page of slots on the end. Nothing is copied
 * and no existing slot moves, so there is no window to get right.
 *
 * A claimer copies its bytes with the station's mutex released, and
 * relocating a slot underneath a worker that owns it is the one thing
 * slot ownership does not cover — which is what paging buys.
 *
 * Growth still takes the station's mutex, as one of the rare
 * structural operations, so that two threads meeting a full buffer
 * add one page between them rather than one each.
 */
static void in_port_grow_locked(cera_in_port_t *sl)
{
    in_port_add_page(sl);
    /* A writer looking for space should start where the space now is.
     * Only a hint: being wrong costs a sweep, not a mistake. */
    sl->write_hint = sl->capacity - sl->page_slots;
    sl->growths++;
}
/* }}} */

/* {{{ in_port_write() */
static void in_port_write(cera_station_t *s, cera_in_port_t *sl, const void *value)
{
    /* Look for somewhere to put it, and grow only if there is
     * genuinely nowhere. Asking the slots directly needs no spare slot
     * and no arithmetic: a full buffer is one where nothing answers.
     */
    void *slot = NULL;
    int c = in_port_scan(sl, &sl->write_hint, CERA_SLOT_EMPTY, CERA_SLOT_RESERVED,
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
        c = in_port_scan(sl, &sl->write_hint, CERA_SLOT_EMPTY, CERA_SLOT_RESERVED,
                         &slot, 0);
        if (c < 0) {
            in_port_grow_locked(sl);
            c = in_port_scan(sl, &sl->write_hint,
                             CERA_SLOT_EMPTY, CERA_SLOT_RESERVED, &slot, 0);
        }
        pthread_mutex_unlock(&s->mutex);
        if (c < 0) {
            cera_bug("delivery: a port with no free slot immediately "
                            "after growing to %d slots\n",
                    atomic_load(&sl->capacity));
        }
    }

    /* **The copy happens with no lock held**. The scan
     * moved this slot to *reserved*, which means it belongs to this
     * thread and no other thread may touch its value — so the bytes
     * need no exclusion from anybody. Publishing it cannot fail for
     * any reason but somebody having touched a slot that was this
     * thread's alone, which is an engine bug worth stopping for.
     *
     * The release on that transition is what makes these bytes
     * visible to whoever later takes the slot from *ready*. */
    memcpy(slot, value, (size_t)sl->elem_size);
    if (!slot_move_at(slot, sl->elem_size, CERA_SLOT_RESERVED, CERA_SLOT_READY)) {
        cera_bug("delivery: publishing a slot this thread had "
                        "reserved, and somebody else had moved it\n");
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
 * Take a ready slot, and take **only** the slot. The
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
static int in_port_take_locked(cera_in_port_t *sl, void **taken)
{
    int c = in_port_scan(sl, &sl->read_hint, CERA_SLOT_READY, CERA_SLOT_CLAIMED,
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
 * Several workers copy out of one station at the same moment while a
 * different worker holds the lock doing its flips. A two-hundred-byte
 * struct per port copied under the mutex would make every worker
 * delivering into that station wait behind it.
 *
 * The worker is finished with the slot the moment the copy lands in
 * its buffer. The box does not run until somebody picks the task up
 * later, reading from the task and holding no slot at all — which is
 * why no user code is ever inside this window either.
 */
static void in_port_release(cera_in_port_t *sl, void *slot, void *into)
{
    memcpy(into, slot, (size_t)sl->elem_size);
    if (!slot_move_at(slot, sl->elem_size, CERA_SLOT_CLAIMED, CERA_SLOT_EMPTY)) {
        cera_bug("delivery: releasing a slot this thread had claimed, "
                        "and somebody else had moved it\n");
    }
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The readiness dispatch. Two tables, indexed by the     */
/* port's kind: "does it hold a value?" and "claim one". Another      */
/* port kind is a new row in each, never a new branch in two          */
/* functions that must be kept in agreement.                          */
/* ------------------------------------------------------------------ */

/* {{{ ring_filled() */
static int ring_filled(const cera_in_port_t *sl)
{
    /* A maintained count rather than two indices differing. Values are
     * claimed wherever they sit rather than from a computed position,
     * and the distance between a head and a tail describes a
     * contiguous run, which this is not. */
    return sl->held > 0;
}
/* }}} */

/* {{{ static_filled() */
static int static_filled(const cera_in_port_t *sl)
{
    /* A static's value is simply always there. */
    (void)sl;
    return 1;
}
/* }}} */

/* {{{ none_filled() */
static int none_filled(const cera_in_port_t *sl)
{
    /* Nobody has said where this port's value comes from, so there is
     * no value and there is no prospect of one. This is
     * the whole of what *none* does at run time: a station holding one
     * answers no to readiness forever, no matter what arrives at its
     * other ports, and therefore never runs. Not an error, and not a
     * value — a station waiting to be finished being built. */
    (void)sl;
    return 0;
}
/* }}} */

/* {{{ int() */
static int (*const in_port_filled[CERA_IN_PORT_KIND_COUNT])(const cera_in_port_t *) = {
    [CERA_IN_PORT_RING]   = ring_filled,
    [CERA_IN_PORT_STATIC] = static_filled,
    [CERA_IN_PORT_NONE]   = none_filled,
};
/* }}} */

/* {{{ ring_claim() */
/*
 * Claiming happens in two moments. Ring values are popped here,
 * under the mutex, which is what makes them spoken-for. A static is
 * resolved later, during task construction, outside the mutex, so
 * the statics table's lock never nests inside a station's. The claim
 * table records that split: a null entry means "resolved at build
 * time".
 *
 * What the split buys is lock ordering, and nothing else.
 */
static void ring_claim(cera_in_port_t *sl, void *into, void **taken)
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
        cera_bug("delivery: a port that answered ready had no ready "
                        "slot when asked for one\n");
    }
    (void)into;
}
/* }}} */

/* {{{ static_claim_locked() */
static void static_claim_locked(cera_in_port_t *sl, void *into, void **taken)
{
    /* Nothing is taken, so nothing is released afterwards: a static is
     * peeked, never consumed. **And the copy stays under the lock**,
     * which is the one place the ownership argument does not reach. A
     * claimed ring slot belongs to one worker, and that ownership is
     * what lets its bytes be copied without exclusion; a static
     * belongs to nobody in particular, so the only thing standing
     * between a claim and a concurrent write to the same constant is
     * this mutex. Moving this copy outside it would reintroduce
     * exactly the torn read that putting the value on the port was
     * meant to make impossible. */
    *taken = NULL;
    /* A copy, under the station's mutex, beside the ring pops.
     * The value lives on the port, so there is no second lock to
     * order.
     *
     * The static half of an input set is therefore as mutually
     * consistent as the buffered half: every value a task carries was
     * taken in one window under one lock, so a box reading two statics
     * cannot get values that were correct at two different moments and
     * never together.
     *
     * Nothing is consumed. A static is always full. */
    memcpy(into, sl->constant, (size_t)sl->elem_size);
}
/* }}} */

/* {{{ none_claim() */
static void none_claim(cera_in_port_t *sl, void *into, void **taken)
{
    (void)taken;
    /* Unreachable, and saying so out loud is the point. The walk above
     * this one asks every port whether it is filled before it claims
     * from any of them, and an unconfigured port answers no — so
     * arriving here means the readiness check and the claim disagreed
     * about the same port, which is an engine bug rather than a
     * situation to handle. */
    (void)sl; (void)into;
    cera_bug("delivery: claimed from a port that has no source — the "
                    "readiness walk and the claim walk disagreed\n");
}
/* }}} */

/* {{{ void() */
/*
 * **No row here is an absence.** Every kind has a function, so the
 * dispatch is a call rather than a call guarded by a question about
 * the table's own shape.
 */
static void (*const in_port_claim_locked[CERA_IN_PORT_KIND_COUNT])
                   (cera_in_port_t *, void *, void **) = {
    [CERA_IN_PORT_RING]   = ring_claim,
    [CERA_IN_PORT_STATIC] = static_claim_locked,
    [CERA_IN_PORT_NONE]   = none_claim,
};
/* }}} */

/* {{{ station_input_bytes() */
/*
 * Total bytes of one complete input set. Element sizes never change
 * after construction, so this reads without the mutex.
 */
static int station_input_bytes(const cera_station_t *s)
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
static int station_ready_and_claim_locked(cera_station_t *s, unsigned char *claimed,
                                          void **taken)
{
    /*
     * **Check all, then flip all**, and the order is the
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
        cera_in_port_t *sl = &s->in_ports[i];
        if (!in_port_filled[sl->kind](sl))
            return 0;
    }

    int offset = 0;
    for (int i = 0; i < s->n_in_ports; i++) {
        cera_in_port_t *sl = &s->in_ports[i];
        /* Every kind, unconditionally.
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
 * The copies, and the release, with **no lock held**.
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
static void station_release_claimed(cera_station_t *s, unsigned char *claimed,
                                    void **taken)
{
    int offset = 0;
    for (int i = 0; i < s->n_in_ports; i++) {
        cera_in_port_t *sl = &s->in_ports[i];
        if (taken[i])
            in_port_release(sl, taken[i], claimed + offset);
        offset += sl->elem_size;
    }
}
/* }}} */

/* ------------------------------------------------------------------ */
/* The task struct in motion.                             */
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
static cera_task_t *task_build(cera_map_t *m, int station_index,
                   const unsigned char *claimed, int port)
{
    cera_station_t *s = cera_map_station(m, station_index);

    int in_bytes = station_input_bytes(s);
    size_t total = sizeof(cera_task_t)
                 + (size_t)s->n_in_ports * sizeof(void *)
                 + (size_t)in_bytes
                 + (size_t)s->out_size;

    cera_task_t *t = malloc(total);
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
     * ran, whatever kind of port it came from, so this is one copy per
     * port out of the caller's buffer. Nothing reaches back into the
     * map for a value it failed to bring along.
     *
     * The claimed buffer may be null only for a station with no ports
     * at all, which the loop below then does not enter.
     */
    int offset = 0;
    for (int i = 0; i < s->n_in_ports; i++) {
        cera_in_port_t *sl = &s->in_ports[i];
        t->in[i] = data + offset;
        if (!claimed)
            die("building a task with no claimed values for a station that "
                "has ports", station_index);
        memcpy(t->in[i], claimed + offset, (size_t)sl->elem_size);
        offset += sl->elem_size;
    }
    /* The program this task belongs to, carried so that finishing it
     * does not depend on which pool ran it. The map was
     * already being passed here and discarded, which is how small
     * this turned out to be. */
    t->owner = m;

    t->out = s->out_size > 0 ? data + in_bytes : NULL;
    return t;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Delivery itself.                                 */
/* ------------------------------------------------------------------ */

/* {{{ cera_map_station_start_after() */
/*
 * Readiness, claim, build, push — a delivery with the delivering taken
 * out. Three callers wanted exactly this and were each doing their own
 * version of it.
 *
 * The seed sweep, and setting or writing a static, all go through it:
 * a station is asked whether it is ready and claims through the one
 * path, so every value a task carries came out of a claim buffer. That
 * is what makes a chain of stations wired through static ports into a
 * recalculation graph.
 *
 * A write cannot make something run that could not run anyway, because
 * the check it triggers is this one: an empty ring port still answers
 * no, and the engine will not invent a value for it.
 */
int cera_map_station_start_after(cera_map_t *m, int station,
                            void (*while_locked)(void *), void *ctx)
{
    if (station < 0 || station >= m->n_stations)
        die("starting a station outside the table", station);
    cera_station_t *s = cera_map_station(m, station);
    if (!s->call)
        die("starting a station with no box placed", station);
    /* Removed and not yet reclaimed: nothing new starts from it.
     * */
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
     * There is exactly one such caller and it is writing a static.
     * A write to a static and the readiness
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
    if (due && s->kind == CERA_STATION_ITERATOR && s->n_out_ports > 0) {
        port = s->cursor;
        s->cursor = (s->cursor + 1) % s->n_out_ports;
    }
    pthread_mutex_unlock(&s->mutex);

    if (due) {
        /* Outside the lock: the copies, then the task. */
        station_release_claimed(s, claimed, taken);
        cera_pool_push(m->pool, task_build(m, station, in_bytes > 0 ? claimed : NULL,
                                      port));
    }
    return due;
}
/* }}} */

/* {{{ cera_map_station_try_start() */
int cera_map_station_try_start(cera_map_t *m, int station)
{
    return cera_map_station_start_after(m, station, NULL, NULL);
}
/* }}} */

/* {{{ cera_map_station_keep_starting() */
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
int cera_map_station_keep_starting(cera_map_t *m, int station)
{
    cera_station_t *s = cera_map_station(m, station);

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
                                 memory_order_relaxed) == CERA_IN_PORT_RING)
            goto drain;
    return 0;

drain:;
    int started = 0;
    while (cera_map_station_try_start(m, station))
        started++;
    return started;
}
/* }}} */

/* {{{ cera_map_station_start_while_ready() */
int cera_map_station_start_while_ready(cera_map_t *m, int station)
{
    int started = cera_map_station_try_start(m, station) ? 1 : 0;
    return started + cera_map_station_keep_starting(m, station);
}
/* }}} */

/* {{{ cera_map_deliver_value() */
int cera_map_deliver_value(cera_map_t *m, int station, int port, const void *value)
{
    if (station < 0 || station >= m->n_stations)
        die("delivering to a station outside the table", station);
    cera_station_t *s = cera_map_station(m, station);

    /*
     * The station may have been removed since this value set out.
     * A worker reads a port's destinations once and then
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
     * **A value arriving at a static port overwrites it**,
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
    if (s->in_ports[port].kind == CERA_IN_PORT_NONE)
        die("delivering into a port that has no source yet", station);
    if (s->in_ports[port].kind == CERA_IN_PORT_STATIC) {
        cera_map_in_port_static_write(m, station, port, value,
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

    /* **The write takes no lock**. Reserving a slot is a
     * single compare-and-swap and the copy that follows goes into
     * bytes this thread owns, so deliveries into one station never
     * serialize against each other — only task construction does. */
    /* Whether the write grew the buffer, which is only knowable by
     * looking before and after — so the looking is inside the flag with
     * the emit it feeds. This is the delivery path, where a comparison
     * per value is exactly the cost that must not be paid unasked. */
#ifdef CERA_WATCH
    int grew_before = s->in_ports[port].growths;
#endif
    in_port_write(s, &s->in_ports[port], value);
#ifdef CERA_WATCH
    if (s->in_ports[port].growths != grew_before)
        CERA_EMIT(m, CERA_WATCH_GREW, station, port,
                  atomic_load(&s->in_ports[port].capacity), 0, 0);
#endif

    STATS_MARK(wait_start);
    pthread_mutex_lock(&s->mutex);
    STATS_CHARGE(s->mutex_wait_ns, wait_start);
    int due = station_ready_and_claim_locked(s, claimed, taken);
    if (due && s->kind == CERA_STATION_ITERATOR && s->n_out_ports > 0) {
        /* The one memory a station keeps, touched at the one moment
         * only one thread can be looking: this task
         * takes the cursor's exit, the cursor moves on, and the
         * choice rides out inside the task. The box never sees it. */
        out_port = s->cursor;
        s->cursor = (s->cursor + 1) % s->n_out_ports;
    }
    pthread_mutex_unlock(&s->mutex);

    if (due) {
        /* Outside the lock: the copies, then the task. */
        station_release_claimed(s, claimed, taken);
        CERA_EMIT(m, CERA_WATCH_DUE, station, 0, 0, 0, 0);
        cera_pool_push(m->pool, task_build(m, station, claimed, out_port));
    }
    return due;
}
/* }}} */

/* ------------------------------------------------------------------ */
/* Routing.                                                           */
/* The kind is consulted at exactly this one moment on the way out    */
/* and nowhere else in the engine. A table, not a chain: a fourth     */
/* kind is a row.                                                     */
/* ------------------------------------------------------------------ */

/* {{{ route_plain() */
static int route_plain(cera_station_t *s, cera_task_t *t)
{
    (void)s; (void)t;
    /* A plain box has one exit. */
    return 0;
}
/* }}} */

/* {{{ route_comparator() */
static int route_comparator(cera_station_t *s, cera_task_t *t)
{
    /* The threshold rode along as the task's last input — claimed
     * like any other port, never handed to the box. The
     * comparison happens here, after the box returned, through the
     * type's own three-way compare: the sign maps
     * straight onto the ports — less is 0, equal is 1, greater 2. */
    int sign = s->compare(t->out, t->in[t->n_in - 1]);
    return sign + 1;
}
/* }}} */

/* {{{ route_iterator() */
static int route_iterator(cera_station_t *s, cera_task_t *t)
{
    (void)s;
    /* Chosen at enqueue time, under the station's mutex, and
     * recorded in the task — so two tasks assembled a
     * moment apart carry different exits no matter which finishes
     * first. Reading it here is the whole row. */
    return t->port;
}
/* }}} */

/* {{{ int() */
static int (*const route_choose[CERA_STATION_KIND_COUNT])(cera_station_t *, cera_task_t *) = {
    [CERA_STATION_PLAIN]      = route_plain,
    [CERA_STATION_COMPARATOR] = route_comparator,
    [CERA_STATION_ITERATOR]   = route_iterator,
};
/* }}} */

/* {{{ station_collect_result() */
/*
 * **One value into the caller's array**, and the bound enforced where
 * it has to be (issue 209a).
 *
 * **The reservation is the bound, not the winding down.** A worker
 * takes the next index with one atomic add; a worker handed an index
 * at or past the room writes nothing. Winding down when an array fills
 * happens alongside and is an optimisation — it stops the machine
 * spending effort on results nobody will keep — but it can never be
 * what keeps the array in bounds, because it is asynchronous and
 * workers are still inside boxes when the last slot goes.
 *
 * **No lock, and no state on a slot.** A ring slot needs empty,
 * reserved, ready and claimed because it is reused and a reader has to
 * know what it is looking at. One of these is written exactly once and
 * read by nobody until the caller looks, so the index is the whole
 * mechanism and two workers writing adjacent slots touch different
 * bytes.
 *
 * The counter keeps climbing past the room. That is deliberate: a
 * caller comparing it against the room learns not only that the array
 * filled but by how much it was overrun, which is the difference
 * between a program that finished and one that is still going.
 */
static void station_collect_result(cera_out_port_t *p, const void *value)
{
    int slot = atomic_fetch_add_explicit(&p->taken, 1, memory_order_acq_rel);
    if (slot >= p->room)
        return;
    memcpy((unsigned char *)p->into + (size_t)slot * (size_t)p->elem_size,
           value, (size_t)p->elem_size);
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
 * **The walk takes no lock and copies nothing**. A port's
 * destinations are one immutable array; a rewire builds a whole new
 * one and swaps the pointer, so reading that pointer once yields
 * something nobody will ever modify. The old set is filed rather than
 * freed, so a walker already inside one is not walking freed memory.
 */
static void map_deliver(void *ctx, cera_task_t *t)
{
    /*
     * **The task says which program it belongs to**, not the pool.
     * The hook's own context is ignored, and that is the
     * whole of what lets one pool serve several programs: a station
     * index means nothing without the table it indexes, so while the
     * map came from the pool, a pool could serve exactly one map.
     */
    (void)ctx;
    cera_map_t *m = t->owner;
    cera_station_t *s = cera_map_station(m, t->station);

    s->runs++;
    CERA_EMIT(m, CERA_WATCH_RAN, t->station, 0, 0, 0, (uint64_t)t->box_ns);

    /* The box's own time, charged onto the task by the shim and moved
     * onto the station here — the one place that holds both.
     * Zero when timing is compiled out, so this costs an add of
     * nothing rather than a branch. */
    s->box_ns += t->box_ns;

    if (s->out_size == 0)
        return;

    int out_port_index = route_choose[s->kind](s, t);

    cera_out_port_t *port = station_out_port(s, out_port_index);

    /*
     * **A value that leaves the map goes wherever the caller said**
     * (issue 209a), and this is the whole of what registering does to
     * the delivery path.
     *
     * It happens before the wires rather than instead of them, because
     * a result may also feed something inside: a station's answer can
     * be both what the program produces and what its next stage
     * consumes, and there is no reason to make an author choose.
     */
    if (port && port->into)
        station_collect_result(port, t->out);

    cera_dest_set_t *set = out_port_dests(port);
    if (!set || set->n == 0) {
        /*
         * Nobody is wired here, so **discard**, deliberately and for
         * every station alike: an unwired comparator branch is the
         * ordinary case, and a program that sends everything below a
         * threshold somewhere means to drop the rest.
         *
         * A marked result is no exception. It used to be — an unwired
         * one *held*, in an array that doubled forever behind the
         * caller's back — and now a value nobody registered for is
         * dropped like any other. Registering is the arrow that was
         * missing.
         */
        return;
    }

    /* A hundred destinations is a hundred lock-write-check cycles by
     * this one worker before it takes more work — acceptable, because
     * each delivery may unblock a station, so this worker is busy
     * manufacturing parallelism for everyone else. */
    for (int i = 0; i < set->n; i++) {
        CERA_EMIT(m, CERA_WATCH_MOVED, t->station, out_port_index,
                  set->items[i].station, set->items[i].port, 0);
        s->produced += cera_map_deliver_value(m, set->items[i].station,
                                         set->items[i].port, t->out);
    }
}
/* }}} */

/* 020's private macros end with 020. */
#undef STATS_MARK
#undef STATS_CHARGE

/* }}} */

/* {{{ 027 — support for generated code */
/* ==================================================================
 *
 * 027 — support for generated code
 *
 * Was src/027-emitted-support.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
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

/* {{{ late_recover_box() */
/*
 * Rows added while the program runs live next door. They
 * are declared here rather than in a header because only these two
 * lookups need them: everything else reaches a box through the row it
 * was already handed.
 */
static const cera_box_place_t *late_recover_box(const char *name);
/* }}} */

/* {{{ late_place_find() */
static const cera_box_place_t *late_place_find(const char *name);
/* }}} */

/* {{{ box_place_matches() */
/*
 * Which generated placement function writes this box's station.
 * Compiled-in rows first, then anything that arrived
 * after the program started.
 *
 * A box the program was built with wins over one added afterwards
 * under the same name, so bringing in new code can never quietly
 * replace something a map already depends on. Replacing a name that
 * was never built in works; shadowing one that was does not.
 *
 * **This is the whole of by-name placement.** Everything a station
 * needs — the shim, the slot sizes, the return size, the comparison —
 * is written directly onto it by the placement function, from a
 * `sizeof` the compiler folded. A placement function is hand placement
 * written by the generator, so naming a box is only a way of finding
 * which one to call.
 */
/*
 * **Three ways to say which box, and they are one rule**
 * : a bare function name; a basename and a function; a path and
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
static int box_place_matches(const cera_box_place_t *row, const char *name)
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
/* }}} */

/* {{{ cera_box_place_find() */
const cera_box_place_t *cera_box_place_find(const char *name)
{
    if (!name || !*name)
        return NULL;
    for (int i = 0; i < n_box_places; i++)
        if (box_place_matches(&box_places[i], name))
            return &box_places[i];
    return late_place_find(name);
}
/* }}} */

/* {{{ cera_struct_text_find() */
/*
 * One type's reader and writer, by name. Asked at
 * placement, so that a port holding a struct constant is *handed* its
 * pair — the same way a station is handed its shim and its comparison
 * — and nothing searches anything afterwards.
 */
const cera_struct_text_t *cera_struct_text_find(const char *type_name)
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

/* {{{ cera_struct_find() */
const cera_struct_info_t *cera_struct_find(const char *type_name)
{
    for (int i = 0; i < n_struct_layouts; i++)
        if (strcmp(struct_layouts[i].name, type_name) == 0)
            return &struct_layouts[i];
    return NULL;
}
/* }}} */

/* {{{ cera_emitted_print() */
/*
 * **What a program can place, and where each one came from.**
 *
 * Which names a program answers to, and which file each one was
 * compiled from. The sizes are folded into placement functions and the
 * type names live in the box source the binary carries, so neither is
 * here to print.
 */
void cera_emitted_print(FILE *out)
{
    fprintf(out, "emitted: %d boxes, %d structs\n",
            n_box_places, n_struct_layouts);
    for (int i = 0; i < n_box_places; i++)
        fprintf(out, "  %-20s %s\n", box_places[i].name,
                box_places[i].address);
    for (int i = 0; i < n_struct_layouts; i++) {
        const cera_struct_info_t *s = &struct_layouts[i];
        fprintf(out, "  struct %s: %d bytes, %d fields\n",
                s->name, s->size, s->n_fields);
        for (int f = 0; f < s->n_fields; f++) {
            const cera_field_info_t *fl = &s->fields[f];
            static const char *const kind_names[] = {
                "int", "uint", "float", "string", "struct",
            };
            fprintf(out, "    +%-3d %-12s %-6s %d bytes\n",
                    fl->offset, fl->name, kind_names[fl->kind], fl->size);
        }
    }
}
/* }}} */

/* {{{ cera_box_source_text() */
/*
 * **The C one box source was compiled from**, by the
 * path the build knew it as.
 *
 * A bare basename matches too, because that is how a person refers to
 * a file they can see — `029-demo-boxes.c` rather than
 * `src/boxes/029-demo-boxes.c`. Where two sources share a basename
 * the full path is the way to say which, exactly as it is for
 * addressing a box.
 */
const char *cera_box_source_text(const char *path)
{
    if (!path || !*path)
        return NULL;

    for (int i = 0; i < cera_n_box_sources; i++)
        if (strcmp(cera_box_sources[i].path, path) == 0)
            return cera_box_sources[i].text;

    /* Then by basename, for somebody who typed what they could see. */
    for (int i = 0; i < cera_n_box_sources; i++) {
        const char *slash = strrchr(cera_box_sources[i].path, '/');
        const char *base = slash ? slash + 1 : cera_box_sources[i].path;
        if (strcmp(base, path) == 0)
            return cera_box_sources[i].text;
    }

    /*
     * **And then what has arrived since**. The build is
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
    return cera_late_source_text(path);
}
/* }}} */

/* {{{ cera_map_build_find() */
/*
 * **The compiled form of one description**, by the path
 * the build knew it as or by the bare name somebody would type — the
 * same two ways a box source is found, for the same reason.
 */
const cera_map_build_t *cera_map_build_find(const char *path)
{
    if (!path || !*path)
        return NULL;

    for (int i = 0; i < cera_n_map_builds; i++)
        if (strcmp(cera_map_builds[i].path, path) == 0)
            return &cera_map_builds[i];

    for (int i = 0; i < cera_n_map_builds; i++) {
        const char *slash = strrchr(cera_map_builds[i].path, '/');
        const char *base = slash ? slash + 1 : cera_map_builds[i].path;
        if (strcmp(base, path) == 0)
            return &cera_map_builds[i];
    }
    return NULL;
}
/* }}} */

/* {{{ cera_map_place_box() */
void cera_map_place_box(cera_map_t *m, int station, const char *box_name, int kind)
{
    /*
     * A place that has been removed but not yet reclaimed is not free.
     * Its record still carries the port count and return
     * size a task being built right now needs, and the sweep that
     * clears them is what makes the place available. Placing here
     * before then would have the sweep clear the *new* station's
     * record.
     */
    if (station >= 0 && station < m->n_stations
        && atomic_load_explicit(&cera_map_station(m, station)->removed,
                                memory_order_acquire)) {
        cera_fail(CERA_EXIT_BAD_CALL, "map: station %d was removed and is not reclaimed yet — "
                "something may still be inside a task built from it\n",
                station);
    }

    const cera_box_place_t *bp = cera_box_place_find(box_name);
    if (!bp) {
        /*
         * Before giving up: a box added while some *earlier* process
         * ran left its source behind under its own name, and this may
         * be that program's dump being reloaded. Recovery
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
        cera_fail(CERA_EXIT_BAD_CALL, "map: no box named '%s' anywhere the build could see — misspelled, or its "
                "source is not under src/boxes/\n", box_name);
    }

    /*
     * **The station is written by the generated placement function**,
     * and by nothing else. Every number in it is a
     * `sizeof` the compiler folded into an immediate, so the sizes are
     * not read from anywhere at run time — they were computed while
     * the box was being compiled and never stored.
     *
     * **The two comparator refusals moved out of here** and into that
     * function, where they were already duplicated. A box that returns
     * nothing cannot be a comparator, because there is nothing to
     * compare; a box whose return type has no comparison cannot be
     * one, because routing on raw bytes would produce an answer and it
     * would be wrong. Both are refused wherever a box is
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
     * comparison function are all written by the placement function.
     *
     * Type names are what let a static's text become bytes of the
     * right shape and what the wire checker reports; the comparison is
     * resolved at placement, so the delivery path compares through a
     * pointer the station holds rather than looking anything up per
     * value.
     */
}
/* }}} */

/* }}} */

/* {{{ 033 — constants, and values from text */
/* ==================================================================
 *
 * 033 — constants, and values from text
 *
 * Was src/033-statics.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
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

/* {{{ where_t */
/*
 * Where an error happened. Published as cera_where_t,
 * because generated readers name the same place, and spelled `where_t`
 * here so the file that has always used the short name still reads
 * the way it did.
 */
typedef cera_where_t where_t;
/* }}} */

/* {{{ die_static() */
static void die_static(const where_t *w, const char *what)
{
    /*
     * **Exit 70 rather than a core dump**: text that will
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
    cera_stop_now(NULL, CERA_EXIT_BAD_CALL, said);
}
/* }}} */

/* {{{ escapes */
/*
 * **One table, two directions**, so the writer and the reader cannot
 * disagree about what a backslash introduces.
 *
 * Every byte the engine can hold has a spelling here, which is what
 * lets the dump round-trip: a value holding a quote, a tab, a newline
 * or a byte above 0x7F is written down and read back unchanged.
 *
 * **Five named escapes and a hexadecimal form.** The five are the ones
 * a person reading a map should see spelled the way they already know
 * them. Everything else
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
/* }}} */

/* {{{ read_quoted() */
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

/* ------------------------------------------------------------------ */
/* What a type name fundamentally is, engine-side. This mirrors the  */
/* generator's own classification — two lists that must agree, which  */
/* the first-pass report flags as duplicated knowledge for the second */
/* pass to unify.                                                     */
/* ------------------------------------------------------------------ */

/* {{{ type type_class_t */
typedef enum {
    TN_INT, TN_UINT, TN_FLOAT, TN_STRING, TN_STRUCT, TN_UNKNOWN
} type_class_t;
/* }}} */

/* {{{ classify_port() */
/*
 * What kind of thing this port holds, and — when it is a struct —
 * where its reader and writer are.
 *
 * **The struct is not searched for.** The port was handed the address
 * of its pair at placement, where the type was known concretely.
 *
 * The primitives are still told apart by their spelling, and that is
 * a different act from comparing two types: a wire is legal on
 * **width** alone, because two boxes may spell one shape differently
 * and mean the same data. Nothing here compares one type
 * against another. It asks how to turn text into bytes, which needs
 * to know whether those bytes are a number, and which kind.
 */
static type_class_t classify_port(const cera_in_port_t *sl,
                                  const cera_struct_text_t **out_struct)
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

/* {{{ write_integer() */
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
/* }}} */

/* {{{ write_unsigned() */
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
/* }}} */

/* {{{ write_float() */
static void write_float(double v, unsigned char *out, int size,
                        const where_t *w)
{
    if (size == 4) { float x = (float)v; memcpy(out, &x, 4); }
    else if (size == 8) { memcpy(out, &v, 8); }
    else die_static(w, "a floating field of a width the reader does not know");
}
/* }}} */

/* {{{ read_integer() */
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
/* }}} */

/* {{{ read_unsigned() */
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
/* }}} */

/* {{{ read_float() */
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
/* Turning a value back into words. The exact mirror of   */
/* the reader above, walking the same field table the other way.      */
/* ------------------------------------------------------------------ */

/* {{{ textbuf_t */
/*
 * A growing piece of text that never overflows and always reports how
 * much it wanted. `used` counts characters the caller asked for, which
 * may exceed the room available — so a caller that cares can tell it
 * was cut short and ask again with a bigger buffer, the same contract
 * snprintf offers.
 */
typedef cera_textbuf_t textbuf_t;
/* }}} */

/* {{{ tb_addf() */
static void tb_addf(textbuf_t *tb, const char *fmt, ...);
/* }}} */

/* {{{ tb_addf() */
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

/* {{{ write_quoted() */
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

/* {{{ cera_text_expect() */
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
const char *cera_text_expect(const char *p, char c, const cera_where_t *w,
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
/* }}} */

/* {{{ cera_text_signed() */
const char *cera_text_signed(const char *p, void *out, int size,
                             const cera_where_t *w, const char *field)
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
/* }}} */

/* {{{ cera_text_unsigned() */
const char *cera_text_unsigned(const char *p, void *out, int size,
                               const cera_where_t *w, const char *field)
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
/* }}} */

/* {{{ cera_text_floating() */
const char *cera_text_floating(const char *p, void *out, int size,
                               const cera_where_t *w, const char *field)
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
/* }}} */

/* {{{ cera_text_chars() */
const char *cera_text_chars(const char *p, char *out, int room,
                            const cera_where_t *w, const char *field)
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
/* }}} */

/* {{{ cera_text_put() */
void cera_text_put(cera_textbuf_t *tb, const char *literal)
{
    tb_addf(tb, "%s", literal);
}
/* }}} */

/* {{{ cera_text_put_signed() */
void cera_text_put_signed(cera_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    tb_addf(tb, "%lld", read_integer((const unsigned char *)bytes, size, &w));
}
/* }}} */

/* {{{ cera_text_put_unsigned() */
void cera_text_put_unsigned(cera_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    tb_addf(tb, "%llu", read_unsigned((const unsigned char *)bytes, size, &w));
}
/* }}} */

/* {{{ cera_text_put_floating() */
void cera_text_put_floating(cera_textbuf_t *tb, const void *bytes, int size)
{
    where_t w = { -1, -1 };
    float_text(tb, read_float((const unsigned char *)bytes, size, &w), size);
}
/* }}} */

/* {{{ cera_text_put_chars() */
void cera_text_put_chars(cera_textbuf_t *tb, const char *chars, int room)
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

/* {{{ value_text() */
/*
 * One value of this port's type, written down. The bytes are given
 * rather than taken from the port, because two different things are
 * written with the same grammar: the constant a static port holds,
 * and each value waiting in a ring buffer when a running program is
 * captured.
 *
 * The port is still needed — it says what shape the bytes are — but
 * not as the place the bytes come from.
 */
static void value_text(const cera_in_port_t *sl, const void *bytes,
                       const char *string, textbuf_t *tb)
{
    where_t w = { -1, -1 };   /* the port is the caller's to name here */
    {
        const cera_struct_text_t *si = NULL;
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
             * Escaped on the way out, so text holding a
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
static int in_port_constant_text(const cera_in_port_t *sl, char *out, int room)
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

/* {{{ in_port_waiting_text() */
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
static int in_port_waiting_text(const cera_in_port_t *sl, char *out, int room)
{
    textbuf_t tb = { out, room, 0 };
    if (room > 0)
        out[0] = 0;

    int written = 0;
    int capacity = atomic_load(&sl->capacity);
    for (int i = 0; i < capacity; i++) {
        void *slot = in_port_slot(sl, i);
        if (!slot || slot_state_at(slot, sl->elem_size) != CERA_SLOT_READY)
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
static void in_port_constant_free(cera_in_port_t *sl)
{
    free(sl->constant);
    free(sl->constant_string);
    sl->constant = NULL;
    sl->constant_string = NULL;
    sl->constant_set = 0;
}
/* }}} */

/* {{{ port_text_to_bytes_ending() */
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
 * offsets, and the same messages naming the field that was wrong.
 *
 * `into` is `elem_size` bytes the caller owns. `owned_string` comes
 * back non-null when the port is a string port, holding characters
 * the caller must keep alive for as long as anything can read the
 * pointer that was written into `into` — because a string value *is*
 * that pointer, and freeing what it points at is freeing something a
 * box may still be looking at.
 *
 * **`must_be_quoted` says which of the two callers this is**, and it
 * only ever matters for a string. Text written down in a map file
 * sits beside other notation — `in 0 - fire` draws a wire and
 * `in 0 = fire` sets a constant — so a string there is quoted, and an
 * unquoted word is refused rather than guessed at. Text handed over
 * by a shell arrived as one whole argument with the quoting already
 * done, so requiring more would be asking somebody to quote twice.
 */
static void port_text_to_bytes_ending(const cera_in_port_t *sl, const char *text,
                                      unsigned char *into,
                                      char **owned_string,
                                      int must_be_quoted,
                                      const where_t *w,
                                      const char **end);
/* }}} */

/* {{{ port_text_to_bytes() */
static void port_text_to_bytes(const cera_in_port_t *sl, const char *text,
                               unsigned char *into, char **owned_string,
                               int must_be_quoted, const where_t *w)
{
    port_text_to_bytes_ending(sl, text, into, owned_string, must_be_quoted,
                              w, NULL);
}
/* }}} */

/* {{{ port_text_to_bytes_ending() */
/*
 * The same reading, saying where it stopped. A list of waiting values
 * is comma separated and a struct value has commas inside it, so the
 * only way to find the separator is to read one value and see where
 * it ended. With `end` null this behaves as it always
 * did: whatever follows the value is trailing text and a fault.
 */
static void port_text_to_bytes_ending(const cera_in_port_t *sl, const char *text,
                                      unsigned char *into,
                                      char **owned_string,
                                      int must_be_quoted,
                                      const where_t *w,
                                      const char **end)
{
    unsigned char *fresh = into;
    char *fresh_string = NULL;

    const cera_struct_text_t *si = NULL;
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
         * **Quoted text goes through the shared escape routines**,
         * and in a map file that is the only form there is. */
        int len;
        if (*text == '"') {
            int room = (int)strlen(text);
            fresh_string = malloc((size_t)room + 1);
            if (!fresh_string)
                die_static(w, "out of memory for string storage");
            const char *stop = read_quoted(text, fresh_string, room, &len,
                                           "a string constant", w);
            if (end) *end = stop;
        } else if (must_be_quoted) {
            /*
             * **A bare word written down is refused, and the message
             * names the collision it is almost certainly.** `= fire`
             * and `- fire` differ by one character and mean unrelated
             * things — a constant and a wire from a station called
             * `fire` — so accepting the bare form leaves two spellings
             * for one value sitting next to a third that means
             * something else entirely. The dump has always written the
             * quoted form, so requiring it is what makes a
             * hand-written file and a dumped one agree.
             */
            die_static(w,
                       "a string constant is quoted: write '= \"fire\"'. "
                       "An unquoted word is refused because '- fire' on "
                       "the same line would mean a wire from a station "
                       "called fire, which is a different thing entirely");
            return;
        } else {
            /* Handed over by a shell, which already decided where this
             * value started and stopped. It runs to the end of what it
             * was given, so it can only be the last of a list — which
             * costs nothing, because an argument list is not a list. */
            len = (int)strlen(text);
            fresh_string = malloc((size_t)len + 1);
            if (!fresh_string)
                die_static(w, "out of memory for string storage");
            memcpy(fresh_string, text, (size_t)len);
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

/* {{{ cera_map_in_port_static_text() */
void cera_map_in_port_static_text(cera_map_t *m, int station, int port, const char *text)
{
    where_t w = { station, port };

    if (station < 0 || station >= m->n_stations)
        die_static(&w, "giving a constant to a station outside the table");
    cera_station_t *s = cera_map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        die_static(&w, "giving a constant to a port the box does not have");
    cera_in_port_t *sl = &s->in_ports[port];
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
    /* Written down, so a string is quoted. */
    port_text_to_bytes(sl, text, fresh, &fresh_string, 1, &w);

    /* One of the four rare structural operations: the
     * install and the tag together, under the station's mutex, so no
     * readiness walk and no claim sees a port mid-change. */
    pthread_mutex_lock(&s->mutex);
    char *old_string = sl->constant_string;
    memcpy(sl->constant, fresh, (size_t)sl->elem_size);
    sl->constant_string = fresh_string;
    sl->constant_set = 1;
    sl->kind = CERA_IN_PORT_STATIC;
    pthread_mutex_unlock(&s->mutex);

    free(fresh);
    free(old_string);

    /* A port that was the last one missing is now filled. A chain of
     * stations wired through static ports is therefore a recalculation
     * graph, and construction's own writes are what start a program.
     *
     * Only once the pool exists, because before that there is nowhere
     * to push and the loader is still assembling — the seed sweep is
     * what starts a freshly loaded map, deliberately and once. */
    if (m->pool)
        cera_map_station_start_while_ready(m, station);
}
/* }}} */

/* {{{ cera_map_deliver_argument_text() */
/*
 * **An argument written as text**, turned into the bytes the port
 * wants and delivered through the ordinary door.
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
const char *cera_map_deliver_argument_text(cera_map_t *m, int station, int port,
                                      const char *text)
{
    static _Thread_local char said[256];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    cera_station_t *s = cera_map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said, "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    cera_in_port_t *sl = &s->in_ports[port];
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
    /* Handed over, one whole argument at a time, quoting already done
     * by whoever split the command line. */
    port_text_to_bytes(sl, text, bytes, &owned, 0, &w);

    const char *no = cera_map_deliver_argument(m, station, port, bytes,
                                          sl->elem_size);
    free(bytes);
    /* `owned` is not freed; see above. */
    return no;
}
/* }}} */

/* {{{ cera_map_deliver_command_line() */
/*
 * **The command line, delivered into a program's entrances**.
 *
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
const char *cera_map_deliver_command_line(cera_map_t *m, int argc, char **argv)
{
    static _Thread_local char said[256];

    /*
     * **A program's arguments are its marked ports that nothing
     * feeds**, in the order their numbers say (issues 213a, 601b).
     *
     * Derived rather than stored, which is what makes the old
     * closing-a-door problem disappear instead of being solved. When
     * an enclosing map wires into one of these ports, that port stops
     * being an argv slot on its own — there is no mark to clear, and a
     * port fed both by a wire and by an outside caller stays legal,
     * because being an argument and being fed are different facts.
     *
     * Counted by walking the numbers rather than the table, because a
     * number is the author's name for an argument and the table's
     * order has nothing to do with it. Bring-up has already refused
     * gaps and repeats, so counting up until nothing answers is safe.
     */
    int station[64], port[64];
    int wanted = argument_slots(m, station, port,
                                (int)(sizeof station / sizeof *station));

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
    if (m->pool && cera_pool_finished(m->pool)) {
        snprintf(said, sizeof said,
                 "this program had already finished before its arguments "
                 "arrived — something outside has to hold a standing "
                 "promise from before the workers are released until the "
                 "last argument is in");
        return said;
    }

    const char *no = NULL;
    for (int k = 0; k < wanted && !no; k++)
        no = cera_map_deliver_argument_text(m, station[k], port[k],
                                            argv[1 + k]);

    return no;
}
/* }}} */

/* {{{ cera_map_in_port_queue_text() */
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
const char *cera_map_in_port_queue_text(cera_map_t *m, int station, int port,
                                   const char *text)
{
    static _Thread_local char said[256];

    if (station < 0 || station >= m->n_stations) {
        snprintf(said, sizeof said, "station %d is outside the table",
                 station);
        return said;
    }
    cera_station_t *s = cera_map_station(m, station);
    if (port < 0 || port >= s->n_in_ports) {
        snprintf(said, sizeof said, "station %d has no port %d — it has %d",
                 station, port, s->n_in_ports);
        return said;
    }
    cera_in_port_t *sl = &s->in_ports[port];
    if (!sl->type_name) {
        snprintf(said, sizeof said,
                 "station %d port %d has no declared type, so waiting values "
                 "have no shape to become", station, port);
        return said;
    }
    if (atomic_load(&sl->kind) != CERA_IN_PORT_RING) {
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
        /* Written down, in a captured file, so a string is quoted —
         * which is also what lets a list of them be separated at all. */
        port_text_to_bytes_ending(sl, p, bytes, &owned, 1, &w, &end);

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
        cera_map_deliver_value(m, station, port, bytes);
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

/* {{{ type static_write_t */
/*
 * The copy itself, as something the delivery path can be asked to do
 * while it holds the station's mutex. It exists as a
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
    cera_in_port_t  *port;
    const void *bytes;
    int         size;
} static_write_t;
/* }}} */

/* {{{ static_write_under_lock() */
static void static_write_under_lock(void *ctx)
{
    static_write_t *j = ctx;
    memcpy(j->port->constant, j->bytes, (size_t)j->size);
}
/* }}} */

/* {{{ cera_map_in_port_static_write() */
void cera_map_in_port_static_write(cera_map_t *m, int station, int port,
                           const void *bytes, int size)
{
    where_t w = { station, port };

    if (station < 0 || station >= m->n_stations)
        die_static(&w, "writing to a station outside the table");
    cera_station_t *s = cera_map_station(m, station);
    if (port < 0 || port >= s->n_in_ports)
        die_static(&w, "writing to a port the box does not have");
    cera_in_port_t *sl = &s->in_ports[port];
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
     * **And it happens inside the same hold as the readiness check**.
     * Writing does not consume anything, so a station
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
        cera_map_station_start_after(m, station, static_write_under_lock, &job);
        /*
         * **And then keep asking**. The call above did the
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
        cera_map_station_keep_starting(m, station);
    } else {
        pthread_mutex_lock(&s->mutex);
        static_write_under_lock(&job);
        pthread_mutex_unlock(&s->mutex);
    }
}
/* }}} */

/* }}} */

/* {{{ 042 — reading a description */
/* ==================================================================
 *
 * 042 — reading a description
 *
 * Was src/042-loader.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * The moment the two halves of a program meet.
 *
 * The binary holds a list of boxes and no map; the file holds a map and
 * no code.
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
 * is the same door a box source goes through. */

/* {{{ late_recover_box() */
/* A box added while some earlier process ran; see 073-latebox.h. It
 * is declared here rather than included, because the loader needs one
 * function from that file and nothing else it offers. */
static const cera_box_place_t *late_recover_box(const char *name);
/* }}} */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ die_load() */
static void die_load(const char *path, int line, const char *station,
                     const char *what)
{
    /*
     * **Exit 65, meaning the input was malformed**, rather
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
    cera_stop_now(NULL, CERA_EXIT_BAD_FILE, said);
}
/* }}} */

/* {{{ read_whole_file() */
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

/* {{{ build_from_file() */
/*
 * **A description on disk becomes the calls it describes, and then
 * those calls are made**.
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
static void build_from_file(cera_map_t *m, const char *path, cera_map_instance_t *out)
{
    char *text = read_whole_file(path);
    const cera_map_build_t *built = cera_late_compile_map(text);
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

/* {{{ marked_incomplete() */
/*
 * **An artifact that says it lost work**. A capture taken
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

/* {{{ load_file() */
static cera_map_t *load_file(const char *path, int n_workers, int salvaging);
/* }}} */

/* {{{ cera_map_load_file() */
/*
 * **Reading a description back is refused when it says it lost work**,
 * unless the caller asks for salvage. A program picked up
 * from an incomplete capture is quietly missing results somebody
 * computed, and quietly is the part this engine refuses everywhere: a
 * fallback is a warning and a warning is an error.
 *
 * Salvaging is a different act, and having a different name for it is
 * the point — whoever calls it has said out loud that they know what
 * is missing.
 */
cera_map_t *cera_map_load_file(const char *path, int n_workers)
{
    return load_file(path, n_workers, 0);
}
/* }}} */

/* {{{ cera_map_load_salvage() */
cera_map_t *cera_map_load_salvage(const char *path, int n_workers)
{
    return load_file(path, n_workers, 1);
}
/* }}} */

/* {{{ load_file() */
static cera_map_t *load_file(const char *path, int n_workers, int salvaging)
{
    /*
     * An empty table, grown one station at a time as the description
     * is built, so reading a map is the same act as adding a station
     * to a running program.
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

    cera_map_t *m = cera_map_create_empty();

    build_from_file(m, path, NULL);

    /* The pool exists before the seed so the seed has somewhere to
     * push, but its workers stay parked until the caller releases. */
    cera_map_start(m, n_workers);

    /*
     * **Reading a file does not validate or seed. It asks for the
     * program to be brought up, the same as anybody else would**, so
     * there is no state called *still loading*.
     */
    const char *no = cera_map_bring_up(m);
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
     * waiting is exactly what it is supposed to do. This
     * refusal means "nothing can start and nothing can arrive, so
     * this program will do nothing at all" — and a declared entrance
     * is a station something outside delivers to, which makes the
     * second half of that false.
     */
    int has_entrance = cera_map_argument_at(m, 0, NULL, NULL);

    if (cera_map_seed_count(m) == 0 && !has_entrance)
        die_load(path, 0, NULL,
                 "nothing to seed — every station waits for a buffered "
                 "value, so the map cannot ever start");

    return m;
}
/* }}} */

/* {{{ cera_map_instantiate_file() */
/*
 * **Bring a description inside a program that already exists**
 *  — the operation this whole file turns out to have been, with
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
cera_map_instance_t cera_map_instantiate_file(cera_map_t *m, const char *path)
{
    /*
     * **Where the stations landed comes back from the built function
     * itself**, because nothing else can know. Adding a station hands
     * back a freed place before it grows the table, so a program that
     * has had removals gets whatever holes exist in whatever order,
     * and the parent wants the doors in the order the description
     * declared them rather than in table order.
     */
    cera_map_instance_t in;
    build_from_file(m, path, &in);
    return in;
}
/* }}} */

/* {{{ cera_map_instance_entrance() */
/*
 * **The station holding this instance's nth argument**, or -1.
 *
 * This is the whole of what a parent is entitled to know about
 * something it brought inside itself. It could reach any of the
 * instance's stations through the handle — the translation table is
 * right there — and doing so would be reaching inside a thing whose
 * author may rename or restructure anything that is not a door.
 *
 * **The nth is the number the description wrote down**, not a count of
 * doors in table order (issue 213a). Under the old scheme the parent
 * got them in the order the stations happened to land, so moving two
 * lines in the sub-map silently swapped two of the parent's arguments.
 * Now the sub-map's author says which is which, and reordering the
 * file changes nothing.
 */
int cera_map_instance_entrance(cera_map_t *m, const cera_map_instance_t *in, int nth)
{
    for (int i = 0; i < in->count; i++) {
        cera_station_t *s = cera_map_station(m, in->station[i]);
        if (!s->call)
            continue;
        for (int j = 0; j < s->n_in_ports; j++)
            if (s->in_ports[j].argument == nth)
                return in->station[i];
    }
    return -1;
}
/* }}} */

/* {{{ cera_map_instance_result() */
/* The same question about the other direction. */
int cera_map_instance_result(cera_map_t *m, const cera_map_instance_t *in, int nth)
{
    for (int i = 0; i < in->count; i++) {
        cera_station_t *s = cera_map_station(m, in->station[i]);
        if (!s->call)
            continue;
        for (cera_out_port_t *p = s->out_ports; p; p = p->next)
            if (p->result == nth)
                return in->station[i];
    }
    return -1;
}
/* }}} */

/* {{{ cera_map_instance_free() */
/*
 * The handle goes; the stations stay. Nothing in the running program
 * refers to this — it was the reader's note to itself about where
 * things landed, and a parent keeps it only for as long as it is
 * still deciding what to wire.
 */
void cera_map_instance_free(cera_map_instance_t *in)
{
    free(in->station);
    in->station = NULL;
    in->count = 0;
}
/* }}} */

/* {{{ cera_map_add_part() */
/*
 * **Adding a box and adding a map are one operation**.
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
const char *cera_map_add_part(cera_map_t *m, const char *what, cera_map_part_t *out)
{
    static _Thread_local char said[512];

    if (!what || !*what)
        return "adding a part with no name";

    const cera_box_place_t *box = cera_box_place_find(what);
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
        int at = cera_map_add_station(m);
        if (at < 0)
            return "the station table would not grow";
        cera_map_place_box(m, at, what, CERA_STATION_PLAIN);
        out->entrance = at;
        out->result = at;
        return NULL;
    }

    if (described) {
        cera_map_instance_t in = cera_map_instantiate_file(m, what);
        out->entrance = cera_map_instance_entrance(m, &in, 0);
        out->result = cera_map_instance_result(m, &in, 0);
        cera_map_instance_free(&in);
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

/* {{{ cera_map_connect_parts() */
/*
 * **A wire from one part's way out to another part's way in**, which
 * is the only wire a composing caller ever needs to draw.
 *
 * For two single boxes this is the ordinary wire, because a box's
 * doors are itself. For two maps there is no seam to cross: after
 * instantiation there are stations with indices, like any others.
 *
 * The port numbers are the ones a wire has always had: which output
 * port of the producing station, and which input port of the
 * receiving one. A comparator's three outcomes are reachable this way
 * exactly as before.
 */
const char *cera_map_connect_parts(cera_map_t *m, cera_map_part_t from, int from_port,
                              cera_map_part_t to, int to_port)
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
    return cera_map_wire(m, from.result, from_port, to.entrance, to_port);
}
/* }}} */

/* {{{ cera_map_seed_count() */
int cera_map_seed_count(cera_map_t *m)
{
    return m->seeded;
}
/* }}} */

/* }}} */

/* {{{ 050 — reports and the observer */
/* ==================================================================
 *
 * 050 — reports and the observer
 *
 * Was src/050-observe.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * 050-observe.c — the engine, saying out loud what it already knew.
 *
 * What this is: the reporting half of phase 7.
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
 * backlog said in the new units**. Four doublings meant a
 * port sixteen times its starting depth; sixteen equal pages mean a
 * port seventeen times it. Leaving the number at four would have
 * turned a warning about a runaway producer into one that fires the
 * moment a consumer is briefly slow, and a warning that cries wolf is
 * one people learn to scroll past — which costs more than the warning
 * was ever worth.
 */
#define GROWTH_SHOUT_THRESHOLD 16

/* {{{ station_label() */
static const char *station_label(cera_map_t *m, int i, char *fallback, size_t n)
{
    if (m->station_names && m->station_names[i])
        return m->station_names[i];
    snprintf(fallback, n, "station %d", i);
    return fallback;
}
/* }}} */

/* {{{ cera_map_report_buffers() */
void cera_map_report_buffers(cera_map_t *m, FILE *out)
{
    fprintf(out, "buffers:\n");
    int spoke = 0;
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        for (int j = 0; j < s->n_in_ports; j++) {
            cera_in_port_t *sl = &s->in_ports[j];
            if (sl->kind != CERA_IN_PORT_RING)
                continue;
            if (sl->growths == 0 && sl->high_water <= 1)
                continue;
            char fallback[32];
            /* "Pages" rather than "times": growth appends rather than
             * doubling, so the count is how many pages were added
             * beyond the first and the depth is a sum across all of
             * them. Saying *pages* stops a reader converting the count
             * into a doubling and getting a wildly wrong idea of the
             * backlog. */
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
        cera_pool_queue_stats(m->pool, &capacity, &high_water, &growths);
        fprintf(out,
                "  the task ring: grew %d time%s to %d entries, high water %d\n"
                "  (port piles mean uneven inputs; ring piles mean consumers\n"
                "   slower than producers — two different diagnoses)\n",
                growths, growths == 1 ? "" : "s", capacity, high_water);
    }
}
/* }}} */

/* {{{ station_time() */
static cera_map_t *sorting_map;   /* qsort has no context argument */

/* Time attributable to a station's own work. Nothing pulls, so the
 * box's own time is the whole of it. */
static long station_time(const cera_station_t *s)
{
    return s->box_ns;
}
/* }}} */

/* {{{ by_time() */
static int by_time(const void *a, const void *b)
{
    const cera_station_t *sa = cera_map_station(sorting_map, *(const int *)a);
    const cera_station_t *sb = cera_map_station(sorting_map, *(const int *)b);
    return (station_time(sb) > station_time(sa))
         - (station_time(sb) < station_time(sa));
}
/* }}} */

/* {{{ by_contention() */
static int by_contention(const void *a, const void *b)
{
    const cera_station_t *sa = cera_map_station(sorting_map, *(const int *)a);
    const cera_station_t *sb = cera_map_station(sorting_map, *(const int *)b);
    return (sb->mutex_wait_ns > sa->mutex_wait_ns)
         - (sb->mutex_wait_ns < sa->mutex_wait_ns);
}
/* }}} */

/* {{{ by_count() */
static int by_count(const void *a, const void *b)
{
    const cera_station_t *sa = cera_map_station(sorting_map, *(const int *)a);
    const cera_station_t *sb = cera_map_station(sorting_map, *(const int *)b);
    return (sb->runs > sa->runs) - (sb->runs < sa->runs);
}
/* }}} */

/* {{{ int() */
static int (*const orderings[CERA_REPORT_ORDER_COUNT])(const void *, const void *) = {
    [CERA_REPORT_BY_TIME]       = by_time,
    [CERA_REPORT_BY_CONTENTION] = by_contention,
    [CERA_REPORT_BY_COUNT]      = by_count,
};
/* }}} */

/* {{{ cera_map_report_stations() */
void cera_map_report_stations(cera_map_t *m, FILE *out, int order)
{
    if (order < 0 || order >= CERA_REPORT_ORDER_COUNT) {
        cera_fail(CERA_EXIT_BAD_CALL, "observe: no such report ordering\n");
    }

    static const char *const order_names[CERA_REPORT_ORDER_COUNT] = {
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
        cera_station_t *s = cera_map_station(m, i);
        char fallback[32];
        fprintf(out, "  %-12s runs %-7ld produced %-7ld",
                station_label(m, i, fallback, sizeof fallback),
                (long)s->runs, (long)s->produced);
#ifdef CERA_STATS
        fprintf(out, " box %8.2fms  waited %8.2fms",
                s->box_ns / 1e6, s->mutex_wait_ns / 1e6);
#endif
        fprintf(out, "\n");
    }
#ifndef CERA_STATS
    fprintf(out, "  (times compiled out; build with -DCERA_STATS to see them)\n");
#endif
}
/* }}} */

/* {{{ cera_stats_box_time() */
/*
 * Called by every generated shim when CERA_STATS is compiled in.
 *
 * The time rides out on the task. The shim charges it to a field the
 * pool carries and never reads, and the delivery walk — which has both
 * the map and the finished task in hand — moves it onto the station
 * afterwards. The pool stays ignorant of stations, and nothing here is
 * process-wide.
 */
void cera_stats_box_time(cera_task_t *t, long ns)
{
    if (t)
        t->box_ns += ns;
}
/* }}} */

/* {{{ observer_main() */
static void *observer_main(void *arg)
{
    cera_map_t *m = arg;
    while (__atomic_load_n(&m->observer_running, __ATOMIC_ACQUIRE)) {
        FILE *out = fopen(m->observer_path, "a");
        if (out) {
            fprintf(out, "--- observation ---\n");
            cera_map_report_buffers(m, out);
            cera_map_report_stations(m, out, CERA_REPORT_BY_COUNT);
            fclose(out);
        }
        usleep((useconds_t)m->observer_interval_ms * 1000);
    }
    return NULL;
}
/* }}} */

/* {{{ cera_map_observe_start() */
void cera_map_observe_start(cera_map_t *m, const char *path, int interval_ms)
{
    if (interval_ms <= 0) {
        /* Refuse rather than default: an engine writing diagnostics
         * nobody reads is a background thread doing nothing useful.
         * Asking for zero means you did not want it. */
        cera_fail(CERA_EXIT_BAD_CALL, "observe: a non-positive interval — if you do not "
                        "want observation, do not start it\n");
    }
    if (m->observer_running) {
        cera_fail(CERA_EXIT_BAD_CALL, "observe: already observing\n");
    }
    m->observer_path = strdup(path);
    m->observer_interval_ms = interval_ms;
    m->observer_running = 1;
    pthread_create(&m->observer, NULL, observer_main, m);
}
/* }}} */

/* {{{ cera_map_observe_stop() */
void cera_map_observe_stop(cera_map_t *m)
{
    if (!m->observer_running)
        return;
    __atomic_store_n(&m->observer_running, 0, __ATOMIC_RELEASE);
    pthread_join(m->observer, NULL);
    free(m->observer_path);
    m->observer_path = NULL;
}
/* }}} */

/* {{{ cera_map_report_shutdown() */
/*
 * The loud parting word: any port that grew past the
 * threshold gets named at teardown, because a map that works while
 * one buffer quietly absorbs a mismatch forever is a map with a
 * design problem nothing else will surface.
 */
void cera_map_report_shutdown(cera_map_t *m)
{
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        for (int j = 0; j < s->n_in_ports; j++) {
            cera_in_port_t *sl = &s->in_ports[j];
            if (sl->kind == CERA_IN_PORT_RING && sl->growths >= GROWTH_SHOUT_THRESHOLD) {
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

/* }}} */

/* {{{ 051 — a live map written back out */
/* ==================================================================
 *
 * 051 — a live map written back out
 *
 * Was src/051-dump.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * The loaded map, written back out as a map.
 *
 * The station table rendered in the map file format, so that loading a
 * map and dumping it produces a file
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
    static const char letters[CERA_STATION_KIND_COUNT] = { 'p', 'c', 'i' };
    return kind < CERA_STATION_KIND_COUNT ? letters[kind] : '?';
}
/* }}} */

/* {{{ cera_map_dump() */
void cera_map_dump(cera_map_t *m, FILE *out)
{
    /*
     * **Every station needs a name**, because a station line begins
     * with one and a file that begins a line with nothing does not
     * read back.
     *
     * Asked per station: a program built by calling the construction
     * surface can be named a station at a time, and can have a station
     * added after the rest were named.
     */
    for (int i = 0; i < m->n_stations; i++) {
        if (!cera_map_station(m, i)->call)
            continue;   /* an empty place is not a station */
        if (i < m->n_named && m->station_names && m->station_names[i])
            continue;
        cera_fail(CERA_EXIT_BAD_CALL, "dump: station %d has no name — a station line begins with "
                "one, so this program cannot be written as a file that reads "
                "back\n", i);
    }

    /*
     * **The names written out are made unique, and the ones on the
     * program are left alone**.
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
        cera_fail(CERA_EXIT_NO_RESOURCE, "dump: out of memory naming stations\n");
    }
    for (int i = 0; i < m->n_stations; i++) {
        if (!cera_map_station(m, i)->call)
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
            cera_fail(CERA_EXIT_NO_RESOURCE, "dump: out of memory naming stations\n");
        }
    }

    /*
     * **What did not finish, said rather than inferred**.
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
        int workers = cera_pool_worker_count(m->pool);
        int busy = 0;
        for (int i = 0; i < workers; i++)
            if (cera_pool_worker_station(m->pool, i) >= 0)
                busy++;
        if (busy > 0) {
            fprintf(out, "# INCOMPLETE CAPTURE\n");
            fprintf(out, "# %d task%s still running and did not finish:\n",
                    busy, busy == 1 ? " was" : "s were");
            for (int i = 0; i < workers; i++) {
                int at = cera_pool_worker_station(m->pool, i);
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
     * **No statics section is written.** The file format's statics
     * section is notation — a way to write a value down once while
     * describing a map and point ports at it by number — and the
     * engine keeps no table behind it.
     *
     * Each value lives on the port that reads it, so every constant is
     * written out beside its port, from its bytes, by the formatter
     * that mirrors the reader. What comes out is what is actually
     * there, including anything a runtime write changed.
     *
     * Two ports that shared an entry in the original file dump as two
     * ports each holding their own copy, because that is what they
     * are. A file that goes in with sharing comes out without it, and
     * reloading gives the same program: the sharing is notation, never
     * behaviour.
     */
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
        /* Announced like every other line kind, so that a
         * station's name never sits where a keyword sits and no word
         * is ever both. */
        fprintf(out, "\nstation %s ", written[i]);
        /*
         * The station knows its own name: the generated placement
         * function wrote it as a literal.
         *
         * **A station placed by hand with no name given has none**,
         * and saying so is more honest than inventing a spelling: a
         * program built that way is not described on disk either, so
         * there is nothing a station line could truthfully say. The
         * marker is deliberately not a legal box name, so a dump
         * carrying one cannot be read back in silence.
         */
        /* A door is a port now (issues 213a, 209a), so the station
         * line says nothing about one and the port lines say it all.
         * A program whose doors did not survive being written down
         * could not be composed after a round trip, which is most of
         * what marking them was for. */
        const char *door = "";

        /*
         * **Whichever form is unambiguous**. A station
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
                const cera_box_place_t *by_bare = cera_box_place_find(bare);
                const cera_box_place_t *by_address = cera_box_place_find(written_as);
                if (by_bare && by_bare == by_address)
                    written_as = bare;
            }
        }

        /*
         * **Where an iterator had got to**, written only
         * when it says something: zero is where one starts, and every
         * other kind of station has no position to be in. The format
         * writes exceptions, and a cursor at the beginning is not one.
         */
        char at[16] = "";
        if (s->kind == CERA_STATION_ITERATOR && s->cursor != 0)
            snprintf(at, sizeof at, " @%d", s->cursor);

        fprintf(out, "%s %c%s%s   # station %d\n",
                written_as ? written_as : "?placed-by-hand?",
                kind_letter(s->kind), door, at, i);

        for (int j = 0; j < s->n_in_ports; j++) {
            cera_in_port_t *sl = &s->in_ports[j];

            /*
             * A starting depth, written only when it differs from the
             * default. The format writes exceptions, and
             * a port at the default depth is not one — saying `x10` on
             * every line would be noise a reader learns to skip.
             *
             * It comes before the source because the inline value form
             * runs to the end of the line, so nothing can follow it.
             */
            char depth[32] = "";
            if (sl->capacity != CERA_IN_PORT_DEFAULT_CAPACITY)
                snprintf(depth, sizeof depth, "x%d ", sl->capacity);

            /*
             * **The receiving end of every wire that arrives here**
             * (issue 601a). A wire is written twice in the format, so
             * a dump that wrote only the arrows leaving each station
             * would produce a file its own reader refuses.
             *
             * Derived, every time, by asking every station what its
             * output ports point at — which is the same sweep removal
             * does, for the same reason: a wire lives on the producing
             * side and an input port carries nothing saying what feeds
             * it. Derived rather than remembered means these lines
             * cannot disagree with the wires they describe.
             *
             * Written before the port's own line, because they say
             * where the port's values come from and the line after
             * says what the port is.
             */
            /*
             * **This port is one of the map's arguments** (issue
             * 601b). Written before its wires and its value, because
             * it says what the port *is* to anyone outside, and the
             * lines after say what happens to it inside.
             */
            if (sl->argument != CERA_NOT_A_DOOR)
                fprintf(out, "  in %d $%d\n", j, sl->argument);

            int sources = 0;
            for (int pass = 0; pass < 2; pass++) {
                int seen = 0;
                for (int from = 0; from < m->n_stations; from++) {
                    cera_station_t *feeder = cera_map_station(m, from);
                    if (!feeder->call)
                        continue;
                    int from_port = 0;
                    for (cera_out_port_t *p = feeder->out_ports; p;
                         p = p->next, from_port++) {
                        cera_dest_set_t *set = out_port_dests(p);
                        for (int d = 0; set && d < set->n; d++) {
                            if (set->items[d].station != i
                                || set->items[d].port != j)
                                continue;
                            if (pass == 0) {
                                sources++;
                                continue;
                            }
                            /* The derived facts ride on the last of
                             * these rather than standing alone, so a
                             * port with wires reads as lines about
                             * that port rather than as lines with a
                             * note wedged between them. */
                            seen++;
                            if (seen == sources
                                && sl->kind == CERA_IN_PORT_RING
                                && !depth[0])
                                fprintf(out,
                                        "  in %d - %s.%d   # buffer, %s, "
                                        "%d bytes, %d slots\n",
                                        j, written[from], from_port,
                                        sl->type_name ? sl->type_name : "?",
                                        sl->elem_size, sl->capacity);
                            else
                                fprintf(out, "  in %d - %s.%d\n",
                                        j, written[from], from_port);
                        }
                    }
                }
            }

            switch (sl->kind) {
            case CERA_IN_PORT_STATIC: {
                /* The value itself, spoken from its bytes rather than
                 * echoed from remembered text, so a constant a runtime
                 * write changed dumps as what it is.
                 *
                 * Asked for its length first and then written, because
                 * a struct constant has no useful upper bound and a
                 * fixed buffer would quietly truncate exactly the
                 * values most worth reading. */
                int wanted = in_port_constant_text(sl, NULL, 0);
                char *text = malloc((size_t)wanted + 1);
                if (!text) {
                    cera_fail(CERA_EXIT_NO_RESOURCE, "dump: out of memory writing a constant\n");
                }
                in_port_constant_text(sl, text, wanted + 1);
                fprintf(out, "  in %d %s= %s   # %s, %d bytes\n", j, depth,
                        text, sl->type_name ? sl->type_name : "?",
                        sl->elem_size);
                free(text);
                break;
            }
            case CERA_IN_PORT_RING: {
                /*
                 * **Values waiting in the buffer, if any**.
                 * This is the difference between a schematic and
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
                        cera_fail(CERA_EXIT_NO_RESOURCE, "dump: out of memory writing "
                                        "waiting values\n");
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
                    /* The depth alone, with no source after it. A
                     * dash here would read back as a port with *no
                     * source*, so a program with a deepened buffer
                     * would not read in again. */
                    fprintf(out, "  in %d %s  # buffer, %s, %d bytes\n",
                            j, depth, sl->type_name ? sl->type_name : "?",
                            sl->elem_size);
                else if (!sources)
                    /* Nothing feeds this port, so there is no line to
                     * hang the facts on and they stand alone. */
                    fprintf(out,
                            "  # port %d: buffer, %s, %d bytes, %d slots\n",
                            j, sl->type_name ? sl->type_name : "?",
                            sl->elem_size, sl->capacity);
                break;
            }
            case CERA_IN_PORT_NONE:
                /*
                 * A port with no source at all, written as a bare
                 * dash. It is a state and not a value, so the station
                 * holding one never becomes ready — and a half-built
                 * program reloads.
                 */
                fprintf(out, "  in %d %s-   # no source yet, %s, %d bytes\n",
                        j, depth, sl->type_name ? sl->type_name : "?",
                        sl->elem_size);
                break;
            }
        }

        /* Written in array order, which is the order the wires were
         * drawn, so dump -> load -> dump produces the same text
         * without anybody arranging it. Nothing in the
         * running engine reads that order or means anything by it. */
        int out_port_index = 0;
        for (cera_out_port_t *p = s->out_ports; p; p = p->next, out_port_index++) {
            /* This port is one of the map's results (issue 601b).
             * Written before its wires, for the same reason an
             * argument mark comes before a port's: it says what the
             * port is to anyone outside. */
            if (p->result != CERA_NOT_A_DOOR)
                fprintf(out, "  out %d $%d\n", out_port_index, p->result);

            cera_dest_set_t *set = out_port_dests(p);
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

/* }}} */

/* {{{ 052 — changing a running program */
/* ==================================================================
 *
 * 052 — changing a running program
 *
 * Was src/052-rewire.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * Changing the shape while it runs.
 *
 * Three properties make it possible: wires hold station indices
 * rather than addresses, stations never move, and a destination list
 * is an immutable array published by one write, so a delivery walk
 * reads it without a lock.
 *
 * What this file does is connect and disconnect.
 *
 * One rewiring lock makes edge
 * validation and list mutation a single operation — two threads each
 * adding an individually legal edge can produce an illegal pair, so
 * the check and the insertion are never separated. List surgery
 * additionally happens under the owning station's mutex, the same
 * lock delivery snapshots under, so no walker can be left holding a
 * freed node.
 *
 * A refusal is handed back and the caller decides. A loader that dies
 * serves its author, but a running engine that dies because a control
 * surface sent one bad instruction takes the plant down with it.
 *
 * Two faces: one returns the reason, so somebody reading a file can
 * collect every mistake and present them together; one stops the
 * program, which is what construction wants. Neither can be ignored
 * into a half-built program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ said() */
/*
 * A refusal that travels upward instead of being printed where it
 * happened. The caller decides what to do with it —
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
    static const int limits[CERA_STATION_KIND_COUNT] = {
        [CERA_STATION_PLAIN] = 1, [CERA_STATION_COMPARATOR] = 3, [CERA_STATION_ITERATOR] = 0,
    };
    return kind < CERA_STATION_KIND_COUNT ? limits[kind] : 1;
}
/* }}} */

/* {{{ cera_map_wire() */
/*
 * **Draw a wire, at any moment** — while a program is
 * being assembled, or on a running one with workers in flight. There
 * is one implementation and it applies every rule, because the rules
 * were never about *when*: a sink has nothing to wire from whether or
 * not the pool has started, and a destination that is not a buffer
 * has nowhere to put a value either way.
 *
 * Returns NULL when the wire was drawn, or a sentence saying why not.
 * The string is valid until this thread's next refusal.
 */
const char *cera_map_wire(cera_map_t *m, int from_station, int port,
                     int to_station, int to_port)
{
    pthread_mutex_lock(&m->rewire_mutex);

    /* Every load-time rule, per edge. */
    if (from_station < 0 || from_station >= m->n_stations
        || to_station < 0 || to_station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("a station index outside the table");
    }
    cera_station_t *from = cera_map_station(m, from_station);
    cera_station_t *to = cera_map_station(m, to_station);
    /* An empty place in the table has no ports to wire and no size to
     * check against, so every question below would be asked of
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
         * Named the way the loader's own copy of this check named it,
         * because that copy is going: which station the
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
                 to->kind == CERA_STATION_COMPARATOR ? ", plus the threshold" : "");
        pthread_mutex_unlock(&m->rewire_mutex);
        return said(message);
    }
    cera_in_port_t *dest = &to->in_ports[to_port];
    /*
     * **A static destination is legal now**: a value
     * arriving there overwrites the constant rather than queueing,
     * which is how a constant gets computed at startup rather than
     * written down. What remains refused is a port with *no source*,
     * which has nothing to overwrite and nowhere to queue — the
     * arriving value would have nowhere to go at all.
     */
    if (dest->kind != CERA_IN_PORT_RING && dest->kind != CERA_IN_PORT_STATIC) {
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
     * The wire check, by **width** rather than by type name.
     * Identical layouts under different names now wire, which
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
         * it.** The source station's own input array has nothing to do
         * with the question: a box that takes nothing and returns a
         * value, which is how most programs start, is wired from just
         * like any other.
         *
         * The destination's array needs no guarding either. A port
         * index past the end was refused a few lines above, and a
         * station with no ports refuses every index there is.
         *
         * **The width decides, and only the width**. Two
         * boxes may spell one shape differently and mean the same
         * data, so a wire is legal when both sides count the same
         * bytes — comparing the names would refuse a connection that
         * is perfectly sound, which is the whole reason names stopped
         * being what a wire is checked against.
         */
        if (from->out_size != dest->elem_size) {
            /*
             * **The refusal names both ends by position and by size,
             * and no type name appears in it**.
             *
             * A name is not what makes a wire legal or illegal — the
             * width is — so a message built around names would invite
             * somebody to go and look at the names, which are not the
             * disagreement. Two types with one layout and different
             * names wire perfectly.
             *
             * What disagrees is a *place* and a *count* on each end,
             * so the message gives both: which station, which port,
             * and how many bytes it has.
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
    cera_out_port_t *p = station_out_port_make(from, port);
    if (!p) {
        pthread_mutex_unlock(&from->mutex);
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("out of memory for a port");
    }

    /*
     * A whole new set, published by one write. Walkers
     * already inside the old one keep walking it and are not
     * disturbed; the old set is filed rather than freed, because one
     * of them may be in it right now.
     */
    cera_dest_set_t *old = out_port_dests(p);
    cera_dest_set_t *fresh_set = dest_set_build(old, to_station, to_port, -1, -1);
    atomic_store_explicit(&p->dests, fresh_set, memory_order_release);
    pthread_mutex_unlock(&from->mutex);
    map_retire(m, old, free);

    pthread_mutex_unlock(&m->rewire_mutex);
    CERA_EMIT(m, CERA_WATCH_WIRED, from_station, port, to_station, to_port, 0);
    return NULL;
}
/* }}} */

/* {{{ cera_map_unwire() */
/*
 * **Cut one wire, at any moment.** NULL when it came out, or a
 * sentence saying why not — the same shape as drawing one, for the
 * same reason: a caller reading a description collects every mistake
 * and presents them together.
 *
 * **Two faces, and no third.** One hands the refusal back so a caller
 * can collect it; one stops the program. Nothing prints a refusal and
 * returns a code, which could be ignored into a program still running
 * that somebody believes they just edited.
 */
const char *cera_map_unwire(cera_map_t *m, int from_station, int port,
                       int to_station, int to_port)
{
    pthread_mutex_lock(&m->rewire_mutex);
    if (from_station < 0 || from_station >= m->n_stations) {
        pthread_mutex_unlock(&m->rewire_mutex);
        return said("a station index outside the table");
    }
    cera_station_t *from = cera_map_station(m, from_station);

    pthread_mutex_lock(&from->mutex);
    cera_out_port_t *p = station_out_port(from, port);
    cera_dest_set_t *old = out_port_dests(p);
    cera_dest_set_t *fresh_set = NULL;
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

    /* Filed, not freed: a walker may be inside the old set right now.
     * A value already on its way down the removed wire
     * is delivered, which is indistinguishable from having been
     * delivered a moment earlier and is fine. */
    map_retire(m, old, free);

    CERA_EMIT(m, CERA_WATCH_UNWIRED, from_station, port, to_station, to_port, 0);
    return NULL;
}
/* }}} */

/* {{{ cera_map_disconnect() */
/*
 * The same operation, for a caller that wants a refusal to stop the
 * program.
 */
void cera_map_disconnect(cera_map_t *m, int from_station, int port,
                    int to_station, int to_port)
{
    const char *no = cera_map_unwire(m, from_station, port, to_station, to_port);
    if (no)
        cera_stop_now(m, CERA_EXIT_BAD_CALL, no);
}
/* }}} */

/* {{{ type removed_parts_t */
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
    cera_station_t  *station;
    cera_out_port_t *out_ports;
    cera_in_port_t  *in_ports;
    int         n_in_ports;
    char       *name;
} removed_parts_t;
/* }}} */

/* {{{ reclaim_station() */
static void reclaim_station(void *p)
{
    removed_parts_t *r = p;

    cera_out_port_t *port = r->out_ports;
    while (port) {
        free(out_port_dests(port));
        cera_out_port_t *next = port->next;
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
    cera_station_t *s = r->station;
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

/* {{{ cera_map_remove_station() */
const char *cera_map_remove_station(cera_map_t *m, int station)
{
    return cera_map_remove_stations(m, &station, 1);
}
/* }}} */

/* {{{ names_one_of() */
/*
 * Whether an index is in the set being removed. A linear scan, because
 * a set is small and removal is rare — the walk that calls this is
 * already the expensive part, and an index structure to make a
 * ten-element search faster would be machinery answering a question
 * nobody asks often.
 */
static int names_one_of(const int *stations, int count, int which)
{
    for (int i = 0; i < count; i++)
        if (stations[i] == which)
            return 1;
    return 0;
}
/* }}} */

/* {{{ cera_map_remove_stations() */
/*
 * **A set of stations, removed in one sweep of the table.**
 *
 * A wire lives in exactly one place — a destination record on the
 * producing station's output port — and an input port carries nothing
 * saying what feeds it. So finding every wire that points at a station
 * means asking every station. For one that is right and cheap; for a
 * set it was the same walk repeated, once per member, each taking and
 * releasing a mutex per station visited.
 *
 * **The back-reference that would avoid the walk is deliberately not
 * built.** An input port could carry a list of its sources, and removal
 * would then ask only those. It is not worth it: every wire operation
 * would maintain two structures that can disagree, and the destination
 * set's entire safety argument is that it is immutable and swapped
 * whole. Delivery — the hot path, run constantly — only ever asks
 * *where does this value go*. Removal is rare. One sweep is the right
 * price.
 *
 * **Everything is marked before any wire is cut**, so the whole set
 * stops starting new work at one moment rather than one at a time. A
 * wire from one removed station to another is named by a member of the
 * set, so it is cut by the same pass — nothing has to know it was
 * interior.
 */
const char *cera_map_remove_stations(cera_map_t *m, const int *stations,
                                     int count)
{
    if (count <= 0)
        return NULL;
    if (!stations)
        return said("removing stations from a list that is not there");

    pthread_mutex_lock(&m->rewire_mutex);

    /*
     * Every index checked before anything changes, so a set with one
     * bad member leaves the program exactly as it was rather than
     * half-pruned. After this the only way to fail is running out of
     * memory.
     */
    for (int i = 0; i < count; i++) {
        int station = stations[i];
        if (station < 0 || station >= m->n_stations) {
            pthread_mutex_unlock(&m->rewire_mutex);
            return said("removing a station outside the table");
        }
        cera_station_t *s = cera_map_station(m, station);
        if (!s->call
            || atomic_load_explicit(&s->removed, memory_order_acquire)) {
            pthread_mutex_unlock(&m->rewire_mutex);
            return said("removing a station that is not there");
        }
        for (int j = 0; j < i; j++)
            if (stations[j] == station) {
                pthread_mutex_unlock(&m->rewire_mutex);
                return said("the same station named twice in one removal");
            }
    }

    /*
     * Marked first, so nothing new starts from any of them while the
     * wires are being cut. Values already on their way are discarded
     * when they arrive, which is what this engine already does with a
     * value that has nowhere to go.
     */
    for (int i = 0; i < count; i++) {
        cera_station_t *s = cera_map_station(m, stations[i]);
        pthread_mutex_lock(&s->mutex);
        atomic_store_explicit(&s->removed, 1, memory_order_release);
        pthread_mutex_unlock(&s->mutex);
    }

    /*
     * One pass. Because the marking happened first, nothing stale can
     * survive to be followed afterwards, which is what makes reusing a
     * place safe without a version on every wire.
     */
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *other = cera_map_station(m, i);
        if (!other->call)
            continue;
        pthread_mutex_lock(&other->mutex);
        for (cera_out_port_t *p = other->out_ports; p; p = p->next) {
            cera_dest_set_t *old = out_port_dests(p);
            if (!old)
                continue;
            int names_any = 0;
            for (int d = 0; d < old->n; d++)
                if (names_one_of(stations, count, old->items[d].station))
                    names_any = 1;
            if (!names_any)
                continue;
            /* Rebuilt without every wire to any member of the set, in
             * one new set rather than one per wire, so a walker sees
             * the before or the after and never a partial cut. */
            cera_dest_set_t *fresh =
                calloc(1, sizeof *fresh
                          + (size_t)(old->n > 0 ? old->n : 1)
                            * sizeof(cera_destination_t));
            if (!fresh) {
                pthread_mutex_unlock(&other->mutex);
                pthread_mutex_unlock(&m->rewire_mutex);
                return said("out of memory rebuilding a destination set");
            }
            int out = 0;
            for (int d = 0; d < old->n; d++)
                if (!names_one_of(stations, count, old->items[d].station))
                    fresh->items[out++] = old->items[d];
            fresh->n = out;
            atomic_store_explicit(&p->dests, fresh, memory_order_release);
            map_retire(m, old, free);
        }
        pthread_mutex_unlock(&other->mutex);
    }

    /*
     * Their parts handed to the scrapyard, which frees them and clears
     * the record once nobody can still be inside a task built from
     * them. Nothing is detached here: a task being built right now
     * reads the port count and the return size, and they have to still
     * be there.
     */
    for (int i = 0; i < count; i++) {
        int station = stations[i];
        cera_station_t *s = cera_map_station(m, station);

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
    }

    pthread_mutex_unlock(&m->rewire_mutex);

    /* Said after the lock is dropped, one per station, because a
     * watcher wants to see what happened rather than to be told about
     * it while it is still happening. */
    for (int i = 0; i < count; i++)
        CERA_EMIT(m, CERA_WATCH_REMOVED, stations[i], 0, 0, 0, 0);
    return NULL;
}
/* }}} */

/* }}} */

/* {{{ 074 — boxes and maps compiled at run time */
/* ==================================================================
 *
 * 074 — boxes and maps compiled at run time
 *
 * Was src/074-latebox.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * A box arriving after the program started.
 *
 * Five steps turn C source into a placeable box while a program runs —
 * save it, generate, compile, load, add — plus the growable half of
 * the table stations are placed from.
 *
 * It runs the same two programs the build runs. The generator turns a
 * box source into
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
 * inside the code being freed — the same retire-sweep-free mechanism
 * destination arrays and removed stations go through, which is why
 * there is one of it rather than three.
 *
 * **Libraries are opened globally**, so a box arriving
 * later can bind to one that arrived earlier instead of carrying its
 * own copy. That is what makes this an iterative compiler rather than
 * a sequence of unrelated compilations, and it is why unloading is
 * careful: something bound to may still be bound to.
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * Which compiler built this binary, where the generator is, and where
 * the headers generated code includes live. Defaults exist only so
 * this file compiles outside the project's Makefile; a real build
 * always defines all four.
 */
#ifndef CERA_CC
#define CERA_CC "cc"
#endif
#ifndef CERA_GENERATOR
#define CERA_GENERATOR "generate"
#endif
#ifndef CERA_INCLUDE
#define CERA_INCLUDE "."
#endif
#ifndef CERA_RAM_SHARED
#define CERA_RAM_SHARED "/dev/shm/minimal-soramech"
#endif
#ifndef CERA_RAM_EXEC
#define CERA_RAM_EXEC "/tmp/minimal-soramech"
#endif

/* {{{ type late_block_t */
/*
 * One dlopen's worth of rows. The arrays belong to the loaded object
 * and live as long as it does, which is forever — see the note about
 * unloading at the top of this file.
 */
typedef struct late_block {
    struct late_block *next;
    /* The placement functions this object brought with it.
     * A box compiled while the program runs has to be
     * placeable the same way as one compiled into it, which means the
     * same generated function doing the writing. */
    const cera_box_place_t *places;
    int                n_places;
    /* And the source it was compiled from, as text. The
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
    const cera_box_source_t *sources;
    int                 n_sources;
    void              *handle;
} late_block_t;
/* }}} */

/* {{{ late_head */
static late_block_t *late_head;   /* newest first */
static int           late_total;
/* }}} */

/* {{{ cera_late_source_dir() */
static int           late_serial; /* names the scratch files apart */

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
const char *cera_late_source_dir(void)
{
    return CERA_RAM_SHARED "/late-boxes";
}
/* }}} */

/* {{{ late_library_dir() */
static const char *late_library_dir(void)
{
    return CERA_RAM_EXEC "/late-boxes";
}
/* }}} */

/* {{{ cera_late_box_count() */
int cera_late_box_count(void)
{
    return late_total;
}
/* }}} */

/* {{{ cera_late_box_at() */
const cera_box_place_t *cera_late_box_at(int i)
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
static const cera_box_place_t *late_place_find(const char *name);
/* }}} */

/* {{{ late_place_find() */
static const cera_box_place_t *late_place_find(const char *name)
{
    for (late_block_t *b = late_head; b; b = b->next)
        for (int i = 0; i < b->n_places; i++)
            /* The same rule the compiled-in rows are searched by,
             * so a bare name, a basename and a path all
             * mean here what they mean there. */
            if (box_place_matches(&b->places[i], name))
                return &b->places[i];
    return NULL;
}
/* }}} */

/* {{{ cera_late_source_text() */
/*
 * The C a late-arriving source was compiled from, by the path it was
 * compiled under. Newest first, for the same reason the box lookup is:
 * a path compiled twice reports the newer text, which is what somebody
 * asking "what is running now" means by the question.
 *
 * Full path first and basename second, matching how a box is
 * addressed, so that a person can type what they can see.
 */
const char *cera_late_source_text(const char *path)
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

/* {{{ ensure_dir() */
static int ensure_dir(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return 0;
    fprintf(stderr, "latebox: cannot create %s: %s\n", path, strerror(errno));
    return -1;
}
/* }}} */

/* {{{ write_text() */
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

/* {{{ run() */
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

/* {{{ close_library() */
static void close_library(void *handle)
{
    dlclose(handle);
}
/* }}} */

/* {{{ cera_late_unload_box() */
int cera_late_unload_box(cera_map_t *m, const char *name)
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
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call)
            continue;
        /*
         * **By the name the station was placed as**, rather than by
         * comparing shim pointers. A station carries the name literal
         * its own placement function wrote.
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
     * one a replaced destination set uses.
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
static const cera_box_place_t *late_recover_box(const char *name);
/* }}} */

/* {{{ late_recover_box() */
static const cera_box_place_t *late_recover_box(const char *name)
{
    if (!name || !*name)
        return NULL;

    char path[512];
    snprintf(path, sizeof path, "%s/%s.c", cera_late_source_dir(), name);

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

    int added = cera_late_compile_source(text);
    free(text);
    if (added < 0) {
        fprintf(stderr, "latebox: '%s' could not be recovered from its own "
                        "saved source\n", name);
        return NULL;
    }
    return cera_box_place_find(name);
}
/* }}} */

/* {{{ ensure_path_dirs() */
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

/* {{{ spill_sources() */
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

    for (int i = 0; i < cera_n_box_sources; i++) {
        if (n >= cap)
            break;
        if (snprintf(full, sizeof full, "%s/%s",
                     dir, cera_box_sources[i].path) >= (int)sizeof full) {
            fprintf(stderr, "latebox: path too long: %s\n",
                    cera_box_sources[i].path);
            return -1;
        }
        if (ensure_path_dirs(full) != 0)
            return -1;
        if (write_text(full, cera_box_sources[i].text) != 0)
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

/* {{{ gather_missing_boxes() */
/*
 * **A description may name a box this program does not hold**, and
 * getting it is the ordinary path rather than a rescue.
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
             CERA_GENERATOR, map_path, list_path);
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
        if (cera_box_place_find(name))
            continue;
        late_recover_box(name);
    }
    fclose(f);
}
/* }}} */

/* {{{ cera_late_spill_sources() */
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
int cera_late_spill_sources(const char *dir)
{
    enum { MAX_SPILLED = 256 };
    const char *paths[MAX_SPILLED];
    int n = spill_sources(dir, paths, MAX_SPILLED);
    for (int i = 0; i < n; i++)
        free((void *)paths[i]);
    return n;
}
/* }}} */

/* {{{ cera_late_compile_map() */
const cera_map_build_t *cera_late_compile_map(const char *map_text)
{
    if (!map_text || !*map_text) {
        fprintf(stderr, "latebox: an empty description describes nothing\n");
        return NULL;
    }

    const char *dir = cera_late_source_dir();
    const char *libdir = late_library_dir();
    if (ensure_dir(CERA_RAM_SHARED) != 0 || ensure_dir(dir) != 0)
        return NULL;
    if (ensure_dir(CERA_RAM_EXEC) != 0 || ensure_dir(libdir) != 0)
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
                      CERA_GENERATOR, gen_path, src_root, map_path);
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
             "%s -std=gnu11 -O2 -fPIC -shared -I%s -o %s %s",
             CERA_CC, CERA_INCLUDE, lib_path, gen_path);
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

    const cera_map_build_t *builds = dlsym(handle, "cera_map_builds");
    const int *count = dlsym(handle, "cera_n_map_builds");
    if (!builds || !count || *count <= 0) {
        fprintf(stderr, "latebox: %s builds no description — the generator "
                        "emitted something unexpected\n", lib_path);
        dlclose(handle);
        return NULL;
    }
    return &builds[0];
}
/* }}} */

/* {{{ cera_late_compile_source() */
int cera_late_compile_source(const char *c_source)
{
    if (!c_source || !*c_source) {
        fprintf(stderr, "latebox: asked to compile nothing\n");
        return -1;
    }

    const char *dir = cera_late_source_dir();
    const char *libdir = late_library_dir();
    if (ensure_dir(CERA_RAM_SHARED) != 0 || ensure_dir(dir) != 0)
        return -1;
    if (ensure_dir(CERA_RAM_EXEC) != 0 || ensure_dir(libdir) != 0)
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

    snprintf(cmd, sizeof cmd, "%s %s %s", CERA_GENERATOR, gen_path, box_path);
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the generator refused %s\n", box_path);
        return -1;
    }

    /* Position-independent and shared, with the engine's headers
     * reachable because generated code includes them. The compiler is
     * the one that built this binary, which is what makes its answer
     * to sizeof the same answer. */
    snprintf(cmd, sizeof cmd,
             "%s -std=gnu11 -O2 -fPIC -shared -I%s -o %s %s",
             CERA_CC, CERA_INCLUDE, lib_path, gen_path);
    if (run(cmd) != 0) {
        fprintf(stderr, "latebox: the compiler refused the generated "
                        "generated source for %s\n", box_path);
        return -1;
    }

    /*
     * **Opened globally, so that the next arrival can bind to this
     * one.** Opened privately, every arrival would be an island: a
     * second one naming a function the first had already compiled
     * would have to carry its own copy.
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
     * **One pair of symbols.** The generated file defines placement
     * functions and nothing beside them: every number a station needs
     * is written straight onto it by the placement function, from a
     * `sizeof` the compiler folded.
     */
    const cera_box_place_t *places = dlsym(handle, "box_places");
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

    /* The source text the object carries. Absent is not
     * an error the way absent placement functions are: an object built
     * by an older generator has boxes but no text, and refusing to
     * load it would trade a working box for a missing document. What
     * it costs is that this source cannot be written back out, and the
     * lookup answers NULL rather than pretending. */
    const cera_box_source_t *sources = dlsym(handle, "cera_box_sources");
    const int *n_sources = dlsym(handle, "cera_n_box_sources");
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

/* }}} */

/* {{{ 092 — signals, capture, and the end */
/* ==================================================================
 *
 * 092 — signals, capture, and the end
 *
 * Was src/092-stopping.c. The number is this section's position in the
 * reading order, which is the only thing the filename ever said.
 * ================================================================== */
/*
 * Every way a program ends except the happy one. The interface is in
 * cera.h.
 *
 * **Nothing here is a signal handler.** The three signals are blocked in every thread and the
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

/* {{{ finished_signal */
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
/* }}} */

/* {{{ report_fd */
/* Where a report goes. Opened during preparation and never after,
 * because a dying program cannot answer for a failed open. */
static int  report_fd = -1;
/* }}} */

/* {{{ report_where */
static char report_where[512];
/* }}} */

/* {{{ interrupts */
/* How many interrupts have arrived. The second one is an escape
 * hatch and takes no other path with it. */
static int interrupts;
/* }}} */

/* {{{ escape_now() */
/*
 * The only signal handler in this file, installed for the length of
 * one gather and doing the one thing a handler is unarguably allowed
 * to do. See the second-interrupt case in cera_wait for why it has to
 * exist at all.
 */
static void escape_now(int sig)
{
    (void)sig;
    _exit(CERA_EXIT_INTERRUPTED);
}
/* }}} */

/* {{{ say() */
/*
 * One line into the report. Formatted into a stack buffer and written
 * with one call, because **write is the only file operation available
 * on every path in this file** — including the one forbidden to take
 * a lock, and a buffered stream takes one.
 */
static void say(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
/* }}} */

/* {{{ say() */
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

/* {{{ error_handler */
/*
 * The one way the engine ends a program it refuses to continue.
 *
 * Every refusal formats its message, hands it to the installed handler
 * if there is one, and then ends the process. There is one of these so
 * that a host has one place to be told from; thirty scattered writes to
 * stderr followed by thirty aborts gave it none.
 *
 * `cera_fail` exits with the code it is given: a fault outside the
 * engine, which a caller may be able to correct. `cera_bug` aborts,
 * leaving a core: the engine found a fault in itself, and the core is
 * the evidence.
 *
 * The handler is read once, into a local, before it is called. Nothing
 * stops a host installing one from another thread while a program is
 * dying, and calling through a pointer that was read twice is a way to
 * call through a null.
 */
static cera_error_fn error_handler = NULL;
/* }}} */

/* {{{ cera_on_error() */
void cera_on_error(cera_error_fn fn)
{
    error_handler = fn;
}
/* }}} */

/* {{{ tell_the_host() */
/* The message reaches the handler without its trailing newline, since a
 * host putting it in a structured log wants the sentence and not the
 * line break the terminal wanted. */
static void tell_the_host(const char *message, int exit_code)
{
    cera_error_fn fn = error_handler;
    if (!fn)
        return;

    size_t n = strlen(message);
    while (n > 0 && (message[n - 1] == '\n' || message[n - 1] == '\r'))
        n--;

    char trimmed[1024];
    if (n >= sizeof trimmed)
        n = sizeof trimmed - 1;
    memcpy(trimmed, message, n);
    trimmed[n] = '\0';
    fn(trimmed, exit_code);
}
/* }}} */

/* {{{ say_and_end() */
static void say_and_end(int exit_code, int leave_core, const char *fmt, va_list ap)
{
    char message[1024];
    vsnprintf(message, sizeof message, fmt, ap);

    fputs(message, stderr);
    tell_the_host(message, exit_code);

    if (leave_core)
        abort();
    exit(exit_code);
}
/* }}} */

/* {{{ cera_fail() */
static void cera_fail(int exit_code, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    say_and_end(exit_code, 0, fmt, ap);
    va_end(ap);
}
/* }}} */

/* {{{ cera_bug() */
static void cera_bug(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    say_and_end(CERA_EXIT_BUG, 1, fmt, ap);
    va_end(ap);
}
/* }}} */

/* {{{ cera_prepare() */
void cera_prepare(const char *report_path)
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
        cera_fail(CERA_EXIT_NO_RESOURCE, "stopping: could not block the signals this "
                        "program answers\n");
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
        mkdir(CERA_RAM_SHARED, 0777);
        snprintf(report_where, sizeof report_where,
                 "%s/stopping-%d.txt", CERA_RAM_SHARED, (int)getpid());
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

/* {{{ cera_report_path() */
const char *cera_report_path(void)
{
    return report_where;
}
/* }}} */

/* {{{ report_without_locks() */
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
static void report_without_locks(cera_map_t *m)
{
    say("== the program stopped on demand, taking no locks ==\n");
    if (!m) {
        say("(no program)\n");
        return;
    }

    int n = m->n_stations;
    say("stations: %d\n", n);
    for (int i = 0; i < n; i++) {
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call)
            continue;
        say("  station %d: %ld run, %ld produced\n", i,
            (long)atomic_load_explicit(&s->runs, memory_order_relaxed),
            (long)atomic_load_explicit(&s->produced, memory_order_relaxed));
    }

    if (m->pool) {
        int workers = cera_pool_worker_count(m->pool);
        say("workers: %d\n", workers);
        for (int i = 0; i < workers; i++) {
            int at = cera_pool_worker_station(m->pool, i);
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

/* {{{ report_everything() */
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
static void report_everything(cera_map_t *m)
{
    say("== the program was interrupted ==\n");
    if (!m) {
        say("(no program)\n");
        return;
    }

    if (m->pool)
        say("tasks queued and never run: %d\n", cera_pool_queued(m->pool));

    say("\n-- stations --\n");
    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
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
            cera_in_port_t *sl = &s->in_ports[j];
            if (atomic_load_explicit(&sl->kind, memory_order_relaxed)
                != CERA_IN_PORT_RING)
                continue;
            say("    port %d: %d waiting, %d deepest, grown %d times\n",
                j, cera_map_in_port_depth(m, i, j), sl->high_water, sl->growths);
        }
    }

    if (m->pool) {
        say("\n-- workers --\n");
        int workers = cera_pool_worker_count(m->pool);
        for (int i = 0; i < workers; i++) {
            int at = cera_pool_worker_station(m->pool, i);
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
     * shape it had at the end, which a program that can be built while
     * running need not share with the file on disk.
     */
    say("\n-- the program as it stands --\n");
    FILE *f = fdopen(dup(report_fd), "a");
    if (f) {
        cera_map_dump(m, f);
        fclose(f);
    }
}
/* }}} */

/* {{{ cera_wait() */
int cera_wait(cera_map_t *m)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, finished_signal);

    if (m && m->pool)
        cera_pool_signal_when_finished(m->pool, finished_signal);

    for (;;) {
        int sig = 0;
        if (sigwait(&set, &sig) != 0)
            continue;

        if (sig == finished_signal) {
            /* The ordinary ending. The last sleeper already broadcast
             * shutdown; this only collects the threads. */
            if (m && m->pool)
                cera_pool_join(m->pool);
            return CERA_EXIT_FINISHED;
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
                _exit(CERA_EXIT_INTERRUPTED);

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
                cera_pool_stop(m->pool);
            report_everything(m);
            return CERA_EXIT_INTERRUPTED;
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
             *
             * **This is the one death that does not go through the
             * funnel**, and for the same reason it takes no locks: an
             * error handler is somebody else's code, and somebody
             * else's code may take a lock. The whole point of this
             * path is that it works when a lock is held by something
             * that will never release it.
             */
            report_without_locks(m);
            abort();
        }
    }
}
/* }}} */

/* {{{ write_capture() */
static int write_capture(cera_map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "capture: cannot write %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    cera_map_dump(m, f);
    if (fclose(f) != 0) {
        fprintf(stderr, "capture: cannot finish writing %s: %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}
/* }}} */

/* {{{ cera_capture() */
int cera_capture(cera_map_t *m, const char *path)
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
        cera_pool_release(m->pool);
        cera_pool_join(m->pool);
    }

    return write_capture(m, path);
}
/* }}} */

/* {{{ write_capture_report() */
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
 * **Nothing here is measured for this.** Every number is already
 * being kept: run and produced counts per station, buffer depths and
 * growths per port, and the list of boxes that arrived after the
 * program started. A report needing its own instrumentation would be
 * a report that changed what it was reporting on.
 */
static int write_capture_report(cera_map_t *m, const char *path)
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
        cera_station_t *s = cera_map_station(m, i);
        if (!s->call)
            continue;
        total_runs += atomic_load_explicit(&s->runs, memory_order_relaxed);
    }
    fprintf(f, "stations: %d, tasks run: %ld\n\n", m->n_stations, total_runs);

    for (int i = 0; i < m->n_stations; i++) {
        cera_station_t *s = cera_map_station(m, i);
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
            cera_in_port_t *sl = &s->in_ports[j];
            if (atomic_load_explicit(&sl->kind, memory_order_relaxed)
                != CERA_IN_PORT_RING)
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
    int late = cera_late_box_count();
    fprintf(f, "\nboxes that arrived while it ran: %d\n", late);
    for (int i = 0; i < late; i++) {
        const cera_box_place_t *row = cera_late_box_at(i);
        if (row)
            fprintf(f, "    %s\n", row->address);
    }

    if (m->pool) {
        int workers = cera_pool_worker_count(m->pool);
        int busy = 0;
        for (int i = 0; i < workers; i++)
            if (cera_pool_worker_station(m->pool, i) >= 0)
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

/* {{{ cera_capture_whole() */
int cera_capture_whole(cera_map_t *m, const char *dir)
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
    if (cera_late_spill_sources(dir) < 0)
        return -1;

    char path[1024];
    if (snprintf(path, sizeof path, "%s/program.map", dir)
        >= (int)sizeof path) {
        fprintf(stderr, "capture: path too long: %s\n", dir);
        return -1;
    }
    if (cera_capture(m, path) != 0)
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

/* {{{ cera_capture_now() */
int cera_capture_now(cera_map_t *m, const char *path)
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

/* {{{ cera_stop_now() */
void cera_stop_now(cera_map_t *m, int exit_code, const char *why)
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
        cera_pool_stop(m->pool);
    say("== an invalid operation ended this program ==\n%s\n",
        why ? why : "an invalid operation");
    report_everything(m);

    _exit(exit_code);
}
/* }}} */

/* }}} */

/* {{{ 118 — watching a running program */
/* ==================================================================
 *
 * 118 — watching a running program
 * ================================================================== */
/*
 * A ring of fixed-size events in shared memory, written by every worker
 * and read by anybody who opens the file. The program never waits for a
 * reader, never learns one is there, and cannot be slowed by one: a
 * writer that catches up with a reader overwrites it, and the reader
 * works out exactly how much it missed from the sequence numbers.
 *
 * The emitting is compiled in only under CERA_WATCH. The reading is
 * always compiled, because a watcher is a different program from the
 * one being watched and has no reason to have been built with watching
 * turned on. Both halves are here so the ring's shape is written down
 * once.
 */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define CERA_WATCH_MAGIC   0x43455241u   /* "CERA" */
#define CERA_WATCH_VERSION 1u

/* {{{ type cera_watch_head_t */
/*
 * What sits at the front of the ring file. `next` is the only thing
 * writers contend on: claiming a slot is one atomic increment of it,
 * and the slot claimed is that sequence modulo the slot count.
 */
typedef struct cera_watch_head {
    uint32_t magic;
    uint32_t version;
    uint32_t slots;
    uint32_t slot_size;
    uint64_t writer_pid;
    _Atomic uint64_t next;
} cera_watch_head_t;
/* }}} */

/* {{{ struct cera_watch */
struct cera_watch {
    cera_watch_head_t  *head;
    cera_watch_event_t *ring;
    size_t              bytes;
    char                path[512];
};
/* }}} */

/* {{{ struct cera_watch_reader */
struct cera_watch_reader {
    cera_watch_head_t  *head;
    cera_watch_event_t *ring;
    size_t              bytes;
    uint64_t            cursor;
    uint64_t            joined;
};
/* }}} */

/* {{{ watch_slots_for() */
/*
 * Room for a few hundred events per station, rounded up to a power of
 * two so the modulo is a mask, with a floor that keeps a two-station
 * program from getting a ring too small to see anything in.
 */
static uint32_t watch_slots_for(int stations)
{
    /*
     * Generous on purpose. A slot is thirty-two bytes, so even the
     * floor here is two megabytes of shared memory — nothing, against
     * the cost of a watcher being told it missed something because the
     * ring was small rather than because it was slow.
     */
    uint64_t want = (uint64_t)(stations > 0 ? stations : 1) * 4096u;
    uint32_t slots = 1u << 16;
    while (slots < want && slots < (1u << 22))
        slots <<= 1;
    return slots;
}
/* }}} */

#ifdef CERA_WATCH
/* {{{ watch_now_ns() */
static uint64_t watch_now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
/* }}} */
#endif

/* {{{ cera_watch_compiled_in() */
/* Whether this binary can be watched at all. A watcher pointed at a
 * program built without the flag would otherwise wait forever for
 * events that were never going to arrive. */
int cera_watch_compiled_in(void)
{
#ifdef CERA_WATCH
    return 1;
#else
    return 0;
#endif
}
/* }}} */

/* {{{ cera_watch_kind_name() */
const char *cera_watch_kind_name(int kind)
{
    static const char *const names[CERA_WATCH_KIND_COUNT] = {
        "came up", "due", "ran", "moved", "grew",
        "added", "removed", "wired", "unwired", "finished"
    };
    if (kind < 0 || kind >= CERA_WATCH_KIND_COUNT)
        return "?";
    return names[kind];
}
/* }}} */

/* {{{ cera_watch_open() */
/*
 * Create the ring and start emitting into it. Returns NULL, or a
 * sentence saying why not.
 *
 * The path is the caller's: passing the same one every run overwrites
 * the same ring and accumulates nothing, and passing a fresh one keeps
 * a dead program's last moments to be read afterwards.
 */
const char *cera_watch_open(cera_map_t *m, const char *path)
{
    static char refusal[640];

    if (!cera_watch_compiled_in())
        return "this program was built without CERA_WATCH, so it emits nothing";

    /* Two programs writing one ring would interleave two graphs into one
     * stream with nothing saying which was which. Refusing costs one
     * check; allowing would cost a field on every event.
     *
     * **The same process is not exempt.** Two maps in one process are
     * two programs — that is the whole of what makes them separable —
     * so a second one seizing the first's ring would truncate it under
     * a reader that had been following it. Closing a trail clears the
     * owner, which is how a program reuses its own path. */
    int probe = open(path, O_RDONLY);
    if (probe >= 0) {
        cera_watch_head_t peek;
        ssize_t got = read(probe, &peek, sizeof peek);
        close(probe);
        if (got == (ssize_t)sizeof peek && peek.magic == CERA_WATCH_MAGIC
            && peek.writer_pid != 0
            && kill((pid_t)peek.writer_pid, 0) == 0) {
            snprintf(refusal, sizeof refusal,
                     "%s is already being written by process %llu, which is "
                     "still running", path, (unsigned long long)peek.writer_pid);
            return refusal;
        }
    }

    uint32_t slots = watch_slots_for(m->n_stations);
    size_t bytes = sizeof(cera_watch_head_t) + (size_t)slots * sizeof(cera_watch_event_t);

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        snprintf(refusal, sizeof refusal, "cannot create the trail at %s: %s",
                 path, strerror(errno));
        return refusal;
    }
    if (ftruncate(fd, (off_t)bytes) != 0) {
        close(fd);
        snprintf(refusal, sizeof refusal, "cannot size the trail at %s: %s",
                 path, strerror(errno));
        return refusal;
    }

    void *at = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (at == MAP_FAILED) {
        snprintf(refusal, sizeof refusal, "cannot map the trail at %s: %s",
                 path, strerror(errno));
        return refusal;
    }

    cera_watch_t *w = calloc(1, sizeof *w);
    if (!w) {
        munmap(at, bytes);
        return "out of memory opening the trail";
    }
    w->head = at;
    w->ring = (cera_watch_event_t *)((char *)at + sizeof(cera_watch_head_t));
    w->bytes = bytes;
    snprintf(w->path, sizeof w->path, "%s", path);

    memset(at, 0, bytes);
    w->head->slots = slots;
    w->head->slot_size = (uint32_t)sizeof(cera_watch_event_t);
    w->head->writer_pid = (uint64_t)getpid();
    w->head->version = CERA_WATCH_VERSION;
    atomic_store(&w->head->next, 1);   /* zero means "never written" */
    /* Written last, so a reader that maps a half-built file sees no
     * magic rather than a header it can believe. */
    w->head->magic = CERA_WATCH_MAGIC;

    m->watch = w;
    return NULL;
}
/* }}} */

/* {{{ cera_watch_close() */
/* Stops the emitting and unmaps. The file stays: a reader attaching
 * afterwards is the whole reason to keep it. */
void cera_watch_close(cera_map_t *m)
{
    cera_watch_t *w = m->watch;
    if (!w)
        return;
    m->watch = NULL;
    w->head->writer_pid = 0;
    munmap(w->head, w->bytes);
    free(w);
}
/* }}} */

/*
 * Compiled only under CERA_WATCH: without it this function does not
 * exist and neither does any call to it.
 *
 * One event. Claim a sequence, fill the slot it lands in, then publish
 * by storing the sequence into the slot with release ordering — so a
 * reader that sees the sequence sees the fields that came with it.
 *
 * Nothing here can block and nothing can fail. A writer that laps a
 * reader simply overwrites, which is the only arrangement in which
 * watching cannot slow the thing watched.
 */
#ifdef CERA_WATCH
/* {{{ watch_emit() */
static void watch_emit(cera_map_t *m, int kind,
                       uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                       uint64_t ns)
{
    cera_watch_t *w = m ? m->watch : NULL;
    if (!w)
        return;

    uint64_t seq = atomic_fetch_add(&w->head->next, 1);
    cera_watch_event_t *slot = &w->ring[seq & (w->head->slots - 1)];

    slot->ns = ns ? ns : watch_now_ns();
    slot->kind = (uint32_t)kind;
    slot->a = a; slot->b = b; slot->c = c; slot->d = d;
    atomic_store_explicit((_Atomic uint64_t *)&slot->seq, seq, memory_order_release);
}
/* }}} */
#endif

/* {{{ cera_watch_attach() */
/*
 * Open somebody else's ring for reading. Read-only, and the reader is
 * invisible to the program: nothing here writes, locks, or announces
 * itself.
 */
cera_watch_reader_t *cera_watch_attach(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;

    struct stat st;
    if (fstat(fd, &st) != 0 || (size_t)st.st_size < sizeof(cera_watch_head_t)) {
        close(fd);
        return NULL;
    }
    void *at = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (at == MAP_FAILED)
        return NULL;

    cera_watch_head_t *head = at;
    if (head->magic != CERA_WATCH_MAGIC || head->version != CERA_WATCH_VERSION
        || head->slot_size != sizeof(cera_watch_event_t)
        || head->slots == 0 || (head->slots & (head->slots - 1)) != 0) {
        munmap(at, (size_t)st.st_size);
        return NULL;
    }

    cera_watch_reader_t *r = calloc(1, sizeof *r);
    if (!r) {
        munmap(at, (size_t)st.st_size);
        return NULL;
    }
    r->head = head;
    r->ring = (cera_watch_event_t *)((char *)at + sizeof(cera_watch_head_t));
    r->bytes = (size_t)st.st_size;

    /*
     * **Start at the oldest event still in the ring, not at the first
     * the program ever wrote.**
     *
     * A reader attaching to a program that has been running for an hour
     * has not *lost* the hour: it was not there. Starting at one and
     * then reporting the difference as loss told every fresh watcher
     * that its view was unreliable, when the only thing that had
     * happened was that it arrived late — which is the ordinary case
     * and the one every page reload creates.
     *
     * What it did miss is still knowable: `joined` records where it
     * came in, so a watcher can say how much of the run it did not see
     * without calling it a fault.
     */
    uint64_t produced = atomic_load_explicit(&head->next, memory_order_acquire);
    r->cursor = produced > head->slots ? produced - head->slots : 1;
    r->joined = r->cursor;
    return r;
}
/* }}} */

/* {{{ cera_watch_next() */
/*
 * The next event, or 0 when there is nothing new yet.
 *
 * `lost` comes back with how many events were overwritten before this
 * one could be read. A reader too slow for the ring is told exactly how
 * far behind it fell rather than quietly showing an incomplete picture.
 */
int cera_watch_next(cera_watch_reader_t *r, cera_watch_event_t *into, uint64_t *lost)
{
    *lost = 0;

    uint64_t produced = atomic_load_explicit(&r->head->next, memory_order_acquire);
    if (r->cursor >= produced)
        return 0;

    /* Anything older than one lap is already overwritten. */
    uint64_t oldest = produced > r->head->slots ? produced - r->head->slots : 1;
    if (r->cursor < oldest) {
        *lost = oldest - r->cursor;
        r->cursor = oldest;
        if (r->cursor >= produced)
            return 0;
    }

    cera_watch_event_t *slot = &r->ring[r->cursor & (r->head->slots - 1)];
    uint64_t seq = atomic_load_explicit((_Atomic uint64_t *)&slot->seq,
                                        memory_order_acquire);
    if (seq != r->cursor)
        return 0;   /* claimed but not yet published, or already lapped */

    *into = *slot;
    r->cursor++;
    return 1;
}
/* }}} */

/* {{{ cera_watch_joined_at() */
/*
 * Which event this reader started from, and how many the program had
 * already written by then.
 *
 * A reader that arrives late has not lost anything — it was not there —
 * and a view that says otherwise makes every page reload look like a
 * fault. Loss is what `cera_watch_next` reports: events that were
 * overwritten *while this reader was already attached*.
 */
void cera_watch_joined_at(cera_watch_reader_t *r, uint64_t *first, uint64_t *before)
{
    if (first) *first = r->joined;
    if (before) *before = r->joined > 1 ? r->joined - 1 : 0;
}
/* }}} */

/* {{{ cera_watch_writer_alive() */
/* Whether the program that wrote this ring is still running. A viewer
 * uses it to say "this program has ended" rather than sitting on a
 * stream that will never move again. */
int cera_watch_writer_alive(cera_watch_reader_t *r)
{
    uint64_t pid = r->head->writer_pid;
    if (pid == 0)
        return 0;
    return kill((pid_t)pid, 0) == 0;
}
/* }}} */

/* {{{ cera_watch_detach() */
void cera_watch_detach(cera_watch_reader_t *r)
{
    if (!r)
        return;
    munmap(r->head, r->bytes);
    free(r);
}
/* }}} */
/* }}} */
