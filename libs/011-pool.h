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
#ifndef SORA_POOL_H
#define SORA_POOL_H

/*
 * A task is one unit of work. The pool knows exactly one thing about
 * it: the first field is a function to call with the task itself as
 * argument. Everything after that field belongs to whoever created
 * the task — phase 1 tests hang synthetic payloads off it, and issue
 * 206 grows it into the engine's real task struct. The pool never
 * looks past `call`.
 */
typedef struct task task_t;
typedef void (*task_call_t)(task_t *t);

struct task {
    task_call_t call;
};

typedef struct pool pool_t;

/*
 * The finish hook is delivery's reserved seat (issue 205). After a
 * worker runs a task and before it frees it, the hook is called with
 * the context given at creation. Passing a null hook means "just free
 * it", which is all phase 1 needs. The hook is how the pool stays
 * ignorant of stations while still carrying their values forward.
 */
typedef void (*pool_finish_t)(void *ctx, task_t *t);

/* {{{ pool_create() — issues 101, 102 */
/*
 * n_workers <= 0 means: use the SORAMECH_WORKERS environment variable
 * if set, otherwise one worker per online processor. Workers are
 * spawned immediately but parked at a starting barrier; nothing runs
 * until pool_release. That gap is where a map is seeded (issue 605).
 */
pool_t *pool_create(int n_workers, pool_finish_t finish, void *finish_ctx);
/* }}} */

/* {{{ pool_push() — issues 101, 103 */
/*
 * Enqueue one task. Grows the ring if it is full (a bounded memory
 * copy, never a wait on user code) and wakes every sleeping worker.
 * The task must have been allocated with malloc; the worker that runs
 * it frees it.
 */
void pool_push(pool_t *p, task_t *t);
/* }}} */

/* {{{ pool_pop() — issue 101 */
/*
 * Dequeue the oldest task, or return null when the queue is empty.
 * Never blocks: the decision to sleep on empty belongs to the worker
 * run loop (issue 103), not to the queue.
 */
task_t *pool_pop(pool_t *p);
/* }}} */

/* {{{ pool_release() — issue 102 */
/* Release the workers past the starting barrier. Called once. */
void pool_release(pool_t *p);
/* }}} */

/* {{{ pool_join() — issue 104 */
/*
 * Wait until the pool has terminated itself — every task run, every
 * worker returned. Termination is decided by the last worker to fall
 * asleep re-checking the queue (issue 104), so this is a wait for
 * genuine completion, not a deadline.
 */
void pool_join(pool_t *p);
/* }}} */

/* {{{ pool_destroy() — issue 101 */
/* Free the pool. If workers were started, join them first. */
void pool_destroy(pool_t *p);
/* }}} */

/* {{{ pool_submitter_register() / pool_submitter_unregister() — issue 104 */
/*
 * The termination rule assumes nothing outside the pool pushes tasks
 * after the workers are released. Anything that does — a trickle-feed
 * test, a control socket — must hold a registration for as long as it
 * might still push, or the pool can declare itself finished between
 * two of its pushes. Unregistering the last outside submitter nudges
 * the workers so a fully-asleep pool re-evaluates termination.
 */
void pool_submitter_register(pool_t *p);
void pool_submitter_unregister(pool_t *p);
/* }}} */

/* {{{ pool_worker_index() — issue 102 */
/*
 * The index of the worker running the calling thread, or -1 when
 * called from a thread that is not a worker. Costs a thread-local
 * read; exists so phase 7 can attribute statistics per worker.
 */
int pool_worker_index(void);
/* }}} */

/* {{{ pool_worker_count() — issue 102 */
/* How many workers this pool actually has, after defaulting. */
int pool_worker_count(pool_t *p);
/* }}} */

/* {{{ pool_queue_stats() — issue 105 */
/*
 * The queue's current capacity, its high-water occupancy, and how
 * many times it has grown. Written for the phase 1 demo: every number
 * a demo reports must be measured, and these are the measurements.
 */
void pool_queue_stats(pool_t *p, int *capacity, int *high_water, int *growths);
/* }}} */

#endif
