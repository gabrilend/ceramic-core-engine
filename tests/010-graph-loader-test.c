/* tests/010-graph-loader-test.c — unit tests for the graph loader.
 *
 * Exercises the phase 1 + phase 2 + early phase 3 work shipped in
 * this iteration: directory walk, JSON parse, per-box schema
 * validation. Topology / cycle / language-enumeration tests land
 * with their iterations.
 *
 * Fixtures live under tests/maps/. Tests cd to the project root so
 * relative paths work regardless of where the test binary is run.
 */

#include "010-graph-loader.h"
#include "009-slot-store.h"
#include "011-spec-registry.h"

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

/* {{{ Path resolution — find tests/maps/ relative to cwd */
static const char *FIXTURE_DIR = "tests/maps";

/* If tests are run from project root: tests/maps/ exists.
 * If run from build/tests/: the binary was started by make test,
 * which cd's to project root first. We don't try to be clever here. */
static int has_fixtures(void)
{
    struct stat st;
    return stat(FIXTURE_DIR, &st) == 0 && S_ISDIR(st.st_mode);
}
/* }}} */

/* {{{ test_load_hello() */
static int test_load_hello(void)
{
    char *err = NULL;
    graph_t *g = graph_load("tests/maps/hello", &err);
    if (!g) {
        fprintf(stderr, "      graph_load failed: %s\n", err ? err : "(null)");
        free(err);
        return 0;
    }

    ASSERT(strcmp(graph_name(g), "hello") == 0);
    ASSERT(strcmp(graph_entry_box_id(g), "greet") == 0);
    ASSERT(graph_description(g) != NULL);
    ASSERT(graph_n_boxes(g) == 2);

    /* greet box */
    const box_t *greet = graph_box_by_id(g, "greet");
    ASSERT(greet != NULL);
    ASSERT(greet->kind == BOX_CALL);
    ASSERT(strcmp(greet->lang, "lua") == 0);
    ASSERT(strcmp(greet->ref,  "src/hello.lua") == 0);
    ASSERT(strcmp(greet->fn,   "greet") == 0);
    ASSERT(greet->routing.kind == ROUTING_PLAIN);
    ASSERT(greet->output_capacity == 256);
    ASSERT(greet->n_inputs == 2);
    ASSERT(strcmp(greet->inputs[0].name, "name") == 0);
    ASSERT(greet->inputs[0].literal == NULL);
    ASSERT(greet->inputs[0].optional == 0);
    ASSERT(strcmp(greet->inputs[1].name, "salutation") == 0);
    ASSERT(strcmp(greet->inputs[1].literal, "Hello") == 0);
    ASSERT(greet->inputs[1].optional == 1);

    /* who box */
    const box_t *who = graph_box_by_id(g, "who");
    ASSERT(who != NULL);
    ASSERT(who->kind == BOX_READ);
    ASSERT(strcmp(who->path, "names.txt") == 0);
    ASSERT(who->n_connections == 1);
    ASSERT(strcmp(who->connections[0].to_box,   "greet") == 0);
    ASSERT(strcmp(who->connections[0].to_input, "name")  == 0);
    ASSERT(who->connections[0].from_branch == NULL);

    /* Topology resolution: who's connection resolves to greet's
     * `name` input. greet is index 0 or 1; name is greet.inputs[0]. */
    int greet_idx = -1;
    for (int i = 0; i < graph_n_boxes(g); i++) {
        if (strcmp(graph_box(g, i)->id, "greet") == 0) { greet_idx = i; break; }
    }
    ASSERT(greet_idx >= 0);
    ASSERT(who->connections[0].to_box_idx == greet_idx);
    ASSERT(who->connections[0].to_input_idx == 0);

    graph_destroy(g);
    return 1;
}
/* }}} */

/* {{{ test_native_invoke_classification() */
/* Verifies issue 312's per-edge fast-path classification:
 *
 *  - input_edge_native[port]  is 1 iff every producer feeding
 *    that port is a call box in the consumer's language.
 *    Literals and data-box producers count as NOT native — they
 *    emit JSON text the consumer's spec must parse.
 *  - output_edge_native[edge] is 1 iff the consumer at the other
 *    end of that connection is a call box in this box's language.
 *  - use_native_invoke remains as the per-box AND of every edge,
 *    used by dispatch sites not yet migrated to per-edge. */
static int test_native_invoke_classification(void)
{
    char *err = NULL;

    /* hello: greet (lua) has a literal-fed input → input edge is
     * JSON. greet has no outgoing connections in this fixture.
     * Per-edge: input 0 not native; vacuous all-outs. Per-box AND
     * = 0 because the input is a JSON edge. */
    {
        graph_t *g = graph_load("tests/maps/hello", &err);
        ASSERT(g);
        const box_t *greet = graph_box_by_id(g, "greet");
        const box_t *who   = graph_box_by_id(g, "who");
        (void)who;
        ASSERT(greet->n_inputs == 2);
        ASSERT(greet->input_edge_native != NULL);
        ASSERT(greet->input_edge_native[0] == 0);  /* fed by read box `who` */
        ASSERT(greet->input_edge_native[1] == 0);  /* literal */
        /* Slice 5 of issue 312 removed the per-box use_native_invoke
         * field. Per-edge bits above carry the same information with
         * better resolution. */
        graph_destroy(g);
    }

    /* comparator: classify (lua) has a literal input but its
     * three outgoing edges all feed lua call boxes → outputs are
     * native, input is JSON. low/mid/high have no outgoing
     * connections and their input comes from classify (a Lua call
     * box) → input native, vacuous outs, all-native = use_native. */
    {
        graph_t *g = graph_load("tests/maps/comparator", &err);
        ASSERT(g);
        const box_t *classify = graph_box_by_id(g, "classify");
        const box_t *low      = graph_box_by_id(g, "low");

        ASSERT(classify->input_edge_native[0]  == 0);  /* literal */
        ASSERT(classify->n_connections         == 3);
        ASSERT(classify->output_edge_native[0] == 1);  /* lt → low (lua) */
        ASSERT(classify->output_edge_native[1] == 1);  /* eq → mid (lua) */
        ASSERT(classify->output_edge_native[2] == 1);  /* gt → high (lua) */

        /* low has two inputs: port 0 "tag" (literal) and port 1
         * "v" (fed by classify, a Lua call box). */
        ASSERT(low->n_inputs               == 2);
        ASSERT(low->input_edge_native[0]   == 0);   /* literal "tag"      */
        ASSERT(low->input_edge_native[1]   == 1);   /* lua classify → "v" */
        graph_destroy(g);
    }

    /* pipeline: lua → c → bash chain. Every adjacent edge crosses
     * a language boundary; per-edge bits are 0 across the board. */
    {
        graph_t *g = graph_load("tests/maps/pipeline", &err);
        ASSERT(g);
        const box_t *dbl = graph_box_by_id(g, "double");  /* lua */
        ASSERT(dbl->n_connections >= 1);
        ASSERT(dbl->output_edge_native[0] == 0);          /* → addone (c) */
        graph_destroy(g);
    }
    return 1;
}
/* }}} */

/* {{{ test_randomizer_weighted_parsing() */
/* Verifies issue 304's randomizer and weighted routing schemas
 * load cleanly with the right per-kind fields. */
static int test_randomizer_weighted_parsing(void)
{
    char tmpl[] = "/tmp/soramech-routing-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char path[4096];
    snprintf(path, sizeof path, "%s/meta.json", dir);
    FILE *fp = fopen(path, "w");
    fputs("{\"name\":\"r\",\"entry_box_id\":\"rnd\"}", fp);
    fclose(fp);
    snprintf(path, sizeof path, "%s/boxes", dir); mkdir(path, 0755);

    snprintf(path, sizeof path, "%s/boxes/rnd.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"rnd\",\"kind\":\"call\",\"lang\":\"lua\","
          "\"ref\":\"r.lua\",\"fn\":\"r\","
          "\"routing\":{\"kind\":\"randomizer\",\"n_outputs\":4}}", fp);
    fclose(fp);

    snprintf(path, sizeof path, "%s/boxes/wtd.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"wtd\",\"kind\":\"call\",\"lang\":\"lua\","
          "\"ref\":\"r.lua\",\"fn\":\"w\","
          "\"routing\":{\"kind\":\"weighted\",\"weights\":[0.8,0.2]}}", fp);
    fclose(fp);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    if (!g) {
        fprintf(stderr, "      load failed: %s\n", err ? err : "(null)");
        free(err);
        return 0;
    }
    const box_t *rnd = graph_box_by_id(g, "rnd");
    const box_t *wtd = graph_box_by_id(g, "wtd");
    ASSERT(rnd->routing.kind      == ROUTING_RANDOMIZER);
    ASSERT(rnd->routing.n_outputs == 4);
    ASSERT(wtd->routing.kind      == ROUTING_WEIGHTED);
    ASSERT(wtd->routing.n_outputs == 2);
    ASSERT(wtd->routing.weights   != NULL);
    ASSERT(wtd->routing.weights[0] == 0.8);
    ASSERT(wtd->routing.weights[1] == 0.2);
    graph_destroy(g);

    /* Cleanup. */
    snprintf(path, sizeof path, "%s/boxes/rnd.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes/wtd.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/meta.json",     dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes",         dir); rmdir(path);
    rmdir(dir);
    return 1;
}
/* }}} */

/* {{{ test_load_branching() — comparator + iterator routing kinds */
static int test_load_branching(void)
{
    char *err = NULL;
    graph_t *g = graph_load("tests/maps/branching", &err);
    if (!g) {
        fprintf(stderr, "      graph_load failed: %s\n", err ? err : "(null)");
        free(err);
        return 0;
    }
    ASSERT(graph_n_boxes(g) == 5);

    const box_t *src = graph_box_by_id(g, "source");
    ASSERT(src != NULL);
    ASSERT(src->routing.kind == ROUTING_ITERATOR);
    ASSERT(src->routing.n_outputs == 3);

    const box_t *cls = graph_box_by_id(g, "classify");
    ASSERT(cls != NULL);
    ASSERT(cls->routing.kind == ROUTING_COMPARATOR);
    ASSERT(cls->routing.comparand == 5.0);
    ASSERT(cls->n_connections == 3);
    /* All three branches reachable. */
    int seen_lt = 0, seen_eq = 0, seen_gt = 0;
    for (int i = 0; i < cls->n_connections; i++) {
        const char *b = cls->connections[i].from_branch;
        if      (b && strcmp(b, "lt") == 0) seen_lt = 1;
        else if (b && strcmp(b, "eq") == 0) seen_eq = 1;
        else if (b && strcmp(b, "gt") == 0) seen_gt = 1;
    }
    ASSERT(seen_lt && seen_eq && seen_gt);

    graph_destroy(g);
    return 1;
}
/* }}} */

/* {{{ test_missing_map_dir() */
static int test_missing_map_dir(void)
{
    char *err = NULL;
    graph_t *g = graph_load("tests/maps/does-not-exist", &err);
    ASSERT(g == NULL);
    ASSERT(err != NULL);
    /* Error mentions the path. */
    ASSERT(strstr(err, "does-not-exist") != NULL);
    free(err);
    return 1;
}
/* }}} */

/* {{{ test_null_argument() */
static int test_null_argument(void)
{
    char *err = NULL;
    graph_t *g = graph_load(NULL, &err);
    ASSERT(g == NULL);
    ASSERT(err != NULL);
    free(err);
    return 1;
}
/* }}} */

/* {{{ Helpers for malformed-fixture tests */
/* Make a temp map directory with a given meta.json and a single
 * box file. Caller frees `out_dir` (or just lets it leak — tests
 * are short-lived). */
static int make_temp_map(const char *meta_json,
                         const char *box_name,
                         const char *box_json,
                         char **out_dir)
{
    char tmpl[] = "/tmp/soramech-loader-test-XXXXXX";
    char *dir = mkdtemp(tmpl);
    if (!dir) return -1;
    *out_dir = strdup(dir);

    char path[4096];
    snprintf(path, sizeof path, "%s/meta.json", dir);
    FILE *fp = fopen(path, "w"); if (!fp) return -1;
    fputs(meta_json, fp); fclose(fp);

    snprintf(path, sizeof path, "%s/boxes", dir);
    mkdir(path, 0755);

    snprintf(path, sizeof path, "%s/boxes/%s.json", dir, box_name);
    fp = fopen(path, "w"); if (!fp) return -1;
    fputs(box_json, fp); fclose(fp);

    return 0;
}

static void cleanup_temp_map(const char *dir, const char *box_name)
{
    char p[4096];
    snprintf(p, sizeof p, "%s/boxes/%s.json", dir, box_name); unlink(p);
    snprintf(p, sizeof p, "%s/meta.json", dir);              unlink(p);
    snprintf(p, sizeof p, "%s/boxes", dir);                  rmdir(p);
    rmdir(dir);
}
/* }}} */

/* {{{ test_unknown_kind() */
static int test_unknown_kind(void)
{
    char *dir = NULL;
    ASSERT(make_temp_map(
        "{\"name\":\"t\",\"entry_box_id\":\"x\"}",
        "x",
        "{\"id\":\"x\",\"kind\":\"frobnicate\"}",
        &dir) == 0);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g == NULL);
    ASSERT(err != NULL);
    ASSERT(strstr(err, "frobnicate") != NULL);

    free(err);
    cleanup_temp_map(dir, "x");
    free(dir);
    return 1;
}
/* }}} */

/* {{{ test_missing_routing() */
static int test_missing_routing(void)
{
    char *dir = NULL;
    ASSERT(make_temp_map(
        "{\"name\":\"t\",\"entry_box_id\":\"x\"}",
        "x",
        "{\"id\":\"x\",\"kind\":\"call\",\"ref\":\"foo.lua\",\"fn\":\"foo\"}",
        &dir) == 0);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g == NULL);
    ASSERT(err != NULL);
    ASSERT(strstr(err, "routing") != NULL);

    free(err);
    cleanup_temp_map(dir, "x");
    free(dir);
    return 1;
}
/* }}} */

/* {{{ test_connection_to_nonexistent_box() */
static int test_connection_to_nonexistent_box(void)
{
    char *dir = NULL;
    ASSERT(make_temp_map(
        "{\"name\":\"t\",\"entry_box_id\":\"a\"}",
        "a",
        "{\"id\":\"a\",\"kind\":\"call\",\"ref\":\"f.lua\",\"fn\":\"f\","
        "\"routing\":{\"kind\":\"plain\"},"
        "\"connections\":[{\"to_box\":\"ghost\",\"to_input\":\"x\"}]}",
        &dir) == 0);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g == NULL);
    ASSERT(err && strstr(err, "ghost") != NULL);
    free(err);
    cleanup_temp_map(dir, "a");
    free(dir);
    return 1;
}
/* }}} */

/* {{{ test_connection_to_nonexistent_input() */
static int test_connection_to_nonexistent_input(void)
{
    /* Two boxes; box 'a' connects to box 'b' on input 'bogus' that
     * doesn't exist. */
    char tmpl[] = "/tmp/soramech-noinput-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char path[4096];
    snprintf(path, sizeof path, "%s/meta.json", dir);
    FILE *fp = fopen(path, "w");
    fputs("{\"name\":\"t\",\"entry_box_id\":\"a\"}", fp);
    fclose(fp);

    snprintf(path, sizeof path, "%s/boxes", dir);
    mkdir(path, 0755);

    snprintf(path, sizeof path, "%s/boxes/a.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"a\",\"kind\":\"call\",\"ref\":\"f.lua\",\"fn\":\"f\","
          "\"routing\":{\"kind\":\"plain\"},"
          "\"connections\":[{\"to_box\":\"b\",\"to_input\":\"bogus\"}]}", fp);
    fclose(fp);

    snprintf(path, sizeof path, "%s/boxes/b.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"b\",\"kind\":\"call\",\"ref\":\"g.lua\",\"fn\":\"g\","
          "\"routing\":{\"kind\":\"plain\"},"
          "\"inputs\":[{\"name\":\"correct\",\"type\":\"string\"}]}", fp);
    fclose(fp);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g == NULL);
    ASSERT(err && strstr(err, "bogus") != NULL);
    free(err);

    snprintf(path, sizeof path, "%s/boxes/a.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes/b.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/meta.json",   dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes",       dir); rmdir(path);
    rmdir(dir);
    return 1;
}
/* }}} */

/* {{{ test_non_iterator_cycle() */
static int test_non_iterator_cycle(void)
{
    /* Two boxes pointing at each other; no iterator → reject. */
    char tmpl[] = "/tmp/soramech-cycle-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char path[4096];
    snprintf(path, sizeof path, "%s/meta.json", dir);
    FILE *fp = fopen(path, "w");
    fputs("{\"name\":\"t\",\"entry_box_id\":\"a\"}", fp);
    fclose(fp);
    snprintf(path, sizeof path, "%s/boxes", dir); mkdir(path, 0755);

    snprintf(path, sizeof path, "%s/boxes/a.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"a\",\"kind\":\"call\",\"ref\":\"f.lua\",\"fn\":\"f\","
          "\"routing\":{\"kind\":\"plain\"},"
          "\"inputs\":[{\"name\":\"v\",\"type\":\"string\"}],"
          "\"connections\":[{\"to_box\":\"b\",\"to_input\":\"v\"}]}", fp);
    fclose(fp);

    snprintf(path, sizeof path, "%s/boxes/b.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"b\",\"kind\":\"call\",\"ref\":\"g.lua\",\"fn\":\"g\","
          "\"routing\":{\"kind\":\"plain\"},"
          "\"inputs\":[{\"name\":\"v\",\"type\":\"string\"}],"
          "\"connections\":[{\"to_box\":\"a\",\"to_input\":\"v\"}]}", fp);
    fclose(fp);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g == NULL);
    ASSERT(err && strstr(err, "cycle") != NULL);
    free(err);

    snprintf(path, sizeof path, "%s/boxes/a.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes/b.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/meta.json",   dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes",       dir); rmdir(path);
    rmdir(dir);
    return 1;
}
/* }}} */

/* {{{ test_iterator_cycle_allowed() */
static int test_iterator_cycle_allowed(void)
{
    /* Three-box cycle a → iter → b → a. The iterator cuts the
     * cycle; load must succeed. */
    char tmpl[] = "/tmp/soramech-itercycle-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char path[4096];
    snprintf(path, sizeof path, "%s/meta.json", dir);
    FILE *fp = fopen(path, "w");
    fputs("{\"name\":\"t\",\"entry_box_id\":\"a\"}", fp);
    fclose(fp);
    snprintf(path, sizeof path, "%s/boxes", dir); mkdir(path, 0755);

    snprintf(path, sizeof path, "%s/boxes/a.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"a\",\"kind\":\"call\",\"ref\":\"f.lua\",\"fn\":\"f\","
          "\"routing\":{\"kind\":\"plain\"},"
          "\"inputs\":[{\"name\":\"feedback\",\"type\":\"string\"}],"
          "\"connections\":[{\"to_box\":\"iter\",\"to_input\":\"in\"}]}", fp);
    fclose(fp);

    snprintf(path, sizeof path, "%s/boxes/iter.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"iter\",\"kind\":\"call\",\"ref\":\"i.lua\",\"fn\":\"i\","
          "\"routing\":{\"kind\":\"iterator\",\"n_outputs\":1},"
          "\"inputs\":[{\"name\":\"in\",\"type\":\"string\"}],"
          "\"connections\":[{\"from_branch\":\"out_0\","
                            "\"to_box\":\"b\",\"to_input\":\"v\"}]}", fp);
    fclose(fp);

    snprintf(path, sizeof path, "%s/boxes/b.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"b\",\"kind\":\"call\",\"ref\":\"g.lua\",\"fn\":\"g\","
          "\"routing\":{\"kind\":\"plain\"},"
          "\"inputs\":[{\"name\":\"v\",\"type\":\"string\"}],"
          "\"connections\":[{\"to_box\":\"a\",\"to_input\":\"feedback\"}]}", fp);
    fclose(fp);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    if (!g) fprintf(stderr, "      load failed: %s\n", err ? err : "(null)");
    ASSERT(g != NULL);
    ASSERT(err == NULL);
    graph_destroy(g);

    snprintf(path, sizeof path, "%s/boxes/a.json",    dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes/iter.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes/b.json",    dir); unlink(path);
    snprintf(path, sizeof path, "%s/meta.json",       dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes",           dir); rmdir(path);
    rmdir(dir);
    return 1;
}
/* }}} */

/* {{{ test_duplicate_id() */
static int test_duplicate_id(void)
{
    /* Use a temp dir with two boxes whose JSON declares the same id. */
    char tmpl[] = "/tmp/soramech-dup-test-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char path[4096];
    snprintf(path, sizeof path, "%s/meta.json", dir);
    FILE *fp = fopen(path, "w");
    fputs("{\"name\":\"t\",\"entry_box_id\":\"x\"}", fp);
    fclose(fp);

    snprintf(path, sizeof path, "%s/boxes", dir);
    mkdir(path, 0755);

    snprintf(path, sizeof path, "%s/boxes/a.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"x\",\"kind\":\"call\",\"ref\":\"f.lua\",\"fn\":\"f\","
          "\"routing\":{\"kind\":\"plain\"}}", fp);
    fclose(fp);

    snprintf(path, sizeof path, "%s/boxes/b.json", dir);
    fp = fopen(path, "w");
    fputs("{\"id\":\"x\",\"kind\":\"call\",\"ref\":\"g.lua\",\"fn\":\"g\","
          "\"routing\":{\"kind\":\"plain\"}}", fp);
    fclose(fp);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g == NULL);
    ASSERT(err && strstr(err, "duplicate") != NULL);
    free(err);

    /* Cleanup. */
    snprintf(path, sizeof path, "%s/boxes/a.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes/b.json", dir); unlink(path);
    snprintf(path, sizeof path, "%s/meta.json",   dir); unlink(path);
    snprintf(path, sizeof path, "%s/boxes",       dir); rmdir(path);
    rmdir(dir);

    return 1;
}
/* }}} */

/* {{{ test_variable_size_producer_lvh_wiring() */
/* 302 follow-on: graph_attach_runtime sets SLOT_FLAG_LARGE_VALUE on
 * any input slot whose producer declares variable-size output
 * (output_capacity == 0). The hello fixture pairs greet (call,
 * output_capacity=256) with who (data box, output_capacity defaults
 * to 0). After attach, greet's input port 0 (`name`, fed by who)
 * should accept a push that exceeds the slot's normal cell
 * capacity, while port 1 (`salutation`, literal-only) should not. */
static int test_variable_size_producer_lvh_wiring(void)
{
    char *err = NULL;
    graph_t *g = graph_load("tests/maps/hello", &err);
    ASSERT(g);

    spec_registry_t *r = spec_registry_load("langs", &err);
    ASSERT(r);

    slot_store_t *s = slot_store_create();
    ASSERT(s);
    /* Tight default cell so the discriminator (push 2 KB) is well
     * outside it — without LARGE_VALUE the push would fail. */
    ASSERT(graph_attach_runtime(g, s, r, 512, &err) == 0);

    const box_t *greet = graph_box_by_id(g, "greet");
    ASSERT(greet && greet->n_inputs == 2);

    char big[2048];
    memset(big, 'A', sizeof big);

    /* Port 0 (`name`) is fed by data box `who` → LARGE_VALUE. */
    ASSERT(slot_push(s, greet->input_slot_ids[0], big, sizeof big, 0) == 0);

    /* Port 1 (`salutation`) has only a literal — no variable-size
     * producer → fixed-size slot → 2 KB push exceeds cell capacity
     * and fails. */
    ASSERT(slot_push(s, greet->input_slot_ids[1], big, sizeof big, 0) == -1);

    slot_store_destroy(s);
    spec_registry_destroy(r);
    graph_destroy(g);
    return 1;
}
/* }}} */

/* {{{ test_entry_box_detection() */
/* Phase 6: graph_load fills entry_box_ids with the indices of boxes
 * the pool runner submits first. A call box whose only input is fed
 * by a read box (a literal value source) qualifies; a call box fed
 * by another call box does not. The hello fixture has exactly one
 * call box, `greet`, whose computed input is wired from a read box,
 * so it must be the sole entry. */
static int test_entry_box_detection(void)
{
    char *err = NULL;
    graph_t *g = graph_load("tests/maps/hello", &err);
    ASSERT(g);

    ASSERT(graph_n_entry_boxes(g) == 1);
    int idx = graph_entry_box(g, 0);
    ASSERT(idx >= 0);
    const box_t *eb = graph_box(g, idx);
    ASSERT(eb && strcmp(eb->id, "greet") == 0);

    graph_destroy(g);
    return 1;
}
/* }}} */

/* {{{ test_entry_box_excludes_downstream() */
/* When a call box's input is fed by another call box, that box must
 * NOT be in the entry set — it has to wait for upstream computation.
 * Build a temp map: read -> A (entry), A -> B (not entry). */
static int test_entry_box_excludes_downstream(void)
{
    char tmpl[] = "/tmp/soramech-entry-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char p[4096];
    snprintf(p, sizeof p, "%s/meta.json", dir);
    FILE *fp = fopen(p, "w");
    fputs("{\"name\":\"e\",\"entry_box_id\":\"a\"}", fp); fclose(fp);
    snprintf(p, sizeof p, "%s/boxes", dir); mkdir(p, 0755);

    snprintf(p, sizeof p, "%s/boxes/lit.json", dir);
    fp = fopen(p, "w");
    fputs("{\"id\":\"lit\",\"kind\":\"read\",\"value\":\"x\","
          "\"connections\":[{\"to_box\":\"a\",\"to_input\":\"in\"}]}", fp);
    fclose(fp);

    snprintf(p, sizeof p, "%s/boxes/a.json", dir);
    fp = fopen(p, "w");
    fputs("{\"id\":\"a\",\"kind\":\"call\",\"lang\":\"lua\",\"ref\":\"a.lua\","
          "\"fn\":\"a\",\"routing\":{\"kind\":\"plain\"},"
          "\"inputs\":[{\"name\":\"in\",\"type\":\"string\"}],"
          "\"connections\":[{\"to_box\":\"b\",\"to_input\":\"in\"}]}", fp);
    fclose(fp);

    snprintf(p, sizeof p, "%s/boxes/b.json", dir);
    fp = fopen(p, "w");
    fputs("{\"id\":\"b\",\"kind\":\"call\",\"lang\":\"lua\",\"ref\":\"a.lua\","
          "\"fn\":\"b\",\"routing\":{\"kind\":\"plain\"},"
          "\"inputs\":[{\"name\":\"in\",\"type\":\"string\"}]}", fp);
    fclose(fp);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g);

    ASSERT(graph_n_entry_boxes(g) == 1);
    const box_t *eb = graph_box(g, graph_entry_box(g, 0));
    ASSERT(eb && strcmp(eb->id, "a") == 0);

    graph_destroy(g);
    snprintf(p, sizeof p, "%s/boxes/lit.json", dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes/a.json",   dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes/b.json",   dir); unlink(p);
    snprintf(p, sizeof p, "%s/meta.json",      dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes",          dir); rmdir(p);
    rmdir(dir);
    return 1;
}
/* }}} */

/* {{{ test_size_class_enumeration() */
/* graph_attach_runtime collects the distinct cell widths it picked
 * across every input port. Build a map whose two call producers
 * declare different output_capacity values; attach with a small
 * default so the producer caps actually dominate. */
static int test_size_class_enumeration(void)
{
    char tmpl[] = "/tmp/soramech-size-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char p[4096];
    snprintf(p, sizeof p, "%s/meta.json", dir);
    FILE *fp = fopen(p, "w");
    fputs("{\"name\":\"s\",\"entry_box_id\":\"p256\"}", fp); fclose(fp);
    snprintf(p, sizeof p, "%s/boxes", dir); mkdir(p, 0755);

    snprintf(p, sizeof p, "%s/boxes/p256.json", dir);
    fp = fopen(p, "w");
    fputs("{\"id\":\"p256\",\"kind\":\"call\",\"lang\":\"lua\",\"ref\":\"x.lua\","
          "\"fn\":\"x\",\"routing\":{\"kind\":\"plain\"},"
          "\"output_capacity\":256,"
          "\"connections\":[{\"to_box\":\"sink\",\"to_input\":\"a\"}]}", fp);
    fclose(fp);

    snprintf(p, sizeof p, "%s/boxes/p1024.json", dir);
    fp = fopen(p, "w");
    fputs("{\"id\":\"p1024\",\"kind\":\"call\",\"lang\":\"lua\",\"ref\":\"x.lua\","
          "\"fn\":\"y\",\"routing\":{\"kind\":\"plain\"},"
          "\"output_capacity\":1024,"
          "\"connections\":[{\"to_box\":\"sink\",\"to_input\":\"b\"}]}", fp);
    fclose(fp);

    snprintf(p, sizeof p, "%s/boxes/sink.json", dir);
    fp = fopen(p, "w");
    fputs("{\"id\":\"sink\",\"kind\":\"call\",\"lang\":\"lua\",\"ref\":\"x.lua\","
          "\"fn\":\"s\",\"routing\":{\"kind\":\"plain\"},"
          "\"inputs\":[{\"name\":\"a\",\"type\":\"string\"},"
                      "{\"name\":\"b\",\"type\":\"string\"}]}", fp);
    fclose(fp);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g);

    spec_registry_t *r = spec_registry_load("langs", &err);
    ASSERT(r);
    slot_store_t *s = slot_store_create();
    ASSERT(s);
    ASSERT(graph_attach_runtime(g, s, r, 64, &err) == 0);

    /* Two distinct widths: 256 (for sink.a) and 1024 (for sink.b).
     * The producer boxes themselves have no input ports, so they
     * contribute nothing. */
    ASSERT(graph_n_size_classes(g) == 2);
    int saw_256 = 0, saw_1024 = 0;
    for (int i = 0; i < graph_n_size_classes(g); i++) {
        int w = graph_size_class(g, i);
        if (w == 256)  saw_256  = 1;
        if (w == 1024) saw_1024 = 1;
    }
    ASSERT(saw_256 && saw_1024);

    slot_store_destroy(s);
    spec_registry_destroy(r);
    graph_destroy(g);
    snprintf(p, sizeof p, "%s/boxes/p256.json",  dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes/p1024.json", dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes/sink.json",  dir); unlink(p);
    snprintf(p, sizeof p, "%s/meta.json",        dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes",            dir); rmdir(p);
    rmdir(dir);
    return 1;
}
/* }}} */

/* {{{ test_distributor_parsing() */
static int test_distributor_parsing(void)
{
    char tmpl[] = "/tmp/soramech-dist-XXXXXX";
    char *dir = mkdtemp(tmpl);
    ASSERT(dir);

    char p[4096];
    snprintf(p, sizeof p, "%s/meta.json", dir);
    FILE *fp = fopen(p, "w");
    fputs("{\"name\":\"d\",\"entry_box_id\":\"dst\"}", fp); fclose(fp);
    snprintf(p, sizeof p, "%s/boxes", dir); mkdir(p, 0755);

    snprintf(p, sizeof p, "%s/boxes/dst.json", dir);
    fp = fopen(p, "w");
    fputs("{\"id\":\"dst\",\"kind\":\"call\",\"lang\":\"lua\",\"ref\":\"d.lua\","
          "\"fn\":\"d\",\"routing\":{\"kind\":\"distributor\",\"n_outputs\":3}}", fp);
    fclose(fp);

    char *err = NULL;
    graph_t *g = graph_load(dir, &err);
    ASSERT(g);
    const box_t *dst = graph_box_by_id(g, "dst");
    ASSERT(dst && dst->routing.kind == ROUTING_DISTRIBUTOR);
    ASSERT(dst->routing.n_outputs == 3);

    graph_destroy(g);
    snprintf(p, sizeof p, "%s/boxes/dst.json", dir); unlink(p);
    snprintf(p, sizeof p, "%s/meta.json",      dir); unlink(p);
    snprintf(p, sizeof p, "%s/boxes",          dir); rmdir(p);
    rmdir(dir);
    return 1;
}
/* }}} */

/* {{{ main() */
int main(void)
{
    /* The test runner is invoked from the project root by `make test`. */
    if (!has_fixtures()) {
        fprintf(stderr, "010-graph-loader-test: tests/maps/ not found; "
                        "run from project root\n");
        return 2;
    }
    printf("010-graph-loader-test:\n");
    RUN(load_hello);
    RUN(load_branching);
    RUN(missing_map_dir);
    RUN(null_argument);
    RUN(unknown_kind);
    RUN(missing_routing);
    RUN(connection_to_nonexistent_box);
    RUN(connection_to_nonexistent_input);
    RUN(non_iterator_cycle);
    RUN(iterator_cycle_allowed);
    RUN(duplicate_id);
    RUN(native_invoke_classification);
    RUN(randomizer_weighted_parsing);
    RUN(variable_size_producer_lvh_wiring);
    RUN(entry_box_detection);
    RUN(entry_box_excludes_downstream);
    RUN(size_class_enumeration);
    RUN(distributor_parsing);
    printf("\n  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
/* }}} */
