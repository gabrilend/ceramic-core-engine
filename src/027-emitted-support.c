/*
 * 027-emitted-support.c — walking what the generator wrote.
 *
 * What this is: the hand-written half of what the generator emits — lookups,
 * printing, and placement-by-name. The data it walks is generated at
 * build time from the box sources; this file never changes when a
 * box does, which is the entire division of labor.
 *
 * How it does it, in general terms: linear walks over two small
 * const arrays. A program has dozens of boxes, not millions, and a
 * lookup happens at load time, not on the delivery path; simplicity
 * wins over any table cleverness.
 */
#include "026-emitted.h"

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
const box_place_t *late_recover_box(const char *name);
const box_place_t *late_place_find(const char *name);
const char        *late_source_text(const char *path);

/* {{{ box_place_find() */
/*
 * Which generated placement function writes this box's station
 * (issue 311b). Compiled-in rows first, then anything that arrived
 * after the program started.
 *
 * That order is deliberate and it was the record's rule before it was
 * this one's: a box the program was built with wins over one added
 * afterwards under the same name, so bringing in new code can never
 * quietly replace something a map already depends on. Replacing a
 * name that was never built in works; shadowing one that was does not.
 *
 * **This is the whole of by-name placement, and now the whole of
 * by-name anything** (issue 311b). The record that used to sit beside
 * this table is gone: it held a name, a shim, parameter sizes and type
 * names, a return type, a task size and a comparison — and every one
 * of those is written directly onto the station by the placement
 * function, from a `sizeof` the compiler folded, so the record was a
 * copy of numbers nobody read twice.
 *
 * A placement function is hand placement written by the generator, so
 * naming a box is only a way of finding which one to call — and once
 * the generator reads maps itself it emits the call directly and this
 * lookup stops existing too (issue 311d).
 */
/*
 * **Three ways to say which box, and they are one rule** (issue
 * 311a): a bare function name; a basename and a function; a path and
 * a function.
 *
 * The path is not a fallback — it is a more specific way of saying
 * the same thing, so nobody has to guess which form is the real one
 * and an author who prefers paths everywhere is not fighting the
 * format.
 *
 * One function rather than a loop body, because the compiled-in rows
 * and the rows that arrived while the program ran are searched
 * separately and must agree about what a name means. They did not,
 * briefly, and the symptom was a dump that could not shorten an
 * address it had just written.
 */
int box_place_matches(const box_place_t *row, const char *name)
{
    if (!strchr(name, ':'))
        return strcmp(row->name, name) == 0;

    /*
     * An address. The whole thing matches exactly, or the part before
     * the colon is a *basename* — `math.c:add` finding
     * `src/boxes/math.c:add`. Matching against a suffix of the path is
     * what makes the brief form work, and requiring the slash before
     * it is what stops `path.c` matching `mypath.c`.
     */
    if (strcmp(row->address, name) == 0)
        return 1;
    size_t n = strlen(name);
    size_t a = strlen(row->address);
    return a > n && row->address[a - n - 1] == '/'
           && strcmp(row->address + a - n, name) == 0;
}

const box_place_t *box_place_find(const char *name)
{
    if (!name || !*name)
        return NULL;
    for (int i = 0; i < n_box_places; i++)
        if (box_place_matches(&box_places[i], name))
            return &box_places[i];
    return late_place_find(name);
}
/* }}} */

/* {{{ struct_text_find() */
/*
 * One type's reader and writer, by name (issue 408). Asked at
 * placement, so that a port holding a struct constant is *handed* its
 * pair — the same way a station is handed its shim and its comparison
 * — and nothing searches anything afterwards.
 */
const struct_text_t *struct_text_find(const char *type_name)
{
    if (!type_name)
        return NULL;
    for (int i = 0; i < n_struct_texts; i++)
        if (struct_texts[i].name
            && strcmp(struct_texts[i].name, type_name) == 0)
            return &struct_texts[i];
    return NULL;
}
/* }}} */

/* {{{ struct_find() */
const struct_info_t *struct_find(const char *type_name)
{
    for (int i = 0; i < n_struct_layouts; i++)
        if (strcmp(struct_layouts[i].name, type_name) == 0)
            return &struct_layouts[i];
    return NULL;
}
/* }}} */

/* {{{ emitted_print() */
/*
 * **What a program can place, and where each one came from.**
 *
 * It used to print every field of every box record — parameter types
 * and sizes, the return type, the task size, whether a comparison
 * existed. None of that is carried any more (issue 311b): the numbers
 * are folded into placement functions and the names live in the box
 * source the binary carries. What is left to print is what is left to
 * know: which names a program answers to, and which file each one was
 * compiled from.
 */
void emitted_print(FILE *out)
{
    fprintf(out, "emitted: %d boxes, %d structs\n",
            n_box_places, n_struct_layouts);
    for (int i = 0; i < n_box_places; i++)
        fprintf(out, "  %-20s %s\n", box_places[i].name,
                box_places[i].address);
    for (int i = 0; i < n_struct_layouts; i++) {
        const struct_info_t *s = &struct_layouts[i];
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

/* {{{ box_source_text() */
/*
 * **The C one box source was compiled from** (issue 311c), by the
 * path the build knew it as.
 *
 * A bare basename matches too, because that is how a person refers to
 * a file they can see — `029-demo-boxes.c` rather than
 * `src/boxes/029-demo-boxes.c`. Where two sources share a basename
 * the full path is the way to say which, exactly as it is for
 * addressing a box.
 */
const char *box_source_text(const char *path)
{
    if (!path || !*path)
        return NULL;

    for (int i = 0; i < sora_n_box_sources; i++)
        if (strcmp(sora_box_sources[i].path, path) == 0)
            return sora_box_sources[i].text;

    /* Then by basename, for somebody who typed what they could see. */
    for (int i = 0; i < sora_n_box_sources; i++) {
        const char *slash = strrchr(sora_box_sources[i].path, '/');
        const char *base = slash ? slash + 1 : sora_box_sources[i].path;
        if (strcmp(base, path) == 0)
            return sora_box_sources[i].text;
    }

    /*
     * **And then what has arrived since** (issue 311d). The build is
     * only the first iteration; a program that has been handed code
     * since is made of more than the build compiled, and asking what
     * it is made of has to get all of it.
     *
     * The build is asked first so that a source compiled in wins over
     * a later one of the same path — the compiled-in copy is what the
     * program's own stations were built from, and the answer should
     * describe the program rather than the last thing that happened
     * to it.
     */
    return late_source_text(path);
}
/* }}} */

/* {{{ map_build_find() */
/*
 * **The compiled form of one description** (issue 311d), by the path
 * the build knew it as or by the bare name somebody would type — the
 * same two ways a box source is found, for the same reason.
 */
const map_build_t *map_build_find(const char *path)
{
    if (!path || !*path)
        return NULL;

    for (int i = 0; i < sora_n_map_builds; i++)
        if (strcmp(sora_map_builds[i].path, path) == 0)
            return &sora_map_builds[i];

    for (int i = 0; i < sora_n_map_builds; i++) {
        const char *slash = strrchr(sora_map_builds[i].path, '/');
        const char *base = slash ? slash + 1 : sora_map_builds[i].path;
        if (strcmp(base, path) == 0)
            return &sora_map_builds[i];
    }
    return NULL;
}
/* }}} */

/* {{{ map_place_box() */
void map_place_box(map_t *m, int station, const char *box_name, int kind)
{
    /*
     * A place that has been removed but not yet reclaimed is not free
     * (issue 216). Its record still carries the port count and return
     * size a task being built right now needs, and the sweep that
     * clears them is what makes the place available. Placing here
     * before then would have the sweep clear the *new* station's
     * record.
     */
    if (station >= 0 && station < m->n_stations
        && atomic_load_explicit(&map_station(m, station)->removed,
                                memory_order_acquire)) {
        fprintf(stderr,
                "map: station %d was removed and is not reclaimed yet — "
                "something may still be inside a task built from it\n",
                station);
        abort();
    }

    const box_place_t *bp = box_place_find(box_name);
    if (!bp) {
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
        bp = late_recover_box(box_name);
    }
    if (!bp) {
        fprintf(stderr,
                "map: no box named '%s' anywhere the build could see — misspelled, or its "
                "source is not under src/boxes/\n", box_name);
        abort();
    }

    /*
     * **The station is written by the generated placement function**
     * (issue 311b), and by nothing else. Every number in it is a
     * `sizeof` the compiler folded into an immediate, so the sizes are
     * not read from anywhere at run time — they were computed while
     * the box was being compiled and never stored.
     *
     * **The two comparator refusals moved out of here** and into that
     * function, where they were already duplicated. A box that returns
     * nothing cannot be a comparator, because there is nothing to
     * compare; a box whose return type has no comparison cannot be
     * one, because routing on raw bytes would produce an answer and it
     * would be wrong (issue 503). Both are refused wherever a box is
     * placed from rather than only through this door, and the
     * generated version says *more* — it names the box by its full
     * address rather than by the bare word a map happened to use.
     *
     * That was the last thing this path did with the box record, and
     * with it gone the record has no readers left.
     */
    bp->place(m, station, kind);

    /*
     * The type each port feeds, the comparator's extra port, and the
     * comparison function are all written by the placement function
     * too — they used to be copied out of the record here, and that
     * was the last thing this path did with it.
     *
     * Type names are what let a static's text become bytes of the
     * right shape and what the wire checker reports; the comparison is
     * resolved at placement so the delivery path compares through a
     * pointer the station holds rather than looking anything up per
     * value. None of that changed. Only who writes it did.
     */
}
/* }}} */
