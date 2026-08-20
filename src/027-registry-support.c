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
const box_info_t *registry_late_find(const char *name);
const char       *registry_late_name_for_shim(task_call_t shim);
const box_info_t *registry_recover_box(const char *name);

/* {{{ registry_find() */
/*
 * The generated rows first, then anything compiled in later. That
 * order is deliberate: a box the program was built with wins over one
 * added afterwards under the same name, so bringing in new code can
 * never quietly replace something a map already depends on. Replacing
 * a name that was never built in works, and shadowing one that was
 * does not.
 */
const box_info_t *registry_find(const char *name)
{
    for (int i = 0; i < registry_n_boxes; i++)
        if (strcmp(registry_boxes[i].name, name) == 0)
            return &registry_boxes[i];
    return registry_late_find(name);
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

/* {{{ registry_box_name_for_shim() */
const char *registry_box_name_for_shim(task_call_t shim)
{
    for (int i = 0; i < registry_n_boxes; i++)
        if (registry_boxes[i].shim == shim)
            return registry_boxes[i].name;
    const char *late = registry_late_name_for_shim(shim);
    return late ? late : "?unknown-box?";
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
    /*
     * A place that has been removed but not yet reclaimed is not free
     * (issue 216). Its record still carries the slot count and return
     * size a task being built right now needs, and the sweep that
     * clears them is what makes the place available. Placing here
     * before then would have the sweep clear the *new* station's
     * record.
     */
    if (station >= 0 && station < m->n_stations
        && atomic_load_explicit(&m->stations[station].removed,
                                memory_order_acquire)) {
        fprintf(stderr,
                "map: station %d was removed and is not reclaimed yet — "
                "something may still be inside a task built from it\n",
                station);
        abort();
    }

    const box_info_t *b = registry_find(box_name);
    if (!b) {
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
        b = registry_recover_box(box_name);
    }
    if (!b) {
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
    if (extra && b->compare == NULL) {
        /* Routing on raw bytes would produce an answer, and it would
         * be wrong (issue 503). The refusal names the type, because
         * the fix is writing that type's __compare. Issue 503 wanted
         * this at build time; the generator never sees the map, so
         * placement is the earliest moment it can land — noted there
         * and in issue 604. */
        fprintf(stderr,
                "map: '%s' cannot be a comparator — its return type '%s' has "
                "no compare function; write %s__compare in a box source\n",
                box_name, b->return_type, b->return_type);
        abort();
    }

    int n = b->n_params + extra;
    int sizes[n > 0 ? n : 1];
    for (int i = 0; i < b->n_params; i++)
        sizes[i] = b->params[i].size;
    if (extra)
        sizes[b->n_params] = b->return_size;

    map_place(m, station, b->shim, kind, n, sizes, b->return_size);

    /* Registry placement knows what hand placement cannot: the type
     * each slot feeds, as text. This is what lets a static entry's
     * text become bytes of the right shape (issue 401), and what the
     * wire checker will compare in phase 6. The comparator's extra
     * slot is typed to the return value, since that is what it will
     * be compared against. */
    station_t *s = &m->stations[station];
    for (int i = 0; i < b->n_params; i++)
        s->slots[i].type_name = b->params[i].type_name;
    if (extra) {
        s->slots[b->n_params].type_name = b->return_type;
        /* Resolved once, here, so the delivery path compares with a
         * call rather than a lookup (issue 503). */
        s->compare = b->compare;
    }
}
/* }}} */
