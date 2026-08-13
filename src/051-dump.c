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
    if (!m->station_names) {
        fprintf(stderr,
                "dump: this map was built by hand and has no names — only a "
                "loaded map can be written back as a file\n");
        abort();
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
        station_t *s = &m->stations[i];
        fprintf(out, "\n%s ", m->station_names[i]);
        /* The box's name is not stored on the station — the shim
         * pointer is read backwards through the registry. */
        fprintf(out, "%s %c   # station %d\n",
                registry_box_name_for_shim(s->call), kind_letter(s->kind), i);

        for (int j = 0; j < s->n_slots; j++) {
            slot_t *sl = &s->slots[j];
            switch (sl->kind) {
            case SLOT_STATIC: {
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
                int wanted = slot_constant_text(sl, NULL, 0);
                char *text = malloc((size_t)wanted + 1);
                if (!text) {
                    fprintf(stderr, "dump: out of memory writing a constant\n");
                    abort();
                }
                slot_constant_text(sl, text, wanted + 1);
                fprintf(out, "  in %d = %s   # %s, %d bytes\n", j, text,
                        sl->type_name ? sl->type_name : "?", sl->elem_size);
                free(text);
                break;
            }
            case SLOT_RING:
                /* The default; the format writes only exceptions,
                 * but the derived facts still deserve saying. */
                fprintf(out, "  # slot %d: buffer, %s, %d bytes, %d cells\n",
                        j, sl->type_name ? sl->type_name : "?",
                        sl->elem_size, sl->capacity);
                break;
            case SLOT_NONE:
                /* An unconfigured port, written as a comment because
                 * the format has no word for one yet — issue 210b owes
                 * that word and is blocked on issue 401, which is
                 * redesigning the line it would share with the static
                 * form.
                 *
                 * A comment is the honest placeholder rather than a
                 * good answer. Omitting the port would be the dump
                 * quietly lying: the file would load into a program
                 * with a *buffered* port where this one has none,
                 * which is a different program that happens to run.
                 * Saying it in a comment loses the round trip and
                 * keeps the truth, and losing the round trip is
                 * visible while a wrong program is not. The test that
                 * a dump of a dump is the dump still holds, because a
                 * comment survives being read and written again.
                 *
                 * Until the word exists, a half-built program is one
                 * of the things this file cannot promise to reload. */
                fprintf(out, "  # slot %d: NO SOURCE — %s, %d bytes; the "
                             "format cannot yet write this, so reloading "
                             "this file gives a buffer here instead\n",
                        j, sl->type_name ? sl->type_name : "?", sl->elem_size);
                break;
            }
        }

        int port_index = 0;
        for (port_t *p = s->ports; p; p = p->next, port_index++)
            for (destination_t *d = p->destinations; d; d = d->next)
                fprintf(out, "  out %d - %s.%d\n", port_index,
                        m->station_names[d->station], d->slot);
    }
}
/* }}} */
