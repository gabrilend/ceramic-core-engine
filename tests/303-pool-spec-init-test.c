/* tests/303-pool-spec-init-test.c — end-to-end test of the
 * spec_registry → pool worker-init plumbing.
 *
 * Loads the spec registry, registers a per-worker init callback
 * with the pool that invokes spec_registry_init_worker on each
 * worker's handles[] array, runs the barrier, then spawns one
 * task per worker that reads pool_current_worker->handles[lua_idx]
 * and stashes it for the main thread to verify. Every worker
 * should have a non-NULL Lua handle after the barrier releases.
 *
 * Run from the project root so `langs/` is reachable.
 */

#include "011-spec-registry.h"
#include "lang-spec.h"
#include "pool.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* {{{ Test harness */
static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "      %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        return 0; \
    } } while (0)

#define RUN(name) \
    do { \
        fprintf(stdout, "  %-44s ", #name); fflush(stdout); \
        if (test_##name()) { fprintf(stdout, "ok\n"); g_pass++; } \
        else                { fprintf(stdout, "FAIL\n"); g_fail++; } \
    } while (0)
/* }}} */

/* {{{ spec_pool_init_cb() — the canonical pool init callback */
/* Wires spec_registry_init_worker into pool_set_worker_init.
 * user is the spec_registry_t *. */
static int spec_pool_init_cb(int worker_idx, worker_ctx_t *ctx, void *user)
{
    spec_registry_t *r = (spec_registry_t *)user;
    return spec_registry_init_worker(r, worker_idx, ctx->handles,
                                     POOL_LANG_SLOTS) >= 0 ? 0 : -1;
}
/* }}} */

/* {{{ Shared state for the read-back-from-worker tests */
#define MAX_OBS 32
static struct {
    int   thread_idx;
    void *lua_handle;
} g_obs[MAX_OBS];
static atomic_int g_obs_count;
static int        g_lua_idx;

static void observe_action(void *arg)
{
    (void)arg;
    int slot = atomic_fetch_add(&g_obs_count, 1);
    if (slot < MAX_OBS) {
        g_obs[slot].thread_idx = pool_current_worker->thread_idx;
        g_obs[slot].lua_handle = pool_current_worker->handles[g_lua_idx];
    }
}
/* }}} */

/* {{{ test_handles_populated() */
static int test_handles_populated(void)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    ASSERT(r);

    /* Find the lua spec's index. */
    g_lua_idx = -1;
    for (int i = 0; i < spec_registry_size(r); i++) {
        if (strcmp(spec_registry_at(r, i)->name, "lua") == 0) {
            g_lua_idx = i;
            break;
        }
    }
    ASSERT(g_lua_idx >= 0);

    pool_t *p = pool_create(4);
    ASSERT(p);
    pool_set_worker_init(p, spec_pool_init_cb, r);
    ASSERT(pool_init_barrier(p) == 0);

    /* Spawn enough tasks that every worker sees one. observe_action
     * only records into the first MAX_OBS slots; iterate just
     * those. */
    atomic_store(&g_obs_count, 0);
    enum { N_TASKS = 64 };
    for (int i = 0; i < N_TASKS; i++) pool_spawn(p, observe_action, NULL, 0);
    pool_wait_quiescent(p);

    int n = atomic_load(&g_obs_count);
    ASSERT(n == N_TASKS);

    int observed = n < MAX_OBS ? n : MAX_OBS;

    /* Every observation must have a non-NULL Lua handle and a
     * thread_idx in range. */
    int seen_per_thread[POOL_MAX_WORKERS] = {0};
    for (int i = 0; i < observed; i++) {
        ASSERT(g_obs[i].thread_idx >= 0);
        ASSERT(g_obs[i].thread_idx < pool_n_workers(p));
        ASSERT(g_obs[i].lua_handle != NULL);
        if (g_obs[i].thread_idx < POOL_MAX_WORKERS)
            seen_per_thread[g_obs[i].thread_idx]++;
    }
    /* All four workers should have been observed at least once. */
    for (int i = 0; i < pool_n_workers(p); i++) ASSERT(seen_per_thread[i] > 0);

    pool_destroy(p);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ test_handles_distinct_per_worker() */
/* Each worker should get its own lua_State, not a shared one. */
static int test_handles_distinct_per_worker(void)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    g_lua_idx = -1;
    for (int i = 0; i < spec_registry_size(r); i++) {
        if (strcmp(spec_registry_at(r, i)->name, "lua") == 0) {
            g_lua_idx = i; break;
        }
    }

    pool_t *p = pool_create(4);
    pool_set_worker_init(p, spec_pool_init_cb, r);
    ASSERT(pool_init_barrier(p) == 0);

    atomic_store(&g_obs_count, 0);
    /* Burst enough tasks to cover every worker. */
    for (int i = 0; i < 200; i++) pool_spawn(p, observe_action, NULL, 0);
    pool_wait_quiescent(p);

    int n = atomic_load(&g_obs_count);
    ASSERT(n > 0);

    /* Collect distinct handles per thread_idx. */
    void *per_thread[POOL_MAX_WORKERS] = {0};
    for (int i = 0; i < n && i < MAX_OBS; i++) {
        int t = g_obs[i].thread_idx;
        if (t < 0 || t >= POOL_MAX_WORKERS) continue;
        if (per_thread[t] == NULL) {
            per_thread[t] = g_obs[i].lua_handle;
        } else {
            ASSERT(per_thread[t] == g_obs[i].lua_handle);
        }
    }
    /* Different workers have different handles. */
    for (int i = 0; i < pool_n_workers(p); i++) {
        if (!per_thread[i]) continue;
        for (int j = i + 1; j < pool_n_workers(p); j++) {
            if (!per_thread[j]) continue;
            ASSERT(per_thread[i] != per_thread[j]);
        }
    }

    pool_destroy(p);
    spec_registry_destroy(r);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    struct stat st;
    if (stat("langs/lua/spec.so", &st) != 0) {
        fprintf(stderr, "303-pool-spec-init-test: langs/lua/spec.so missing; "
                        "run from project root with `make` first\n");
        return 2;
    }
    printf("303-pool-spec-init-test:\n");
    RUN(handles_populated);
    RUN(handles_distinct_per_worker);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
