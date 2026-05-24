/* src/008-pool-runner.c — phase 3 runner entry point.
 *
 * What it is, in a sentence: the C binary that loads a map
 * directory, stands up the slot store + spec registry + thread
 * pool, runs the dispatch loop to quiescence, and prints what
 * each box produced.
 *
 * As of this iteration the runner actually runs maps end-to-end:
 *
 *   1. Load and validate the graph (305).
 *   2. Create the slot store (302) and spec registry (303).
 *   3. Create the thread pool (301) and register a per-worker
 *      init callback that populates each worker's handles[]
 *      with the spec registry (303 pool hook).
 *   4. Attach runtime — allocate one slot per input port and
 *      resolve each call box's spec (305 phase 5+7).
 *   5. Initialize the dispatch context with output capture.
 *   6. pool_init_barrier — releases workers from their init
 *      stage into the task loop.
 *   7. Push literal input values to slots, then spawn every box
 *      that's ready (every input either literal or optional).
 *      Wired-input boxes spawn automatically when an upstream
 *      push triggers spawn-on-ready.
 *   8. pool_wait_quiescent.
 *   9. Print each box's captured output, tear down.
 *
 * The JSONL run log (issue 311) and the dispatch action's
 * iterator multi-spawn path (issue 304) are still ahead.
 */

#include "010-graph-loader.h"
#include "009-slot-store.h"
#include "011-spec-registry.h"
#include "012-dispatch.h"
#include "014-event-queue.h"
#include "016-unified-allocator.h"
#include "pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* {{{ usage() */
static void usage(const char *progname)
{
    fprintf(stderr,
        "usage: %s <map-directory>\n"
        "\n"
        "Runs a SoraMech map. The map directory must contain meta.json\n"
        "and a boxes/ subdirectory.\n",
        progname);
}
/* }}} */

/* {{{ Pool init / teardown callbacks
 *
 * The init callback runs `spec_registry_init_worker_filtered`
 * with the map's language list (issue 305) so workers only spin
 * up specs the map actually uses — a Lua-only map skips the bash
 * subprocess fork on every worker, etc. The teardown is the
 * symmetric release so subprocess specs shut down cleanly when
 * the pool destroys workers. */
struct spec_init_args {
    spec_registry_t *registry;
    const char     **languages;   /* from graph_n_languages / graph_language */
    int              n_languages;
};

static int spec_pool_init_cb(int worker_idx, worker_ctx_t *ctx, void *user)
{
    struct spec_init_args *a = (struct spec_init_args *)user;
    return spec_registry_init_worker_filtered(
        a->registry, worker_idx, ctx->handles, POOL_LANG_SLOTS,
        a->languages, a->n_languages) >= 0 ? 0 : -1;
}

static void spec_pool_teardown_cb(int worker_idx, worker_ctx_t *ctx, void *user)
{
    (void)worker_idx;
    struct spec_init_args *a = (struct spec_init_args *)user;
    spec_registry_teardown_worker(a->registry, ctx->handles,
                                  spec_registry_size(a->registry));
}
/* }}} */

/* {{{ find_langs_dir() — locate the langs/ directory at runtime
 *
 * Precedence: SORAMECH_LANGS_DIR env var > sibling of the
 * binary (via /proc/self/exe) > "langs" relative to cwd.
 * `buf` is a scratch buffer the resolved path may live in. */
static const char *find_langs_dir(char *buf, size_t cap)
{
    const char *env = getenv("SORAMECH_LANGS_DIR");
    if (env && *env) return env;

    /* Leave plenty of headroom for the "/langs" suffix so the gcc
     * fortify warning about snprintf overflow doesn't fire. */
    char exe[2048];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0 && n < (ssize_t)(sizeof exe - 1)) {
        exe[n] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash) {
            *slash = '\0';
            int w = snprintf(buf, cap, "%s/langs", exe);
            if (w > 0 && (size_t)w < cap) {
                struct stat st;
                if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode)) return buf;
            }
        }
    }
    return "langs";
}
/* }}} */

/* {{{ now_secs() */
static double now_secs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* }}} */

/* {{{ open_event_queue() — try map_dir/tmp/last-run.jsonl */
/* Falls back to /tmp/soramech-last-run.jsonl when there's no tmp/
 * subdir in the map. Returns NULL silently if even the fallback
 * can't be opened — the runner works without a run log. */
static event_queue_t *open_event_queue(const char *map_dir,
                                       char *path_out, size_t cap)
{
    snprintf(path_out, cap, "%s/tmp/last-run.jsonl", map_dir);
    char tmpdir[4096];
    snprintf(tmpdir, sizeof tmpdir, "%s/tmp", map_dir);
    struct stat st;
    if (stat(tmpdir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        snprintf(path_out, cap, "/tmp/soramech-last-run.jsonl");
    }
    return event_queue_create(path_out);
}
/* }}} */

/* {{{ print_outputs() */
/* Reports each task-producing box's captured output. Read boxes are
 * not tasks under 244 — they have no output to capture and are
 * skipped from this report entirely; their cached values reach the
 * graph via consumer pulls, observed in the consumers' outputs. */
static void print_outputs(const dispatch_ctx_t *ctx)
{
    if (!ctx->last_outputs) return;
    /* Iterate over the graph's current box count rather than
     * ctx->n_boxes — the latter includes 319d's runtime-headroom
     * slots for boxes that may have been created at runtime; the
     * graph's atomic count reflects the actual population. Skip
     * any NULL entries defensively in case of races. */
    int n = graph_n_boxes(ctx->graph);
    for (int i = 0; i < n; i++) {
        const box_t *b = graph_box(ctx->graph, i);
        if (!b) continue;
        if (b->kind == BOX_READ) continue;
        const char *out = ctx->last_outputs[i];
        if (out) {
            fprintf(stderr, "  %s → %s\n", b->id, out);
        } else {
            fprintf(stderr, "  %s → (no output)\n", b->id);
        }
    }
}
/* }}} */

/* {{{ main() */
int main(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(argv[0]);
        return (argc < 2) ? 2 : 0;
    }
    const char *map_dir = argv[1];
    char *err = NULL;
    int   rc  = 1;

    graph_t         *g       = NULL;
    slot_store_t    *slots   = NULL;
    spec_registry_t *specs   = NULL;
    pool_t          *pool    = NULL;
    event_queue_t   *events  = NULL;
    dispatch_ctx_t   ctx;
    int              ctx_initialised = 0;
    char             jsonl_path[4096] = {0};

    struct timespec  ts_start;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    long start_us = (long)ts_start.tv_sec * 1000000L + ts_start.tv_nsec / 1000;

    /* 1. Load the graph. */
    g = graph_load(map_dir, &err);
    if (!g) {
        fprintf(stderr, "soramech-pool: %s\n", err ? err : "load failed");
        goto cleanup;
    }

    /* 2. Slot store and spec registry. */
    slots = slot_store_create();
    if (!slots) {
        fprintf(stderr, "soramech-pool: slot_store_create failed\n");
        goto cleanup;
    }
    char langs_buf[4096];
    const char *langs_dir = find_langs_dir(langs_buf, sizeof langs_buf);
    specs = spec_registry_load(langs_dir, &err);
    if (!specs) {
        fprintf(stderr, "soramech-pool: %s\n", err ? err : "spec load failed");
        goto cleanup;
    }

    /* 3. Pool. */
    pool = pool_create(0);   /* default n_workers */
    if (!pool) {
        fprintf(stderr, "soramech-pool: pool_create failed\n");
        goto cleanup;
    }
    /* spec_init_args bundles the registry + the map's language
     * list. We populate it after graph_attach_runtime so the
     * language enumeration is in place. */
    struct spec_init_args sia = { specs, NULL, 0 };
    pool_set_worker_init    (pool, spec_pool_init_cb,     &sia);
    pool_set_worker_teardown(pool, spec_pool_teardown_cb, &sia);

    /* 4. Attach runtime (allocate slots, resolve specs, enumerate
     * languages). After this, sia.languages can be filled in. */
    if (graph_attach_runtime(g, slots, specs, 4096, &err) != 0) {
        fprintf(stderr, "soramech-pool: %s\n", err ? err : "attach runtime failed");
        goto cleanup;
    }

    /* Build the language list for the spec init filter. We need
     * a flat array of const char* — copy each language pointer
     * into a small stack buffer. The graph owns the strings. */
    const char *lang_ptrs[POOL_LANG_SLOTS];
    int n_langs = graph_n_languages(g);
    if (n_langs > POOL_LANG_SLOTS) n_langs = POOL_LANG_SLOTS;
    for (int i = 0; i < n_langs; i++) lang_ptrs[i] = graph_language(g, i);
    sia.languages   = lang_ptrs;
    sia.n_languages = n_langs;

    /* 5. Dispatch context with output capture. */
    if (dispatch_ctx_init(&ctx, g, slots, specs, pool, 1, &err) != 0) {
        fprintf(stderr, "soramech-pool: %s\n", err ? err : "dispatch_ctx_init failed");
        goto cleanup;
    }
    ctx_initialised = 1;

    /* 6. Barrier — workers populate handles, then enter the task loop. */
    if (pool_init_barrier(pool) != 0) {
        fprintf(stderr, "soramech-pool: worker init failed\n");
        goto cleanup;
    }

    /* 6b. Open the run-log event queue and emit run_start. */
    events = open_event_queue(map_dir, jsonl_path, sizeof jsonl_path);
    if (events) {
        ctx.events = events;
        /* Verbose logging gates are opt-in per the architecture doc;
         * off by default to keep the log small even for iteration-
         * heavy maps. */
        const char *lv = getenv("SORAMECH_LOG_VALUES");
        ctx.log_values = (lv && lv[0] && lv[0] != '0') ? 1 : 0;

        event_queue_run_start(events, now_secs(),
                              graph_name(g), pool_n_workers(pool));

        /* Slot allocator events — one per input slot, plus one per
         * iterator counter slot, all fired right after attach so
         * the run log records the full slot layout up front. */
        const char *ls = getenv("SORAMECH_LOG_SLOTS");
        if (ls && ls[0] && ls[0] != '0') {
            double ts = now_secs();
            for (int i = 0; i < graph_n_boxes(g); i++) {
                const box_t *b = graph_box(g, i);
                if (b->input_slot_ids) {
                    int n_cells = b->multi_spawn ? 16 : 1;
                    for (int j = 0; j < b->n_inputs; j++) {
                        event_queue_slot_alloc(events, ts,
                            b->input_slot_ids[j], 4096, n_cells,
                            b->id, b->inputs[j].name);
                    }
                }
                if (b->counter_slot_id >= 0) {
                    event_queue_slot_alloc(events, ts,
                        b->counter_slot_id, 4, 1, b->id, "<counter>");
                }
            }
        }
    }

    fprintf(stderr,
        "soramech-pool: '%s' — %d box(es), entry %s, %d worker(s)\n",
        graph_name(g), graph_n_boxes(g),
        graph_entry_box_id(g) ? graph_entry_box_id(g) : "(none)",
        pool_n_workers(pool));
    if (events) {
        fprintf(stderr, "soramech-pool: run log → %s\n", jsonl_path);
    }

    /* 7. Push literals, then walk every box and spawn whichever
     *    are ready. Wired-input consumers will get auto-spawned by
     *    the dispatch action's spawn-on-input-ready check. */
    if (dispatch_push_literals(&ctx, &err) != 0) {
        fprintf(stderr, "soramech-pool: %s\n", err ? err : "push literals failed");
        goto cleanup;
    }
    for (int i = 0; i < graph_n_boxes(g); i++) {
        dispatch_spawn_if_ready(&ctx, i, 0);
    }

    /* 8. Quiesce. */
    pool_wait_quiescent(pool);

    /* Quiescence-trigger sweep (issue 302). The pool has no tasks
     * in flight at this point, so it's the natural moment for the
     * allocator's deep sweep to fuse free-chunk runs the cheap
     * eager-merge couldn't catch — there's no producer or consumer
     * mid-read to invalidate. Today the runner has a single drain
     * so the only effect is on shutdown statistics; future shapes
     * that drain and respawn (long iterators, batched submissions)
     * benefit between batches without further wiring. */
    ua_sweep(slot_store_allocator(slots));

    struct timespec ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    long end_us = (long)ts_end.tv_sec * 1000000L + ts_end.tv_nsec / 1000;
    if (events) {
        event_queue_run_end(events, now_secs(),
                            end_us - start_us, ctx.tasks_dispatched);
    }

    /* 9. Report. */
    fprintf(stderr, "soramech-pool: run complete (%d task(s) in %ld µs).\n",
            ctx.tasks_dispatched, end_us - start_us);
    fprintf(stderr, "soramech-pool: outputs:\n");
    print_outputs(&ctx);

    rc = 0;

cleanup:
    free(err);
    if (events) event_queue_destroy(events);    /* drains + joins writer */
    if (ctx_initialised) dispatch_ctx_destroy(&ctx);
    if (pool)  pool_destroy(pool);
    if (specs) spec_registry_destroy(specs);
    if (slots) slot_store_destroy(slots);
    if (g)     graph_destroy(g);
    return rc;
}
/* }}} */
