/*
 * 081-test-placement.c — the generated placement functions say the
 * same thing the box records say (issue 311b).
 *
 * What this is: the proof that a translation is faithful, taken while
 * both sides of it still exist. The generator has begun emitting a
 * function per box that writes a station directly, beside the record
 * that has always described one. Placement still routes through the
 * record. Before it stops doing so, the two have to be shown to
 * produce the same station — and the only moment that comparison is
 * cheap is while both are there to compare.
 *
 * How it does it, in general terms: for every box the generator
 * emitted, place it twice — once through the record and once through
 * its placement function — and compare every field a station keeps.
 * Not a sample of boxes and not a sample of fields: a difference in
 * either is a station that behaves differently depending on which
 * door it came through, which is the exact thing having two doors was
 * supposed to stop being possible.
 *
 * Comparator placement is checked too, wherever a box can be one,
 * because that is where the two paths do the most work — an extra
 * port appears, typed to the return value, and a comparison function
 * is resolved onto the station.
 */
#include "018-station.h"
#include "026-registry.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;

/* {{{ static void complain() */
static void complain(const char *box, const char *what)
{
    fprintf(stderr, "  %s: %s\n", box, what);
    failures++;
}
/* }}} */

/* {{{ static int same_station() */
/*
 * Every field placement writes, compared one at a time so a failure
 * names which one differs rather than only that something does.
 */
static int same_station(const char *box, const station_t *a,
                        const station_t *b)
{
    int ok = 1;
    if (a->call != b->call)        { complain(box, "different shim"); ok = 0; }
    if (a->kind != b->kind)        { complain(box, "different kind"); ok = 0; }
    if (a->out_size != b->out_size){ complain(box, "different return size"); ok = 0; }
    if (a->compare != b->compare)  { complain(box, "different comparison"); ok = 0; }
    if (a->n_in_ports != b->n_in_ports) {
        complain(box, "different port count");
        return 0;   /* comparing ports past here would read past one */
    }
    for (int i = 0; i < a->n_in_ports; i++) {
        const in_port_t *pa = &a->in_ports[i];
        const in_port_t *pb = &b->in_ports[i];
        if (pa->elem_size != pb->elem_size) {
            complain(box, "a port takes a different number of bytes");
            ok = 0;
        }
        /* The type name is compared by *content*, not by pointer: the
         * record's string and the placement function's literal are two
         * spellings of the same thing and need not be one address. */
        const char *ta = pa->type_name ? pa->type_name : "";
        const char *tb = pb->type_name ? pb->type_name : "";
        if (strcmp(ta, tb) != 0) {
            complain(box, "a port carries a different type name");
            ok = 0;
        }
    }
    return ok;
}
/* }}} */

/* {{{ static int place_both_ways() */
/*
 * Returns 1 when the box was placeable as this kind at all. A box
 * that returns nothing cannot be a comparator, and both paths refuse
 * it by stopping the program — which is the right behaviour and not
 * something a test can call twice, so those are skipped here and the
 * refusal itself is proven where refusals are proven.
 */
static int place_both_ways(const box_place_t *bp, int kind)
{
    const box_info_t *rec = registry_find(bp->name);
    if (!rec) {
        complain(bp->name, "the table names a box the registry does not");
        return 0;
    }
    if (kind == STATION_COMPARATOR
        && (rec->return_size == 0 || rec->compare == NULL))
        return 0;

    map_t *m = map_create(2);
    map_place_box(m, 0, bp->name, kind);     /* through the record */
    bp->place(m, 1, kind);                   /* through the function */

    int ok = same_station(bp->name, map_station(m, 0), map_station(m, 1));
    map_destroy(m);
    return ok;
}
/* }}} */

int main(void)
{
    if (registry_n_places <= 0) {
        fprintf(stderr, "no placement functions were emitted at all\n");
        return 1;
    }

    int plain = 0, comparators = 0;
    for (int i = 0; i < registry_n_places; i++) {
        place_both_ways(&registry_places[i], STATION_PLAIN);
        plain++;
        if (place_both_ways(&registry_places[i], STATION_COMPARATOR))
            comparators++;
    }

    /* The table's two names must agree about which box they mean: the
     * bare name is what a map says today, the address is what it will
     * say, and a row where they disagree would send the two forms to
     * different functions. */
    for (int i = 0; i < registry_n_places; i++) {
        const char *addr = registry_places[i].address;
        const char *colon = strrchr(addr, ':');
        if (!colon || strcmp(colon + 1, registry_places[i].name) != 0)
            complain(registry_places[i].name,
                     "its address names a different function");
    }

    if (failures) {
        fprintf(stderr, "%d placement differences\n", failures);
        return 1;
    }
    printf("  %d boxes placed both ways, field for field identical\n", plain);
    printf("  %d of them placed as comparators too, with the extra port "
           "and the comparison matching\n", comparators);
    return 0;
}
