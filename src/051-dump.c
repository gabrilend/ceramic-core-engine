/*
 * 051-dump.c — the loaded map, written back out as a map.
 *
 * What this is: issue 703. The station table rendered in the map
 * file format, so that loading a map and dumping it produces a file
 * equivalent to the one that went in. Round-tripping is the point:
 * if the dump and the original ever disagree, one of them is wrong,
 * and the disagreement is a loader bug nothing else would catch.
 * Once rewiring exists, the file on disk stops describing the
 * program — this becomes the only accurate description of what is
 * actually running.
 *
 * How it does it, in general terms: walks the table, never any
 * remembered text (except the statics entries' load-time text, which
 * is the one place bytes cannot be turned back into words — the
 * first-pass report carries that gap). Derived facts the file format
 * cannot say — types, sizes, indices, the gather depth — ride as
 * comments beside the lines that parse.
 */
#include "049-observe.h"
#include "026-registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ kind_letter() */
static char kind_letter(unsigned char kind)
{
    static const char letters[STATION_KIND_COUNT] = { 'p', 'c', 'i' };
    return kind < STATION_KIND_COUNT ? letters[kind] : '?';
}
/* }}} */

/* {{{ map_dump() */
void map_dump(map_t *m, FILE *out)
{
    /*
     * **Every station needs a name**, because a station line begins
     * with one and a file that begins a line with nothing does not
     * read back.
     *
     * This used to ask whether the map had *any* names, which was the
     * same question while reading a file was the only way to build a
     * program — the loader named all of them or none. A program built
     * by calling the construction surface can be named a station at a
     * time, and can have a station added after the rest were named
     * (issue 212), so the question is now asked per station.
     */
    for (int i = 0; i < m->n_stations; i++) {
        if (!map_station(m, i)->call)
            continue;   /* an empty place is not a station */
        if (i < m->n_named && m->station_names && m->station_names[i])
            continue;
        fprintf(stderr,
                "dump: station %d has no name — a station line begins with "
                "one, so this program cannot be written as a file that reads "
                "back\n", i);
        abort();
    }

    /*
     * **The names written out are made unique, and the ones on the
     * program are left alone** (issue 217).
     *
     * A station name is an arbitrary label the engine never reads;
     * two stations in one program may share one and nothing about the
     * program is worse for it. Bringing one description inside another
     * twice produces exactly that — two copies of every station the
     * description names, including its doors.
     *
     * A *file* cannot have two, and the reason is not fussiness: an
     * arrow is written as a destination name, so a file with two
     * `gate` lines cannot say which `gate` an arrow means. The parser
     * refuses one, correctly.
     *
     * So the disambiguation happens here, where it is needed, and
     * nothing is lost by it — a label carrying no meaning can be
     * spelled differently without the program changing. What comes
     * back from reading such a file is the same graph with different
     * labels on some of its stations, which is the same program by
     * every measure this project has.
     *
     * The suffix separator is a character the parser will accept
     * inside a name and never confuse for anything else. It cannot be
     * a dot: an arrow destination is split on its *last* dot to find
     * the port, so `gate.2` would read as station `gate`, port 2.
     */
    char **written = calloc((size_t)(m->n_stations > 0 ? m->n_stations : 1),
                            sizeof *written);
    if (!written) {
        fprintf(stderr, "dump: out of memory naming stations\n");
        abort();
    }
    for (int i = 0; i < m->n_stations; i++) {
        if (!map_station(m, i)->call)
            continue;
        const char *want = m->station_names[i];
        char candidate[128];
        snprintf(candidate, sizeof candidate, "%s", want);
        for (int attempt = 2; ; attempt++) {
            int taken = 0;
            for (int j = 0; j < i; j++)
                if (written[j] && strcmp(written[j], candidate) == 0)
                    taken = 1;
            if (!taken)
                break;
            snprintf(candidate, sizeof candidate, "%s~%d", want, attempt);
        }
        written[i] = strdup(candidate);
        if (!written[i]) {
            fprintf(stderr, "dump: out of memory naming stations\n");
            abort();
        }
    }

    fprintf(out, "# dumped from the live station table — what the engine is\n");
    fprintf(out, "# actually running, which is not necessarily what any file\n");
    fprintf(out, "# said. derived facts appear as comments.\n");

    /*
     * There is no statics section any more, and its absence is the
     * dump getting *more* accurate rather than less.
     *
     * The file format's statics section is notation — a way to write a
     * value down once while describing a map and point ports at it by
     * number. The engine used to keep that table alive, so the dump
     * echoed the text each entry was given and wrote the numbers back.
     * That had a hole in it, admitted in its own comment: a value
     * changed while the program ran was not re-serialized, so the dump
     * printed what the file had said rather than what the engine was
     * holding.
     *
     * With each value living on the port that reads it (issue 401),
     * every constant is written out beside its port, from its bytes,
     * by the formatter that mirrors the reader. What comes out is what
     * is actually there — including anything a runtime write changed
     * — which is the whole reason the dump exists.
     *
     * Two ports that shared an entry in the original file dump as two
     * ports each holding their own copy, because that is what they now
     * are. A file that goes in with sharing comes out without it, and
     * reloading gives the same program: the sharing was never
     * observable in behaviour, only in notation.
     */
    for (int i = 0; i < m->n_stations; i++) {
        station_t *s = map_station(m, i);
        fprintf(out, "\n%s ", written[i]);
        /*
         * The station knows its own name (issue 311b): the generated
         * placement function wrote it as a literal. This used to scan
         * every box record for one whose call site matched — the
         * registry read backwards, a linear search to answer a
         * question the station could just have been told.
         *
         * **A station placed by hand with no name given has none**,
         * and saying so is more honest than inventing a spelling: a
         * program built that way is not described on disk either, so
         * there is nothing a station line could truthfully say. The
         * marker is deliberately not a legal box name, so a dump
         * carrying one cannot be read back in silence.
         */
        /* The door, if it is one (issues 209, 213). A program whose
         * doors did not survive being written down could not be
         * composed after a round trip, which is most of what naming
         * them was for. */
        const char *door = s->door == DOOR_IN  ? " entry"
                         : s->door == DOOR_OUT ? " result"
                         : "";
        fprintf(out, "%s %c%s   # station %d\n",
                s->box_name ? s->box_name : "?placed-by-hand?",
                kind_letter(s->kind), door, i);

        for (int j = 0; j < s->n_in_ports; j++) {
            in_port_t *sl = &s->in_ports[j];

            /*
             * A starting depth, written only when it differs from the
             * default (issue 210b). The format writes exceptions, and
             * a port at the default depth is not one — saying `x10` on
             * every line would be noise a reader learns to skip.
             *
             * It comes before the source because the inline value form
             * runs to the end of the line, so nothing can follow it.
             */
            char depth[32] = "";
            if (sl->capacity != IN_PORT_DEFAULT_CAPACITY)
                snprintf(depth, sizeof depth, "x%d ", sl->capacity);

            switch (sl->kind) {
            case IN_PORT_STATIC: {
                /* The value itself, spoken from its bytes rather than
                 * echoed from remembered text (issue 401) — so a
                 * constant a runtime write changed dumps as what it
                 * now is, which the old table-and-number form could
                 * not do.
                 *
                 * Asked for its length first and then written, because
                 * a struct constant has no useful upper bound and a
                 * fixed buffer would quietly truncate exactly the
                 * values most worth reading. */
                int wanted = in_port_constant_text(sl, NULL, 0);
                char *text = malloc((size_t)wanted + 1);
                if (!text) {
                    fprintf(stderr, "dump: out of memory writing a constant\n");
                    abort();
                }
                in_port_constant_text(sl, text, wanted + 1);
                fprintf(out, "  in %d %s= %s   # %s, %d bytes\n", j, depth,
                        text, sl->type_name ? sl->type_name : "?",
                        sl->elem_size);
                free(text);
                break;
            }
            case IN_PORT_RING:
                /* The default source; the format writes only
                 * exceptions, but the derived facts still deserve
                 * saying — and a non-default depth is an exception, so
                 * it gets a line of its own rather than a comment. */
                if (depth[0])
                    /* The depth alone, with no source after it. It
                     * used to write a dash here, which reads back as
                     * a port with *no source* — so a program with a
                     * deepened buffer could be written down and not
                     * read in again. */
                    fprintf(out, "  in %d %s  # buffer, %s, %d bytes\n",
                            j, depth, sl->type_name ? sl->type_name : "?",
                            sl->elem_size);
                else
                    fprintf(out,
                            "  # port %d: buffer, %s, %d bytes, %d slots\n",
                            j, sl->type_name ? sl->type_name : "?",
                            sl->elem_size, sl->capacity);
                break;
            case IN_PORT_NONE:
                /*
                 * A port with no source at all, written as a bare dash
                 * (issue 210b). It is a state and not a value, so the
                 * station holding one simply never becomes ready.
                 *
                 * This used to be a comment saying the format had no
                 * word for it, which kept the dump honest at the cost
                 * of the round trip: a half-built program was one of
                 * the things a dump could not promise to reload. It
                 * can now.
                 */
                fprintf(out, "  in %d %s-   # no source yet, %s, %d bytes\n",
                        j, depth, sl->type_name ? sl->type_name : "?",
                        sl->elem_size);
                break;
            }
        }

        /* Written in array order, which is the order the wires were
         * drawn, so dump -> load -> dump produces the same text
         * without anybody arranging it (issue 214). Nothing in the
         * running engine reads that order or means anything by it. */
        int out_port_index = 0;
        for (out_port_t *p = s->out_ports; p; p = p->next, out_port_index++) {
            dest_set_t *set = out_port_dests(p);
            for (int di = 0; set && di < set->n; di++)
                fprintf(out, "  out %d - %s.%d\n", out_port_index,
                        written[set->items[di].station],
                        set->items[di].port);
        }
    }

    for (int i = 0; i < m->n_stations; i++)
        free(written[i]);
    free(written);
}
/* }}} */
