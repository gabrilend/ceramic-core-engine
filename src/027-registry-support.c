/*
 * 027-registry-support.c — walking what the generator wrote.
 *
 * What this is: the hand-written half of the registry — lookups,
 * printing, and placement-by-name. The data it walks is generated at
 * build time from the box sources; this file never changes when a
 * box does, which is the entire division of labor.
 *
 * How it does it, in general terms: linear walks over two small
 * const arrays. A program has dozens of boxes, not millions, and a
 * lookup happens at load time, not on the delivery path; simplicity
 * wins over any table cleverness.
 */
#include "026-registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ registry_find() */
const box_info_t *registry_find(const char *name)
{
    for (int i = 0; i < registry_n_boxes; i++)
        if (strcmp(registry_boxes[i].name, name) == 0)
            return &registry_boxes[i];
    return NULL;
}
/* }}} */

/* {{{ struct_find() */
const struct_info_t *struct_find(const char *type_name)
{
    for (int i = 0; i < registry_n_structs; i++)
        if (strcmp(registry_structs[i].name, type_name) == 0)
            return &registry_structs[i];
    return NULL;
}
/* }}} */

/* {{{ registry_print() */
void registry_print(FILE *out)
{
    fprintf(out, "registry: %d boxes, %d structs\n",
            registry_n_boxes, registry_n_structs);
    for (int i = 0; i < registry_n_boxes; i++) {
        const box_info_t *b = &registry_boxes[i];
        fprintf(out, "  %-16s (", b->name);
        for (int p = 0; p < b->n_params; p++)
            fprintf(out, "%s%s:%d", p ? ", " : "",
                    b->params[p].type_name, b->params[p].size);
        fprintf(out, ") -> %s:%d  task %zu bytes  compare %s\n",
                b->return_type, b->return_size, b->task_size,
                b->compare ? "yes" : "no");
    }
    for (int i = 0; i < registry_n_structs; i++) {
        const struct_info_t *s = &registry_structs[i];
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

/* {{{ map_place_box() */
void map_place_box(map_t *m, int station, const char *box_name, int kind)
{
    const box_info_t *b = registry_find(box_name);
    if (!b) {
        /* The most common mistake a map will ever contain. */
        fprintf(stderr,
                "map: no box named '%s' in the registry — misspelled, or its "
                "source is not under src/boxes/\n", box_name);
        abort();
    }

    /* A comparator carries one extra slot at the end of the array,
     * holding the value to compare against, typed to match the box's
     * return value because that is what it will be compared with
     * (issue 502). The shim is handed only the real parameters; the
     * readiness walk sees all of them and does not care. */
    int extra = (kind == STATION_COMPARATOR) ? 1 : 0;
    if (extra && b->return_size == 0) {
        fprintf(stderr,
                "map: '%s' cannot be a comparator — it returns nothing, so "
                "there is nothing to compare\n", box_name);
        abort();
    }

    int n = b->n_params + extra;
    int sizes[n > 0 ? n : 1];
    for (int i = 0; i < b->n_params; i++)
        sizes[i] = b->params[i].size;
    if (extra)
        sizes[b->n_params] = b->return_size;

    map_place(m, station, b->shim, kind, n, sizes, b->return_size);
}
/* }}} */
