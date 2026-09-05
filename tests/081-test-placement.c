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
#include "cera.h"

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
static int same_station(const char *box, const cera_station_t *a,
                        const cera_station_t *b)
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
        const cera_in_port_t *pa = &a->in_ports[i];
        const cera_in_port_t *pb = &b->in_ports[i];
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
static int place_both_ways(const cera_box_place_t *bp, int kind)
{
    cera_map_t *m = cera_map_create(2);
    cera_map_place_box(m, 0, bp->name, kind);     /* found by name */
    bp->place(m, 1, kind);                   /* called directly */

    int ok = same_station(bp->name, cera_map_station(m, 0), cera_map_station(m, 1));
    cera_map_destroy(m);
    return ok;
}
/* }}} */

int main(void)
{
    if (n_box_places <= 0) {
        fprintf(stderr, "no placement functions were emitted at all\n");
        return 1;
    }

    /*
     * **The comparator half of this test is gone, and the reason is
     * that its subject went** (issue 311b).
     *
     * It used to place every box as a comparator too, skipping the
     * ones that cannot be one — a box returning nothing, or one whose
     * return type has no comparison. It knew which those were by
     * asking the box *record*, and the record has been deleted: every
     * number it held is written straight onto a station by the
     * placement function, so it was a copy nobody read twice.
     *
     * There is no longer any way to ask in advance, because the two
     * refusals live inside the placement function and fire when it is
     * called — which is right, and means a test cannot probe without
     * ending the program. Comparator placement is proven where
     * comparators are used, in the routing tests; the two refusals are
     * proven in the map file's gallery of refusals.
     *
     * What survives here is the claim worth keeping: **a name and the
     * function it resolves to build the same station.** A generator
     * that paired one box's name with another's placement function
     * would be caught by exactly this.
     */
    int plain = 0;
    for (int i = 0; i < n_box_places; i++) {
        place_both_ways(&box_places[i], CERA_STATION_PLAIN);
        plain++;
    }

    /* The table's two names must agree about which box they mean: the
     * bare name is what a map says today, the address is what it will
     * say, and a row where they disagree would send the two forms to
     * different functions. */
    for (int i = 0; i < n_box_places; i++) {
        const char *addr = box_places[i].address;
        const char *colon = strrchr(addr, ':');
        if (!colon || strcmp(colon + 1, box_places[i].name) != 0)
            complain(box_places[i].name,
                     "its address names a different function");
    }

    if (failures) {
        fprintf(stderr, "%d placement differences\n", failures);
        return 1;
    }
    printf("  %d boxes placed by name and by function, field for field "
           "identical\n", plain);
    return 0;
}
