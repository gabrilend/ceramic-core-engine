/* tests/012-dispatch-test.c — dispatch layer tests, including
 * the first end-to-end run of a real map through the runtime.
 *
 * Setup that every test shares: graph_load → slot_store_create →
 * spec_registry_load → pool_create → pool_set_worker_init →
 * graph_attach_runtime → dispatch_ctx_init → pool_init_barrier.
 * The teardown is the reverse.
 */

#include "012-dispatch.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

/* {{{ spec_pool_init_cb() — wire spec registry into pool worker init */
static int spec_pool_init_cb(int worker_idx, worker_ctx_t *ctx, void *user)
{
    spec_registry_t *r = (spec_registry_t *)user;
    return spec_registry_init_worker(r, worker_idx, ctx->handles,
                                     POOL_LANG_SLOTS) >= 0 ? 0 : -1;
}
/* }}} */

/* {{{ Runtime — bundles every piece of phase 3 state */
typedef struct {
    graph_t         *graph;
    slot_store_t    *slots;
    spec_registry_t *specs;
    pool_t          *pool;
    dispatch_ctx_t   ctx;
} runtime_t;

static int runtime_setup(runtime_t *rt, const char *map_dir,
                         int n_workers, int capture_outputs)
{
    char *err = NULL;
    memset(rt, 0, sizeof *rt);

    rt->graph = graph_load(map_dir, &err);
    if (!rt->graph) {
        fprintf(stderr, "      graph_load: %s\n", err ? err : "(null)");
        free(err); return -1;
    }
    rt->slots = slot_store_create();
    rt->specs = spec_registry_load("langs", &err);
    if (!rt->specs) {
        fprintf(stderr, "      spec_registry_load: %s\n", err ? err : "(null)");
        free(err); return -1;
    }
    rt->pool = pool_create(n_workers);
    if (graph_attach_runtime(rt->graph, rt->slots, rt->specs, 4096, &err) != 0) {
        fprintf(stderr, "      graph_attach_runtime: %s\n", err ? err : "(null)");
        free(err); return -1;
    }
    if (dispatch_ctx_init(&rt->ctx, rt->graph, rt->slots, rt->specs,
                          rt->pool, capture_outputs, &err) != 0) {
        free(err); return -1;
    }
    pool_set_worker_init(rt->pool, spec_pool_init_cb, rt->specs);
    if (pool_init_barrier(rt->pool) != 0) return -1;
    return 0;
}

static void runtime_teardown(runtime_t *rt)
{
    if (rt->pool) pool_destroy(rt->pool);
    dispatch_ctx_destroy(&rt->ctx);
    if (rt->specs) spec_registry_destroy(rt->specs);
    if (rt->slots) slot_store_destroy(rt->slots);
    if (rt->graph) graph_destroy(rt->graph);
}
/* }}} */

/* {{{ test_dispatch_skeleton_run() */
/* Smoke-test the spawn → action → return loop end-to-end. The hello
 * fixture's `greet` call box is fed by the `who` read box; under
 * 244 the read box never dispatches but the consumer pulls its
 * cached `names.txt` bytes at attempt time. We spawn the consumer
 * directly and confirm the pool serviced it. */
static int test_dispatch_skeleton_run(void)
{
    runtime_t rt;
    if (runtime_setup(&rt, "tests/maps/hello", 2, 0) != 0) {
        runtime_teardown(&rt); return 0;
    }

    int greet = graph_box_index(rt.graph, "greet");
    ASSERT(greet >= 0);
    dispatch_spawn_if_ready(&rt.ctx, greet, 0);
    pool_wait_quiescent(rt.pool);
    ASSERT(atomic_load(&rt.ctx.tasks_dispatched) >= 1);

    runtime_teardown(&rt);
    return 1;
}
/* }}} */

/* {{{ test_end_to_end_calc() — the headline test */
/* Load the calc fixture (one Lua call box with two literal inputs),
 * push literals, spawn the entry box, wait for quiescence, verify
 * the captured output is "42" (17 + 25). */
static int test_end_to_end_calc(void)
{
    runtime_t rt;
    if (runtime_setup(&rt, "tests/maps/calc", 2, 1) != 0) {
        runtime_teardown(&rt); return 0;
    }

    char *err = NULL;
    ASSERT(dispatch_push_literals(&rt.ctx, &err) == 0);

    /* The entry box should now have all its inputs ready. */
    int entry = graph_box_index(rt.graph, "add");
    ASSERT(entry >= 0);
    dispatch_spawn_if_ready(&rt.ctx, entry, 0);

    pool_wait_quiescent(rt.pool);
    ASSERT(atomic_load(&rt.ctx.tasks_dispatched) == 1);

    /* Captured output for the add box must be "42". */
    ASSERT(rt.ctx.capture_outputs);
    ASSERT(dispatch_captured_output(&rt.ctx, entry, NULL) != NULL);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, entry, NULL), "42") == 0);

    runtime_teardown(&rt);
    return 1;
}
/* }}} */

/* {{{ test_calc_mul() — same fixture, different function */
/* Re-load the calc map, override one input via direct slot_push,
 * and call M.mul instead of M.add to exercise re-use. We just
 * load the calc map, but build a different result by writing
 * different literal values manually (skipping push_literals). */
static int test_calc_mul(void)
{
    runtime_t rt;
    if (runtime_setup(&rt, "tests/maps/calc", 2, 1) != 0) {
        runtime_teardown(&rt); return 0;
    }

    /* Push our own literal values: 6 and 7, expecting 42. */
    int entry = graph_box_index(rt.graph, "add");
    const box_t *b = graph_box(rt.graph, entry);
    ASSERT(b);
    ASSERT(b->n_inputs == 2);

    const char *six = "6", *seven = "7";
    /* slot_push_native aliases to slot_push for single-ring slots
     * and handles the ordering-ring write for dual-ring slots (issue
     * 312 slice 3). Calc's input ports are dual-ring (call box,
     * non-LARGE_VALUE, non-TAGGED) so we go through the dual-aware
     * variant. */
    ASSERT(slot_push_native(rt.slots, b->input_slot_ids[0], six,   1, 0) == 0);
    ASSERT(slot_push_native(rt.slots, b->input_slot_ids[1], seven, 1, 0) == 0);

    /* The function name is fixed in the box JSON ("add"); we just
     * verify add(6, 7) = "13". */
    dispatch_spawn_if_ready(&rt.ctx, entry, 0);
    pool_wait_quiescent(rt.pool);
    ASSERT(dispatch_captured_output(&rt.ctx, entry, NULL) != NULL);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, entry, NULL), "13") == 0);

    runtime_teardown(&rt);
    return 1;
}
/* }}} */

/* {{{ test_dispatch_counter_burst() */
static int test_dispatch_counter_burst(void)
{
    runtime_t rt;
    if (runtime_setup(&rt, "tests/maps/calc", 4, 1) != 0) {
        runtime_teardown(&rt); return 0;
    }
    int entry = graph_box_index(rt.graph, "add");
    char *err = NULL;
    ASSERT(dispatch_push_literals(&rt.ctx, &err) == 0);

    /* Force-spawn 8 add-box tasks (bypass the single-spawn guard
     * via dispatch_spawn). All read the same input slots
     * (1-cell peek), all compute 17 + 25 = 42. */
    enum { N = 8 };
    for (int i = 0; i < N; i++) dispatch_spawn(&rt.ctx, entry, 0);
    pool_wait_quiescent(rt.pool);
    ASSERT(atomic_load(&rt.ctx.tasks_dispatched) == N);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, entry, NULL), "42") == 0);

    runtime_teardown(&rt);
    return 1;
}
/* }}} */

/* {{{ test_comparator_routing() */
/* The comparator fixture's classify box returns its literal input
 * unchanged. With v="8" and comparand=5, dispatch should route
 * along the "gt" branch — only high fires, low and mid don't. */
static int test_comparator_routing(void)
{
    runtime_t rt;
    if (runtime_setup(&rt, "tests/maps/comparator", 2, 1) != 0) {
        runtime_teardown(&rt); return 0;
    }
    char *err = NULL;
    ASSERT(dispatch_push_literals(&rt.ctx, &err) == 0);

    int classify = graph_box_index(rt.graph, "classify");
    int low      = graph_box_index(rt.graph, "low");
    int mid      = graph_box_index(rt.graph, "mid");
    int high     = graph_box_index(rt.graph, "high");
    ASSERT(classify >= 0 && low >= 0 && mid >= 0 && high >= 0);

    dispatch_spawn_if_ready(&rt.ctx, classify, 0);
    pool_wait_quiescent(rt.pool);

    /* classify ran; only the "gt" branch (high) ran behind it. */
    ASSERT(dispatch_captured_output(&rt.ctx, classify, NULL) != NULL);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, classify, NULL), "8") == 0);
    ASSERT(dispatch_captured_output(&rt.ctx, high, NULL) != NULL);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, high, NULL), "HIGH:8") == 0);
    ASSERT(dispatch_captured_output(&rt.ctx, low, NULL) == NULL);
    ASSERT(dispatch_captured_output(&rt.ctx, mid, NULL) == NULL);

    runtime_teardown(&rt);
    return 1;
}
/* }}} */

/* {{{ test_iterator_routing_single_fire() */
/* First invocation of an iterator with counter=0 picks branch
 * out_0. Subsequent firings would advance to out_1 / out_2, but
 * this iteration's dispatch doesn't multi-spawn. */
static int test_iterator_routing_single_fire(void)
{
    runtime_t rt;
    if (runtime_setup(&rt, "tests/maps/iter-route", 2, 1) != 0) {
        runtime_teardown(&rt); return 0;
    }
    char *err = NULL;
    ASSERT(dispatch_push_literals(&rt.ctx, &err) == 0);

    int iter = graph_box_index(rt.graph, "iter");
    int a    = graph_box_index(rt.graph, "a");
    int b    = graph_box_index(rt.graph, "b");
    int c    = graph_box_index(rt.graph, "c");

    dispatch_spawn_if_ready(&rt.ctx, iter, 0);
    pool_wait_quiescent(rt.pool);

    /* counter=0 → out_0 → a. */
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, iter, NULL), "hi") == 0);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, a, NULL), "A:hi") == 0);
    ASSERT(dispatch_captured_output(&rt.ctx, b, NULL) == NULL);
    ASSERT(dispatch_captured_output(&rt.ctx, c, NULL) == NULL);

    runtime_teardown(&rt);
    return 1;
}
/* }}} */


/* {{{ test_iterator_multi_fire() */
/* The iterator's input is an N-cell POP slot. Push two extra
 * values (beyond the literal), then spawn the iterator once. The
 * dispatch action auto-re-spawns while POP inputs remain, so the
 * atomic counter walks 0 → 1 → 2 and each branch fires once. */
static int test_iterator_multi_fire(void)
{
    runtime_t rt;
    if (runtime_setup(&rt, "tests/maps/iter-route", 2, 1) != 0) {
        runtime_teardown(&rt); return 0;
    }
    char *err = NULL;
    ASSERT(dispatch_push_literals(&rt.ctx, &err) == 0);

    int iter = graph_box_index(rt.graph, "iter");
    int a    = graph_box_index(rt.graph, "a");
    int bx   = graph_box_index(rt.graph, "b");
    int c    = graph_box_index(rt.graph, "c");

    const box_t *iter_box = graph_box(rt.graph, iter);
    ASSERT(iter_box->multi_spawn == 1);
    ASSERT(iter_box->n_inputs == 1);

    /* Queue two more values on top of the literal — "hi", "hi2",
     * "hi3" — so the iterator has three to walk through. */
    ASSERT(slot_push(rt.slots, iter_box->input_slot_ids[0], "hi2", 3, 0) == 0);
    ASSERT(slot_push(rt.slots, iter_box->input_slot_ids[0], "hi3", 3, 0) == 0);

    /* One spawn — auto-re-spawn handles the rest. */
    dispatch_spawn(&rt.ctx, iter, 0);
    pool_wait_quiescent(rt.pool);

    ASSERT(dispatch_captured_output(&rt.ctx, a, NULL) != NULL);
    ASSERT(dispatch_captured_output(&rt.ctx, bx, NULL) != NULL);
    ASSERT(dispatch_captured_output(&rt.ctx, c, NULL) != NULL);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, a, NULL),  "A:hi")  == 0);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, bx, NULL), "B:hi2") == 0);
    ASSERT(strcmp(dispatch_captured_output(&rt.ctx, c, NULL),  "C:hi3") == 0);

    runtime_teardown(&rt);
    return 1;
}
/* }}} */

/* {{{ test_read_predecessor_rotation() — issue 244 */
/* Three read boxes feed the same consumer input port. Spawning the
 * consumer three times in a row should rotate through the three
 * cached values via the per-port atomic counter the loader
 * allocated. Serial spawns make the order deterministic — counter
 * 0 → r0, counter 1 → r1, counter 2 → r2.
 *
 * This is the end-to-end test 244 owed for the multi-predecessor
 * rotation path. The single-predecessor pull is already exercised
 * by the read-literal and hello fixtures. */
static int test_read_predecessor_rotation(void)
{
    char tmpl[] = "/tmp/soramech-rotation-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char abs[4096];
    ASSERT(realpath("tests/maps/calc/src/calc.lua", abs));

    char p[4096];
    snprintf(p, sizeof p, "%s/meta.json", dir);
    FILE *fp = fopen(p, "w");
    fputs("{\"name\":\"rot\",\"entry_box_id\":\"sink\"}", fp); fclose(fp);
    snprintf(p, sizeof p, "%s/boxes", dir); mkdir(p, 0755);

    for (int i = 0; i < 3; i++) {
        snprintf(p, sizeof p, "%s/boxes/r%d.json", dir, i);
        fp = fopen(p, "w");
        fprintf(fp,
            "{\"id\":\"r%d\",\"kind\":\"read\",\"value\":\"v%d\","
            "\"connections\":[{\"to_box\":\"sink\",\"to_input\":\"x\"}]}",
            i, i);
        fclose(fp);
    }
    snprintf(p, sizeof p, "%s/boxes/sink.json", dir);
    fp = fopen(p, "w");
    fprintf(fp,
        "{\"id\":\"sink\",\"kind\":\"call\",\"lang\":\"lua\","
        "\"ref\":\"%s\",\"fn\":\"identity\","
        "\"inputs\":[{\"name\":\"x\",\"type\":\"string\"}],"
        "\"routing\":{\"kind\":\"plain\"}}",
        abs);
    fclose(fp);

    runtime_t rt;
    int ok = (runtime_setup(&rt, dir, 1, 1) == 0);
    if (ok) {
        int sink = graph_box_index(rt.graph, "sink");
        ASSERT(sink >= 0);
        const box_t *sb = graph_box(rt.graph, sink);
        ASSERT(sb && sb->n_read_predecessors &&
               sb->n_read_predecessors[0] == 3 &&
               sb->read_pred_counter_slot[0] >= 0);

        /* Each k-th spawn should pull from the predecessor at
         * counter index k. Read directly from the loader's list so
         * the test is agnostic to whatever filesystem order
         * surfaced r0 / r1 / r2 in. */
        for (int k = 0; k < 3; k++) {
            int pred_idx = sb->read_predecessor_ids[0][k];
            const box_t *pred = graph_box(rt.graph, pred_idx);
            ASSERT(pred && pred->cached_value);

            dispatch_spawn(&rt.ctx, sink, 0);
            pool_wait_quiescent(rt.pool);
            ASSERT(dispatch_captured_output(&rt.ctx, sink, NULL) != NULL);
            ASSERT(strcmp(dispatch_captured_output(&rt.ctx, sink, NULL),
                          pred->cached_value) == 0);
        }
    }
    runtime_teardown(&rt);

    for (int i = 0; i < 3; i++) {
        snprintf(p, sizeof p, "%s/boxes/r%d.json", dir, i); unlink(p);
    }
    snprintf(p, sizeof p, "%s/boxes/sink.json", dir); unlink(p);
    snprintf(p, sizeof p, "%s/meta.json",       dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes",           dir); rmdir(p);
    rmdir(dir);
    return ok;
}
/* }}} */

/* {{{ test_dual_ring_per_cell_format() — issue 312
 *
 * The dual-ring slot's per-cell ring tag (NATIVE vs JSON) is set
 * by the producer at push time and consulted by the dispatch's
 * read_inputs at pop time to seed the consumer spec's
 * input_native[i] flag. This test exercises the flow with a
 * Lua consumer whose function visibly distinguishes what arrived:
 * M.describe returns "table:..." for a parsed table input and
 * "string:..." for a raw-bytes string input.
 *
 * Three cases since the bug-323 fix amended the native contract
 * (tables cross same-language wires as JSON, cell still tagged
 * native, consumer sniffs a leading '{' / '['):
 *   1. native tag + unstructured bytes  → raw string, untouched
 *   2. json tag                          → parsed, as always
 *   3. native tag + structured bytes    → parsed (the 323 sniff)
 *
 * Each case uses a fresh fixture: dual-ring slots are single-cell
 * (slice 1 deferred DUAL_RING + multi-cell combining), so we
 * can't push two cells to the same slot in one run.  */
static int build_describe_fixture(char *dir, size_t cap)
{
    char tmpl[] = "/tmp/soramech-dualring-XXXXXX";
    char *got = mkdtemp(tmpl);
    if (!got) return -1;
    snprintf(dir, cap, "%s", got);

    char abs[4096];
    if (!realpath("tests/maps/calc/src/calc.lua", abs)) return -1;

    char p[4096];
    snprintf(p, sizeof p, "%s/meta.json", dir);
    FILE *fp = fopen(p, "w");
    fputs("{\"name\":\"dr\",\"entry_box_id\":\"sink\"}", fp); fclose(fp);
    snprintf(p, sizeof p, "%s/boxes", dir); mkdir(p, 0755);

    snprintf(p, sizeof p, "%s/boxes/sink.json", dir);
    fp = fopen(p, "w");
    fprintf(fp,
        "{\"id\":\"sink\",\"kind\":\"call\",\"lang\":\"lua\","
        "\"ref\":\"%s\",\"fn\":\"describe\","
        "\"inputs\":[{\"name\":\"x\",\"type\":\"string\"}],"
        "\"routing\":{\"kind\":\"plain\"}}",
        abs);
    fclose(fp);
    return 0;
}

static void cleanup_describe_fixture(const char *dir)
{
    char p[4096];
    snprintf(p, sizeof p, "%s/boxes/sink.json", dir); unlink(p);
    snprintf(p, sizeof p, "%s/meta.json",       dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes",           dir); rmdir(p);
    rmdir(dir);
}

static int test_dual_ring_per_cell_format(void)
{
    const char *bytes  = "{\"a\":42}";
    int         n      = (int)strlen(bytes);
    int ok = 1;

    /* Case 1: native push of an unstructured payload → Lua sees the
     * raw string. The raw-fidelity claim is made with bytes that
     * don't look like JSON; structured-looking native bytes are
     * case 3's territory since the 323 fix. */
    {
        char dir[1024];
        ASSERT(build_describe_fixture(dir, sizeof dir) == 0);
        runtime_t rt;
        ASSERT(runtime_setup(&rt, dir, 2, 1) == 0);
        int sink = graph_box_index(rt.graph, "sink");
        ASSERT(sink >= 0);
        const box_t *sb = graph_box(rt.graph, sink);
        const char *plain = "plain-42";
        ASSERT(slot_push_native(rt.slots, sb->input_slot_ids[0],
                                plain, (int)strlen(plain), 0) == 0);
        dispatch_spawn_if_ready(&rt.ctx, sink, 0);
        pool_wait_quiescent(rt.pool);
        ASSERT(dispatch_captured_output(&rt.ctx, sink, NULL) != NULL);
        if (strncmp(dispatch_captured_output(&rt.ctx, sink, NULL), "string:", 7) != 0) {
            fprintf(stderr, "      native half got: %s\n",
                    dispatch_captured_output(&rt.ctx, sink, NULL));
            ok = 0;
        }
        runtime_teardown(&rt);
        cleanup_describe_fixture(dir);
    }

    /* Half 2: JSON push → Lua parses → "table:42" */
    {
        char dir[1024];
        ASSERT(build_describe_fixture(dir, sizeof dir) == 0);
        runtime_t rt;
        ASSERT(runtime_setup(&rt, dir, 2, 1) == 0);
        int sink = graph_box_index(rt.graph, "sink");
        ASSERT(sink >= 0);
        const box_t *sb = graph_box(rt.graph, sink);
        ASSERT(slot_push_json(rt.slots, sb->input_slot_ids[0],
                              bytes, n, 0) == 0);
        dispatch_spawn_if_ready(&rt.ctx, sink, 0);
        pool_wait_quiescent(rt.pool);
        ASSERT(dispatch_captured_output(&rt.ctx, sink, NULL) != NULL);
        if (strcmp(dispatch_captured_output(&rt.ctx, sink, NULL), "table:42") != 0) {
            fprintf(stderr, "      json half got: %s\n",
                    dispatch_captured_output(&rt.ctx, sink, NULL));
            ok = 0;
        }
        runtime_teardown(&rt);
        cleanup_describe_fixture(dir);
    }

    /* Case 3: native push of structured-looking bytes → parsed
     * anyway. Bug 323's floor fix: tables cross same-language wires
     * as JSON with the cell still tagged native, so the Lua input
     * side sniffs the leading brace and rebuilds the value. Pinned
     * here at the unit level; tests/maps/323-table-fast-path covers
     * the same contract end-to-end. */
    {
        char dir[1024];
        ASSERT(build_describe_fixture(dir, sizeof dir) == 0);
        runtime_t rt;
        ASSERT(runtime_setup(&rt, dir, 2, 1) == 0);
        int sink = graph_box_index(rt.graph, "sink");
        ASSERT(sink >= 0);
        const box_t *sb = graph_box(rt.graph, sink);
        ASSERT(slot_push_native(rt.slots, sb->input_slot_ids[0],
                                bytes, n, 0) == 0);
        dispatch_spawn_if_ready(&rt.ctx, sink, 0);
        pool_wait_quiescent(rt.pool);
        ASSERT(dispatch_captured_output(&rt.ctx, sink, NULL) != NULL);
        if (strcmp(dispatch_captured_output(&rt.ctx, sink, NULL), "table:42") != 0) {
            fprintf(stderr, "      structured-native half got: %s\n",
                    dispatch_captured_output(&rt.ctx, sink, NULL));
            ok = 0;
        }
        runtime_teardown(&rt);
        cleanup_describe_fixture(dir);
    }

    return ok;
}
/* }}} */

/* {{{ test_distributor_picks_least_full() — issue 304 */
/* Build a temp map: one distributor `dist` (n_outputs=2) wired to
 * two echo sinks `s0` / `s1`. Pre-fill `s1`'s input slot so its
 * fill count is 1; leave `s0` empty. Push the distributor's input
 * and spawn it. The distributor's argmin-over-fill picker should
 * route to `s0` (less loaded). Only `s0` captures an output —
 * `s1` was never spawn-triggered.
 *
 * Counter-rotated tie-breaking is also exercised here implicitly:
 * if the picker had a bias toward branch 0 the test would pass
 * spuriously, so the inverse case (s0 pre-filled, expect s1) is
 * the actual proof. We run both. */
static int build_distributor_fixture(char *dir, size_t cap)
{
    char tmpl[] = "/tmp/soramech-distributor-XXXXXX";
    char *got = mkdtemp(tmpl);
    if (!got) return -1;
    snprintf(dir, cap, "%s", got);

    /* Reach back at the project root's calc.lua via an absolute
     * path. Keeps the temp fixture self-sufficient without copying
     * a Lua file. resolve_path leaves absolute paths untouched. */
    char abs[4096];
    if (!realpath("tests/maps/calc/src/calc.lua", abs)) return -1;

    char p[4096];
    snprintf(p, sizeof p, "%s/meta.json", dir);
    FILE *fp = fopen(p, "w");
    if (!fp) return -1;
    fputs("{\"name\":\"dist\",\"entry_box_id\":\"dist\"}", fp);
    fclose(fp);

    snprintf(p, sizeof p, "%s/boxes", dir);
    mkdir(p, 0755);

    snprintf(p, sizeof p, "%s/boxes/dist.json", dir);
    fp = fopen(p, "w");
    fprintf(fp,
        "{\"id\":\"dist\",\"kind\":\"call\",\"lang\":\"lua\","
        "\"ref\":\"%s\",\"fn\":\"add\","
        "\"inputs\":["
            "{\"name\":\"a\",\"type\":\"string\",\"value\":\"1\"},"
            "{\"name\":\"b\",\"type\":\"string\",\"value\":\"0\"}"
        "],"
        "\"routing\":{\"kind\":\"distributor\",\"n_outputs\":2},"
        "\"connections\":["
            "{\"from_box\":\"dist\",\"from_branch\":\"out_0\","
              "\"to_box\":\"s0\",\"to_input\":\"x\"},"
            "{\"from_box\":\"dist\",\"from_branch\":\"out_1\","
              "\"to_box\":\"s1\",\"to_input\":\"x\"}"
        "]}", abs);
    fclose(fp);

    for (int i = 0; i < 2; i++) {
        snprintf(p, sizeof p, "%s/boxes/s%d.json", dir, i);
        fp = fopen(p, "w");
        fprintf(fp,
            "{\"id\":\"s%d\",\"kind\":\"call\",\"lang\":\"lua\","
            "\"ref\":\"%s\",\"fn\":\"identity\","
            "\"inputs\":[{\"name\":\"x\",\"type\":\"string\"}],"
            "\"routing\":{\"kind\":\"plain\"}}",
            i, abs);
        fclose(fp);
    }
    return 0;
}

static void cleanup_distributor_fixture(const char *dir)
{
    char p[4096];
    snprintf(p, sizeof p, "%s/boxes/dist.json", dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes/s0.json",   dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes/s1.json",   dir); unlink(p);
    snprintf(p, sizeof p, "%s/meta.json",       dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes",           dir); rmdir(p);
    rmdir(dir);
}

static int distributor_case(const char *dir, int pre_fill_branch,
                            int expect_picked)
{
    runtime_t rt;
    if (runtime_setup(&rt, dir, 2, 1) != 0) {
        runtime_teardown(&rt); return 0;
    }
    char *err = NULL;
    ASSERT(dispatch_push_literals(&rt.ctx, &err) == 0);

    int dist = graph_box_index(rt.graph, "dist");
    int s0   = graph_box_index(rt.graph, "s0");
    int s1   = graph_box_index(rt.graph, "s1");
    int targets[2] = { s0, s1 };
    ASSERT(dist >= 0 && s0 >= 0 && s1 >= 0);

    const box_t *busy = graph_box(rt.graph, targets[pre_fill_branch]);
    ASSERT(busy && busy->input_slot_ids);
    /* Inflate the chosen sink's slot fill so the distributor sees
     * it as "loaded" without ever spawn-triggering it. */
    ASSERT(slot_push(rt.slots, busy->input_slot_ids[0], "x", 1, 0) == 0);

    dispatch_spawn_if_ready(&rt.ctx, dist, 0);
    pool_wait_quiescent(rt.pool);

    int picked    = targets[expect_picked];
    int rejected  = targets[1 - expect_picked];
    ASSERT(dispatch_captured_output(&rt.ctx, picked, NULL) != NULL);
    ASSERT(dispatch_captured_output(&rt.ctx, rejected, NULL) == NULL);

    runtime_teardown(&rt);
    return 1;
}

static int test_distributor_picks_least_full(void)
{
    char dir[1024];
    ASSERT(build_distributor_fixture(dir, sizeof dir) == 0);

    /* s1 pre-filled → s0 is less loaded → distributor picks s0. */
    int ok_a = distributor_case(dir, /*pre_fill=*/1, /*expect=*/0);
    /* s0 pre-filled → distributor picks s1. */
    int ok_b = distributor_case(dir, /*pre_fill=*/0, /*expect=*/1);

    cleanup_distributor_fixture(dir);
    ASSERT(ok_a && ok_b);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    struct stat st;
    if (stat("tests/maps/calc/meta.json", &st) != 0 ||
        stat("langs/lua/spec.so", &st) != 0) {
        fprintf(stderr, "012-dispatch-test: fixtures or specs not built; "
                        "run from project root with `make` first\n");
        return 2;
    }
    printf("012-dispatch-test:\n");
    RUN(dispatch_skeleton_run);
    RUN(end_to_end_calc);
    RUN(calc_mul);
    RUN(dispatch_counter_burst);
    RUN(comparator_routing);
    RUN(iterator_routing_single_fire);
    RUN(iterator_multi_fire);
    RUN(read_predecessor_rotation);
    RUN(dual_ring_per_cell_format);
    RUN(distributor_picks_least_full);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
