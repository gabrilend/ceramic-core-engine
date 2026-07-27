/*
 * 044-phase-6-run-map.c — the one binary.
 *
 * What this is: the whole point of the project as a single program.
 * It takes a map file, loads it, and runs it. Three different map
 * files make it three different programs; the compiler is never
 * invoked between them.
 *
 * How it does it, in general terms: everything is the engine's —
 * this file only calls load, draws what the loader actually built
 * (from the in-memory station table, never from the file text, so
 * any disagreement between the two would show), explains which
 * stations the seed took and why the others were left, prints where
 * the loading time went, releases the workers, and waits for the
 * map to finish itself.
 */
#include "040-mapfile.h"

#include <stdio.h>
#include <string.h>

/* {{{ kind_letter() */
static char kind_letter(unsigned char kind)
{
    static const char letters[STATION_KIND_COUNT] = { 'p', 'c', 'i' };
    return kind < STATION_KIND_COUNT ? letters[kind] : '?';
}
/* }}} */

/* {{{ draw_map() */
/*
 * The loaded map, drawn from the table. What is shown is what the
 * engine will actually run — if this and the file ever disagree, the
 * loader has a bug nothing else would catch.
 */
static void draw_map(map_t *m)
{
    printf("the map, as built (drawn from the station table):\n");
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        const char *name = m->station_names ? m->station_names[i] : "?";
        printf("  [%d] %-10s %c  slots:", i, name, kind_letter(s->kind));
        for (int j = 0; j < s->n_slots; j++) {
            slot_t *sl = &s->slots[j];
            switch (sl->kind) {
            case SLOT_RING:
                printf(" %d:buffer(%s, depth %d)", j,
                       sl->type_name ? sl->type_name : "?",
                       map_slot_depth(m, i, j));
                break;
            case SLOT_STATIC:
                printf(" %d:static($%d)", j, sl->static_id);
                break;
            case SLOT_GATHER:
                printf(" %d:gathers[%s]", j,
                       m->station_names ? m->station_names[sl->source] : "?");
                break;
            }
        }
        if (s->n_slots == 0)
            printf(" none");
        printf("\n");

        int port_index = 0;
        for (port_t *p = s->ports; p; p = p->next, port_index++) {
            for (destination_t *d = p->destinations; d; d = d->next)
                printf("        port %d -> %s.%d\n", port_index,
                       m->station_names ? m->station_names[d->station] : "?",
                       d->slot);
        }
    }
}
/* }}} */

/* {{{ explain_seed() */
/* Which stations the seed took, and why each of the others was
 * left — the gatherer that passes its readiness check vacuously and
 * is still correctly skipped is the subtle one (issue 606). */
static void explain_seed(map_t *m)
{
    printf("the seed, explained by counting (%d enqueued):\n",
           map_seed_count(m));
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = &m->stations[i];
        const char *name = m->station_names ? m->station_names[i] : "?";

        int has_ring = 0;
        for (int j = 0; j < s->n_slots; j++)
            if (s->slots[j].kind == SLOT_RING)
                has_ring = 1;
        int is_pulled = 0;
        for (int k = 0; k < m->n_stations && !is_pulled; k++)
            for (int j = 0; j < m->stations[k].n_slots; j++)
                if (m->stations[k].slots[j].kind == SLOT_GATHER
                    && m->stations[k].slots[j].source == i)
                    is_pulled = 1;

        if (has_ring)
            printf("  %-10s waits: buffered inputs mean delivery will "
                   "discover it\n", name);
        else if (is_pulled)
            printf("  %-10s skipped: gathered on demand — vacuously ready, "
                   "but its value belongs inside someone's task, and at "
                   "startup nobody is assembling one\n", name);
        else
            printf("  %-10s SEEDED: nothing can ever be written into it, so "
                   "if it is to run at all, it starts here\n", name);
    }
}
/* }}} */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: run-map <map-file> [--quiet]\n");
        return 1;
    }
    int quiet = argc > 2 && strcmp(argv[2], "--quiet") == 0;

    map_t *m = map_load_file(argv[1], 0);

    if (!quiet) {
        printf("\n");
        draw_map(m);
        printf("\n");
        explain_seed(m);
        printf("\nwhere the loading time went:\n");
        printf("  parse        %8.1f us\n", map_load_last_timing.parse * 1e6);
        printf("  first pass   %8.1f us\n", map_load_last_timing.first_pass * 1e6);
        printf("  second pass  %8.1f us\n", map_load_last_timing.second_pass * 1e6);
        printf("  validation   %8.1f us\n", map_load_last_timing.validation * 1e6);
        printf("  seed         %8.1f us\n", map_load_last_timing.seed * 1e6);
        printf("\n");
    }

    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);
    return 0;
}
