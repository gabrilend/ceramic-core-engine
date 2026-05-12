/* src/010-graph-loader.c — C graph loader, implementation.
 *
 * Pipeline: open the map directory; parse meta.json; walk every
 * .json file under boxes/; parse each box; flatten input
 * declarations and outgoing connections per box; validate
 * per-box schema.
 *
 * Memory model: a single json_arena_t owned by the graph_t holds
 * every string and every parsed JSON tree across every file in the
 * map. Box / connection / input arrays are plain malloc — fixed
 * size after the load completes. graph_destroy frees both.
 *
 * Designed in issue 305. Phases 4–7 (topology, size classes, entry
 * boxes, language enumeration) ship in follow-on iterations.
 */

#include "010-graph-loader.h"
#include "json.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ Graph struct */
struct graph {
    json_arena_t *arena;

    const char   *name;
    const char   *description;
    const char   *entry_box_id;

    int           n_boxes;
    box_t        *boxes;
};
/* }}} */

/* {{{ err_fmt() — malloc'd diagnostic string */
__attribute__((format(printf, 1, 2)))
static char *err_fmt(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    size_t len = (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, buf, len);
    out[len] = '\0';
    return out;
}
/* }}} */

/* {{{ ends_with() — true iff `s` ends with `suffix` */
static int ends_with(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    if (ls < lf) return 0;
    return memcmp(s + ls - lf, suffix, lf) == 0;
}
/* }}} */

/* {{{ parse_routing() — translate JSON routing object into routing_t */
static int parse_routing(const json_node_t *r, routing_t *out,
                         const char *box_id, char **err)
{
    if (!r) {
        *err = err_fmt("box '%s': missing 'routing' field", box_id);
        return -1;
    }
    if (json_kind((json_node_t *)r) != JSON_OBJECT) {
        *err = err_fmt("box '%s': 'routing' is not an object", box_id);
        return -1;
    }
    json_node_t *kind_n = json_object_get(r, "kind");
    if (!kind_n || json_kind(kind_n) != JSON_STRING) {
        *err = err_fmt("box '%s': routing.kind missing or not a string", box_id);
        return -1;
    }
    const char *kind = json_string_value(kind_n);

    memset(out, 0, sizeof *out);

    if (strcmp(kind, "plain") == 0) {
        out->kind = ROUTING_PLAIN;
        out->n_outputs = 1;
        return 0;
    }
    if (strcmp(kind, "comparator") == 0) {
        out->kind = ROUTING_COMPARATOR;
        out->n_outputs = 3;
        json_node_t *c = json_object_get(r, "comparand");
        if (!c) {
            *err = err_fmt("box '%s': comparator routing missing 'comparand'", box_id);
            return -1;
        }
        if (json_kind(c) == JSON_NUMBER) {
            out->comparand = json_number_value(c);
        } else if (json_kind(c) == JSON_STRING) {
            /* Issue 233 example shows comparand as a string;
             * tolerate either form, parse the string as a double. */
            const char *s = json_string_value(c);
            char *endp = NULL;
            out->comparand = strtod(s, &endp);
            if (endp == s) {
                *err = err_fmt("box '%s': comparand '%s' is not a number", box_id, s);
                return -1;
            }
        } else {
            *err = err_fmt("box '%s': comparand must be number or string", box_id);
            return -1;
        }
        return 0;
    }
    if (strcmp(kind, "iterator") == 0) {
        out->kind = ROUTING_ITERATOR;
        json_node_t *n = json_object_get(r, "n_outputs");
        if (!n || json_kind(n) != JSON_NUMBER) {
            *err = err_fmt("box '%s': iterator routing missing 'n_outputs'", box_id);
            return -1;
        }
        out->n_outputs = (int)json_number_value(n);
        if (out->n_outputs < 1) {
            *err = err_fmt("box '%s': iterator n_outputs must be >= 1", box_id);
            return -1;
        }
        return 0;
    }

    *err = err_fmt("box '%s': unknown routing.kind '%s' "
                   "(this iteration supports plain, comparator, iterator)",
                   box_id, kind);
    return -1;
}
/* }}} */

/* {{{ parse_inputs() — input port array */
static int parse_inputs(json_node_t *node, box_t *box,
                        const char *box_id, char **err)
{
    if (!node) {
        box->n_inputs = 0;
        box->inputs = NULL;
        return 0;
    }
    if (json_kind(node) != JSON_ARRAY) {
        *err = err_fmt("box '%s': 'inputs' is not an array", box_id);
        return -1;
    }
    int n = json_array_size(node);
    box->n_inputs = n;
    if (n == 0) { box->inputs = NULL; return 0; }

    box->inputs = calloc((size_t)n, sizeof(input_decl_t));
    if (!box->inputs) {
        *err = err_fmt("out of memory");
        return -1;
    }

    for (int i = 0; i < n; i++) {
        json_node_t *item = json_array_at(node, i);
        if (!item || json_kind(item) != JSON_OBJECT) {
            *err = err_fmt("box '%s': input port %d is not an object", box_id, i);
            return -1;
        }
        json_node_t *name_n = json_object_get(item, "name");
        if (!name_n || json_kind(name_n) != JSON_STRING) {
            *err = err_fmt("box '%s': input port %d missing 'name'", box_id, i);
            return -1;
        }
        box->inputs[i].name = json_string_value(name_n);

        json_node_t *type_n = json_object_get(item, "type");
        box->inputs[i].type = (type_n && json_kind(type_n) == JSON_STRING)
                                ? json_string_value(type_n) : "string";

        /* `value`: only string literals supported in this iteration. */
        json_node_t *val_n = json_object_get(item, "value");
        box->inputs[i].literal = (val_n && json_kind(val_n) == JSON_STRING)
                                    ? json_string_value(val_n) : NULL;

        json_node_t *opt_n = json_object_get(item, "optional");
        box->inputs[i].optional = (opt_n && json_kind(opt_n) == JSON_BOOL)
                                    ? json_bool_value(opt_n) : 0;
    }
    return 0;
}
/* }}} */

/* {{{ parse_connections() — outgoing connection array */
static int parse_connections(json_node_t *node, box_t *box,
                             const char *box_id, char **err)
{
    if (!node) {
        box->n_connections = 0;
        box->connections = NULL;
        return 0;
    }
    if (json_kind(node) != JSON_ARRAY) {
        *err = err_fmt("box '%s': 'connections' is not an array", box_id);
        return -1;
    }
    int n = json_array_size(node);
    box->n_connections = n;
    if (n == 0) { box->connections = NULL; return 0; }

    box->connections = calloc((size_t)n, sizeof(connection_t));
    if (!box->connections) {
        *err = err_fmt("out of memory");
        return -1;
    }

    for (int i = 0; i < n; i++) {
        json_node_t *c = json_array_at(node, i);
        if (!c || json_kind(c) != JSON_OBJECT) {
            *err = err_fmt("box '%s': connection %d is not an object", box_id, i);
            return -1;
        }
        json_node_t *to_box_n   = json_object_get(c, "to_box");
        json_node_t *to_input_n = json_object_get(c, "to_input");
        if (!to_box_n || json_kind(to_box_n) != JSON_STRING) {
            *err = err_fmt("box '%s': connection %d missing 'to_box'", box_id, i);
            return -1;
        }
        if (!to_input_n || json_kind(to_input_n) != JSON_STRING) {
            *err = err_fmt("box '%s': connection %d missing 'to_input'", box_id, i);
            return -1;
        }
        box->connections[i].to_box   = json_string_value(to_box_n);
        box->connections[i].to_input = json_string_value(to_input_n);

        /* from_branch is optional: NULL for plain output ports, or
         * "lt"/"eq"/"gt" for comparator, or branch name for
         * iterator. The validation that the branch matches the
         * routing kind lives in phase 4 (topology). */
        json_node_t *fb = json_object_get(c, "from_branch");
        box->connections[i].from_branch =
            (fb && json_kind(fb) == JSON_STRING) ? json_string_value(fb) : NULL;
    }
    return 0;
}
/* }}} */

/* {{{ parse_box_file() — read one boxes/<id>.json into a box_t */
static int parse_box_file(graph_t *g, box_t *box,
                          const char *path, char **err)
{
    int err_line = 0;
    const char *err_msg = NULL;
    json_node_t *n = json_parse_file(g->arena, path, &err_line, &err_msg);
    if (!n) {
        *err = err_fmt("%s:%d: %s", path, err_line, err_msg ? err_msg : "parse error");
        return -1;
    }
    if (json_kind(n) != JSON_OBJECT) {
        *err = err_fmt("%s: top-level must be a JSON object", path);
        return -1;
    }

    memset(box, 0, sizeof *box);

    /* id */
    json_node_t *id_n = json_object_get(n, "id");
    if (!id_n || json_kind(id_n) != JSON_STRING) {
        *err = err_fmt("%s: missing or non-string 'id'", path);
        return -1;
    }
    box->id = json_string_value(id_n);

    /* kind */
    json_node_t *kind_n = json_object_get(n, "kind");
    if (!kind_n || json_kind(kind_n) != JSON_STRING) {
        *err = err_fmt("%s: box '%s' missing 'kind'", path, box->id);
        return -1;
    }
    const char *kind_str = json_string_value(kind_n);
    if      (strcmp(kind_str, "call")       == 0) box->kind = BOX_CALL;
    else if (strcmp(kind_str, "data")       == 0) box->kind = BOX_DATA;
    else if (strcmp(kind_str, "file_write") == 0) box->kind = BOX_FILE_WRITE;
    else {
        *err = err_fmt("%s: box '%s': unknown kind '%s'", path, box->id, kind_str);
        return -1;
    }

    /* Kind-specific required fields */
    if (box->kind == BOX_CALL) {
        json_node_t *ref_n = json_object_get(n, "ref");
        if (!ref_n || json_kind(ref_n) != JSON_STRING) {
            *err = err_fmt("%s: call box '%s' missing 'ref'", path, box->id);
            return -1;
        }
        box->ref = json_string_value(ref_n);

        json_node_t *fn_n = json_object_get(n, "fn");
        if (fn_n && json_kind(fn_n) == JSON_STRING) {
            box->fn = json_string_value(fn_n);
        }

        json_node_t *lang_n = json_object_get(n, "lang");
        if (lang_n && json_kind(lang_n) == JSON_STRING) {
            box->lang = json_string_value(lang_n);
        }

        json_node_t *r_n = json_object_get(n, "routing");
        if (parse_routing(r_n, &box->routing, box->id, err) != 0) return -1;

        json_node_t *oc_n = json_object_get(n, "output_capacity");
        if (oc_n && json_kind(oc_n) == JSON_NUMBER) {
            box->output_capacity = (int)json_number_value(oc_n);
        }
    } else if (box->kind == BOX_DATA) {
        json_node_t *path_n = json_object_get(n, "path");
        if (!path_n || json_kind(path_n) != JSON_STRING) {
            *err = err_fmt("%s: data box '%s' missing 'path'", path, box->id);
            return -1;
        }
        box->path = json_string_value(path_n);
    }
    /* file_write boxes: nothing additional beyond inputs. */

    if (parse_inputs(json_object_get(n, "inputs"),
                     box, box->id, err) != 0) return -1;
    if (parse_connections(json_object_get(n, "connections"),
                          box, box->id, err) != 0) return -1;

    return 0;
}
/* }}} */

/* {{{ count_json_files_in_dir() — first pass, count .json entries */
static int count_json_files_in_dir(const char *dir, char **err)
{
    DIR *d = opendir(dir);
    if (!d) {
        *err = err_fmt("cannot open directory '%s'", dir);
        return -1;
    }
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (ends_with(e->d_name, ".json")) n++;
    }
    closedir(d);
    return n;
}
/* }}} */

/* {{{ load_meta() — parse map_dir/meta.json into the graph */
static int load_meta(graph_t *g, const char *map_dir, char **err)
{
    char path[4096];
    int w = snprintf(path, sizeof path, "%s/meta.json", map_dir);
    if (w < 0 || w >= (int)sizeof path) {
        *err = err_fmt("meta.json path too long");
        return -1;
    }

    int err_line = 0; const char *err_msg = NULL;
    json_node_t *meta = json_parse_file(g->arena, path, &err_line, &err_msg);
    if (!meta) {
        *err = err_fmt("%s:%d: %s", path, err_line, err_msg ? err_msg : "parse error");
        return -1;
    }
    if (json_kind(meta) != JSON_OBJECT) {
        *err = err_fmt("%s: top-level must be a JSON object", path);
        return -1;
    }

    json_node_t *name = json_object_get(meta, "name");
    if (!name || json_kind(name) != JSON_STRING) {
        *err = err_fmt("%s: missing 'name'", path);
        return -1;
    }
    g->name = json_string_value(name);

    json_node_t *entry = json_object_get(meta, "entry_box_id");
    if (entry && json_kind(entry) == JSON_STRING) {
        g->entry_box_id = json_string_value(entry);
    }

    json_node_t *desc = json_object_get(meta, "description");
    if (desc && json_kind(desc) == JSON_STRING) {
        g->description = json_string_value(desc);
    }

    return 0;
}
/* }}} */

/* {{{ load_boxes() — walk boxes/, parse every .json into g->boxes */
static int load_boxes(graph_t *g, const char *map_dir, char **err)
{
    char dir[4096];
    int w = snprintf(dir, sizeof dir, "%s/boxes", map_dir);
    if (w < 0 || w >= (int)sizeof dir) {
        *err = err_fmt("boxes path too long");
        return -1;
    }

    int n = count_json_files_in_dir(dir, err);
    if (n < 0) return -1;

    g->n_boxes = n;
    if (n == 0) { g->boxes = NULL; return 0; }
    g->boxes = calloc((size_t)n, sizeof(box_t));
    if (!g->boxes) { *err = err_fmt("out of memory"); return -1; }

    DIR *d = opendir(dir);
    if (!d) { *err = err_fmt("cannot open directory '%s'", dir); return -1; }

    int i = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')              continue;
        if (!ends_with(e->d_name, ".json"))   continue;

        char path[4096];
        int wp = snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (wp < 0 || wp >= (int)sizeof path) {
            closedir(d);
            *err = err_fmt("box path too long");
            return -1;
        }

        if (parse_box_file(g, &g->boxes[i], path, err) != 0) {
            closedir(d);
            return -1;
        }
        i++;
    }
    closedir(d);

    /* Validate id uniqueness (O(n^2) scan; n is small). */
    for (int j = 0; j < n; j++) {
        for (int k = j + 1; k < n; k++) {
            if (strcmp(g->boxes[j].id, g->boxes[k].id) == 0) {
                *err = err_fmt("duplicate box id '%s'", g->boxes[j].id);
                return -1;
            }
        }
    }

    return 0;
}
/* }}} */

/* {{{ graph_load() */
graph_t *graph_load(const char *map_dir, char **err)
{
    if (err) *err = NULL;
    if (!map_dir) {
        if (err) *err = err_fmt("graph_load: NULL map_dir");
        return NULL;
    }

    graph_t *g = calloc(1, sizeof *g);
    if (!g) { if (err) *err = err_fmt("out of memory"); return NULL; }

    g->arena = json_arena_create();
    if (!g->arena) {
        if (err) *err = err_fmt("out of memory");
        free(g);
        return NULL;
    }

    if (load_meta(g, map_dir, err) != 0)  { graph_destroy(g); return NULL; }
    if (load_boxes(g, map_dir, err) != 0) { graph_destroy(g); return NULL; }
    return g;
}
/* }}} */

/* {{{ graph_destroy() */
void graph_destroy(graph_t *g)
{
    if (!g) return;
    if (g->boxes) {
        for (int i = 0; i < g->n_boxes; i++) {
            free(g->boxes[i].inputs);
            free(g->boxes[i].connections);
        }
        free(g->boxes);
    }
    json_arena_destroy(g->arena);
    free(g);
}
/* }}} */

/* {{{ Accessors */
const char *graph_name(const graph_t *g)         { return g ? g->name : NULL; }
const char *graph_description(const graph_t *g)  { return g ? g->description : NULL; }
const char *graph_entry_box_id(const graph_t *g) { return g ? g->entry_box_id : NULL; }
int         graph_n_boxes(const graph_t *g)      { return g ? g->n_boxes : 0; }

const box_t *graph_box(const graph_t *g, int i)
{
    if (!g || i < 0 || i >= g->n_boxes) return NULL;
    return &g->boxes[i];
}

const box_t *graph_box_by_id(const graph_t *g, const char *id)
{
    if (!g || !id) return NULL;
    for (int i = 0; i < g->n_boxes; i++) {
        if (strcmp(g->boxes[i].id, id) == 0) return &g->boxes[i];
    }
    return NULL;
}
/* }}} */
