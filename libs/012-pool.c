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
#include "011-pool.h"

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
