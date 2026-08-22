/*
 * 069-genemit.c — the description becomes C.
 *
 * What this is: the half of the generator that writes. Four kinds of
 * output, in the order the engine's header expects to find them: a
 * three-way comparison per orderable return type (issue 305), a shim
 * per box (302), a field table per struct (304), and the placement functions
 * itself (303).
 *
 * How it does it, in general terms: every size and every offset is
 * emitted as a `sizeof` or `offsetof` expression rather than as a
 * number, so the C compiler computes all of them and this program
 * never guesses about padding or alignment. That is guarantee C1 and
 * it is the reason a box compiled later can wire into a box compiled
 * earlier — both numbers came from the same place.
 *
 * The output is built whole in memory and written to a temporary name,
 * then moved into place only on success. A generator that dies partway
 * must never leave yesterday's emission, or half of today's, where a
 * build can find it (issue 306).
 */
#include "067-genparse.h"
/* The map reader, so the generator can compile a description into
 * the calls it describes (issue 311d). */
#include "040-mapfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static const char *field_kind_name() */
static const char *field_kind_name(tkind_t k)
{
    switch (k) {
    case TKIND_INT:    return "FIELD_INT";
    case TKIND_UINT:   return "FIELD_UINT";
    case TKIND_FLOAT:  return "FIELD_FLOAT";
    case TKIND_STRING: return "FIELD_STRING";
    case TKIND_STRUCT: return "FIELD_STRUCT";
    default:           return "FIELD_INT";
    }
}
/* }}} */

/* {{{ static int struct_index() */
static int struct_index(const description_t *d, const sdef_t *s)
{
    for (int i = 0; i < d->structs.n; i++)
        if (vec_at(&d->structs, i) == (const void *)s)
            return i;
    return 0;
}
/* }}} */

/* {{{ static int struct_index_by_name() */
/*
 * Which row of the emitted struct table describes this type, or -1
 * when the type is not a struct at all. Used to hand a port its field
 * table's address at placement, so that reading a written-out
 * constant follows a pointer instead of searching the table by name
 * (issue 311b).
 */
static int struct_index_by_name(const description_t *d, const char *type)
{
    for (int i = 0; i < d->structs.n; i++) {
        const sdef_t *s = vec_at(&d->structs, i);
        if (strcmp(s->name, type) == 0)
            return i;
    }
    return -1;
}
/* }}} */

/* {{{ static void emit_compares() */
/*
 * A comparison per return type that some box produces, emitted once
 * each. Every one copies the bytes into real typed variables before
 * comparing, because raw-byte comparison reads negative floats
 * backwards — a negative float looks larger than a positive one when
 * you compare the bytes (issue 305).
 *
 * A struct return with no author-written comparison simply gets a
 * null in its row. The refusal happens at load, when a
 * comparator actually asks for one (issue 604), because a box whose
 * value nobody ever compares is perfectly legal.
 */
static void emit_compares(buf_t *w, const description_t *d,
                          const char **compare_of)
{
    for (int i = 0; i < d->boxes.n; i++) {
        const box_t *b = vec_at(&d->boxes, i);
        const char *t = b->ret;
        if (strcmp(t, "void") == 0 || compare_of[i])
            continue;

        /* Has this type already been handled by an earlier box? */
        const char *already = NULL;
        for (int j = 0; j < i; j++) {
            const box_t *e = vec_at(&d->boxes, j);
            if (compare_of[j] && strcmp(e->ret, t) == 0) {
                already = compare_of[j];
                break;
            }
        }
        if (already) {
            compare_of[i] = already;
            continue;
        }
        /* Or already looked at and found to have none? */
        int seen = 0;
        for (int j = 0; j < i; j++) {
            const box_t *e = vec_at(&d->boxes, j);
            if (strcmp(e->ret, t) == 0)
                seen = 1;
        }
        if (seen)
            continue;

        char *sym = NULL;
        if (gp_primitive_kind(t, NULL)) {
            sym = gt_mangle(d->arena, t);
            char *full = arena_alloc(d->arena, strlen(sym) + 16);
            sprintf(full, "%s__compare_g", sym);
            sym = full;
            buf_line(w, "/* three-way compare for %s, generated (issue 305) */", t);
            buf_line(w, "static int %s(const void *a, const void *b)", sym);
            buf_line(w, "{");
            buf_line(w, "    %s x, y;", t);
            buf_line(w, "    memcpy(&x, a, sizeof x);");
            buf_line(w, "    memcpy(&y, b, sizeof y);");
            buf_line(w, "    return (x > y) - (x < y);");
            buf_line(w, "}");
            buf_line(w, "");
        } else if (gp_compare_for(d, t)) {
            sym = gt_mangle(d->arena, t);
            char *full = arena_alloc(d->arena, strlen(sym) + 16);
            sprintf(full, "%s__compare_g", sym);
            sym = full;
            buf_line(w, "/* wrapper over the author's %s__compare (issue 305) */", t);
            buf_line(w, "static int %s(const void *a, const void *b)", sym);
            buf_line(w, "{");
            buf_line(w, "    %s x, y;", t);
            buf_line(w, "    memcpy(&x, a, sizeof x);");
            buf_line(w, "    memcpy(&y, b, sizeof y);");
            buf_line(w, "    return %s__compare(x, y);", t);
            buf_line(w, "}");
            buf_line(w, "");
        }
        compare_of[i] = sym;
    }
}
/* }}} */

/* {{{ static void emit_shims() */
/*
 * The per-box call site. Loads are memcpy and never pointer casts:
 * the task's value area packs values back to back, so a double
 * following an int sits misaligned, and memcpy is how that stays
 * defined behaviour everywhere (issue 302).
 */
static void emit_shims(buf_t *w, const description_t *d)
{
    for (int i = 0; i < d->boxes.n; i++) {
        const box_t *b = vec_at(&d->boxes, i);
        buf_line(w, "/* generated from: %s %s(...) [%s:%d] */",
                 b->ret, b->name, b->file, b->line);
        buf_line(w, "static void %s__call(task_t *t)", b->name);
        buf_line(w, "{");
        buf_line(w, "    (void)t;");
        for (int j = 0; j < b->n_params; j++) {
            buf_line(w, "    %s a%d;", b->params[j].type, j);
            buf_line(w, "    memcpy(&a%d, t->in[%d], sizeof a%d);", j, j, j);
        }

        /* Timing exists only under SORA_STATS (issue 702): the clock
         * reads compile out entirely otherwise, so measurement is a
         * choice made at build time and never a standing tax. */
        buf_line(w, "#ifdef SORA_STATS");
        buf_line(w, "    struct timespec sora_t0, sora_t1;");
        buf_line(w, "    clock_gettime(CLOCK_MONOTONIC, &sora_t0);");
        buf_line(w, "#endif");

        buf_addstr(w, "    ");
        if (strcmp(b->ret, "void") != 0)
            buf_addf(w, "%s r = ", b->ret);
        buf_addf(w, "%s(", b->name);
        for (int j = 0; j < b->n_params; j++)
            buf_addf(w, "%sa%d", j ? ", " : "", j);
        buf_line(w, ");");

        buf_line(w, "#ifdef SORA_STATS");
        buf_line(w, "    clock_gettime(CLOCK_MONOTONIC, &sora_t1);");
        /* The time is charged onto the task, and the delivery walk
         * moves it onto the station afterwards. It used to be charged
         * straight to the station, found through a process-wide
         * pointer to the running map, and that pointer was what
         * limited a process to a single running map (issue 405). */
        buf_line(w, "    sora_stats_box_time(t,");
        buf_line(w, "        (sora_t1.tv_sec - sora_t0.tv_sec) * 1000000000L");
        buf_line(w, "        + (sora_t1.tv_nsec - sora_t0.tv_nsec));");
        buf_line(w, "#endif");

        if (strcmp(b->ret, "void") != 0)
            buf_line(w, "    memcpy(t->out, &r, sizeof r);");
        buf_line(w, "}");
        buf_line(w, "");
    }
}
/* }}} */

/* {{{ static void emit_structs() */
/*
 * One field table per struct, with every offset an `offsetof` and
 * every size a `sizeof`, so a struct with a padding hole reads
 * correctly without this program knowing anything about padding
 * (issue 304). Nested structs point into the shared array declared
 * extern in the emitted-shapes header.
 */
static void emit_structs(buf_t *w, const description_t *d)
{
    for (int i = 0; i < d->structs.n; i++) {
        const sdef_t *s = vec_at(&d->structs, i);
        buf_line(w, "static const field_info_t %s__fields[] = {", s->name);
        for (int j = 0; j < s->n_fields; j++) {
            const field_t *f = &s->fields[j];
            char nested[64];
            if (f->nested)
                snprintf(nested, sizeof nested, "&struct_layouts[%d]",
                         struct_index(d, f->nested));
            else
                snprintf(nested, sizeof nested, "NULL");
            buf_line(w,
                     "    { \"%s\", (int)offsetof(%s, %s), "
                     "(int)sizeof(((%s *)0)->%s), %s, %s, %d },",
                     f->name, s->name, f->name, s->name, f->name,
                     field_kind_name(f->kind), nested, f->array_len);
        }
        buf_line(w, "};");
        buf_line(w, "");
    }

    buf_line(w, "const struct_info_t struct_layouts[] = {");
    for (int i = 0; i < d->structs.n; i++) {
        const sdef_t *s = vec_at(&d->structs, i);
        buf_line(w, "    { \"%s\", (int)sizeof(%s), %d, %s__fields },",
                 s->name, s->name, s->n_fields, s->name);
    }
    if (d->structs.n == 0)
        buf_line(w, "    { NULL, 0, 0, NULL },");
    buf_line(w, "};");
    buf_line(w, "const int n_struct_layouts = %d;", d->structs.n);
    buf_line(w, "");
}
/* }}} */

/* {{{ the box record's emitter, deleted — issue 311b */
/*
 * **A record per box was emitted here and is not any more.**
 *
 * It held a name, a shim pointer, parameter type names and sizes, a
 * return type and size, the exact task allocation, and a comparison
 * function — and a station was built by reading it once at placement
 * and copying out the parts a station keeps.
 *
 * Every one of those is written straight onto the station by that
 * box's placement function now, from a `sizeof` the compiler folds
 * into an immediate. The record was a copy of numbers nobody read
 * twice, and emitting it meant the generator produced two
 * descriptions of one box that had to agree.
 *
 * What survives is the placement table below: a name, an address, and
 * a function pointer. Once the generator reads maps itself it emits
 * the calls directly and that goes too (issue 311d).
 */
/* }}} */

/* {{{ static const char *path_within() */
/*
 * A box's path, shortened against the project root, so that a symbol
 * does not carry the absolute path of whatever machine ran the build.
 * A generated file that differs by *where* it was built is one nobody
 * can compare against another.
 *
 * Returns the whole path when there is no root or it does not match,
 * which is the case for a box compiled while a program runs: it has
 * no project root to be relative to, and its path is already unique.
 */
static const char *path_within(const char *file, const char *root)
{
    if (!root || !*root)
        return file;
    size_t n = strlen(root);
    if (strncmp(file, root, n) == 0 && file[n] == '/')
        return file + n + 1;
    return file;
}
/* }}} */

/* {{{ static void emit_placements() */
/*
 * One **placement function** per box (issue 311b), emitted beside the
 * record rather than instead of it, so the two can be compared before
 * either is trusted.
 *
 * The observation this rests on: a box's record is read exactly once,
 * at placement, and never again. A station keeps its own shim, its own
 * slot sizes, its own return size and its own comparison, and the
 * station header is deliberately free of any reference back. So the
 * record exists only to be read at the one moment generated code could
 * just as well do the writing — and generated code writing it turns
 * every number into an expression the compiler folds into an
 * immediate, stored nowhere at all.
 *
 * **A placement function is hand placement, written by the generator
 * instead of by a person.** That is why there are not two doors into
 * the engine: placing by name is only a way of finding which generated
 * hand-placement to call.
 *
 * Every size stays a `sizeof`, so the compiler still computes every
 * number and the generator still never guesses one — the rule the
 * whole engine rests on is untouched by moving where they live.
 */
static void emit_placements(buf_t *w, const description_t *d,
                            const char **compare_of, arena_t *a,
                            const char *root)
{
    for (int i = 0; i < d->boxes.n; i++) {
        const box_t *b = vec_at(&d->boxes, i);
        int is_void = strcmp(b->ret, "void") == 0;
        const char *file = path_within(b->file, root);
        char *sym = gt_box_symbol(a, file, b->name);

        /* **Not static, and that is the whole of issue 311d step 7.**
         * A station-builder used to be private, which made it
         * invisible from outside the program and therefore impossible
         * for anything compiled later to bind to. A map arriving
         * mid-run is compiled into a shared object that calls these by
         * name; a shared object can only bind to a name the thing it
         * was loaded into published, so the program has to publish
         * them.
         *
         * One symbol per box is enough. It holds the box's shim and
         * the box body behind it, so the published list stays short
         * while the code stays reachable.
         *
         * The cost is admitted where it lands: a published symbol is a
         * root the section collector may not touch, so a program keeps
         * the boxes it was built with rather than shedding the ones it
         * never places. Extendable at run time is a requirement of
         * this engine; small was never one. */
        buf_line(w, "/* places %s:%s */", file, b->name);
        buf_line(w, "void %s__place(map_t *m, int station, int kind);", sym);
        buf_line(w, "void %s__place(map_t *m, int station, int kind)", sym);
        buf_line(w, "{");

        /* A comparator carries one extra port holding the value to
         * compare against, typed to the box's return value because
         * that is what it will be compared with. Both refusals say
         * which of the two reasons applies — "cannot be a comparator"
         * alone would leave the author guessing which fix to make. */
        if (is_void) {
            buf_line(w, "    if (kind == STATION_COMPARATOR) {");
            buf_line(w, "        fprintf(stderr, \"map: '%s:%s' cannot be a \"",
                     file, b->name);
            buf_line(w, "                \"comparator — it returns nothing, so \"");
            buf_line(w, "                \"there is nothing to compare\\n\");");
            buf_line(w, "        abort();");
            buf_line(w, "    }");
        } else if (!compare_of[i]) {
            buf_line(w, "    if (kind == STATION_COMPARATOR) {");
            buf_line(w, "        fprintf(stderr, \"map: '%s:%s' cannot be a \"",
                     file, b->name);
            buf_line(w, "                \"comparator — its return type '%s' has \"",
                     b->ret);
            buf_line(w, "                \"no compare function; write \"");
            buf_line(w, "                \"%s__compare in a box source\\n\");",
                     b->ret);
            buf_line(w, "        abort();");
            buf_line(w, "    }");
        }

        buf_line(w, "    int extra = (kind == STATION_COMPARATOR) ? 1 : 0;");
        /* The threshold slot is always in the array and counted only
         * when it is wanted, which also keeps the array from being
         * zero-length for a box that takes no parameters — something C
         * does not allow. */
        buf_addstr(w, "    int sizes[] = {");
        for (int j = 0; j < b->n_params; j++)
            buf_addf(w, " (int)sizeof(%s),", b->params[j].type);
        buf_addf(w, " (int)sizeof(%s) };", is_void ? "int" : b->ret);
        buf_line(w, "");

        char ret_size[128];
        if (is_void)
            snprintf(ret_size, sizeof ret_size, "0");
        else
            snprintf(ret_size, sizeof ret_size, "(int)sizeof(%s)", b->ret);

        buf_line(w, "    map_place(m, station, %s__call, kind, %d + extra,",
                 b->name, b->n_params);
        buf_line(w, "              sizes, %s);", ret_size);
        buf_line(w, "");
        buf_line(w, "    station_t *s = map_station(m, station);");
        buf_line(w, "    (void)s;");
        /* The name the dump reads, written here rather than found by
         * scanning every box for a matching call site (issue 311b).
         * Bare today because that is what a map file says; it becomes
         * the full address when the format carries one. */
        /*
         * **The full address, not the bare name** (issue 311a): the
         * file a box lives in and the function within it. A bare name
         * is not an address — it is a name in a namespace nobody wrote
         * down — and naming the file is the same information a reader
         * wants anyway when they go looking for what `add` does.
         *
         * The dump shortens it back to the bare name when that
         * resolves uniquely, so a map written briefly stays brief.
         */
        buf_line(w, "    s->box_name = \"%s:%s\";",
                 path_within(b->file, root), b->name);
        for (int j = 0; j < b->n_params; j++) {
            buf_line(w, "    s->in_ports[%d].type_name = \"%s\";",
                     j, b->params[j].type);
            /* The layout, handed over rather than looked for. A
             * written-out constant needs to know which field sits at
             * which offset, and the placement function knows the type
             * concretely — so the port is given the address instead of
             * searching a table by name (issue 311b). */
            int si = struct_index_by_name(d, b->params[j].type);
            if (si >= 0)
                buf_line(w, "    s->in_ports[%d].fields = &struct_layouts[%d];",
                         j, si);
        }
        if (!is_void && compare_of[i]) {
            int rsi = struct_index_by_name(d, b->ret);
            buf_line(w, "    if (extra) {");
            buf_line(w, "        s->in_ports[%d].type_name = \"%s\";",
                     b->n_params, b->ret);
            if (rsi >= 0)
                buf_line(w, "        s->in_ports[%d].fields = "
                            "&struct_layouts[%d];", b->n_params, rsi);
            /* Resolved once, here, so the delivery path compares
             * through a pointer the station already holds rather than
             * looking anything up per value. */
            buf_line(w, "        s->compare = %s;", compare_of[i]);
            buf_line(w, "    }");
        }
        buf_line(w, "}");
        buf_line(w, "");
    }

    /*
     * The two-column table that finds one of these by the name a map
     * writes. Temporary by design: once the generator reads maps
     * itself it emits the *calls*, and a placement function is reached
     * by being called rather than by being found (issue 311d). Until
     * then this is what by-name placement looks one up in — and it is
     * what keeps every emitted function referenced, which a build with
     * warnings as errors requires.
     */
    buf_line(w, "const box_place_t box_places[] = {");
    for (int i = 0; i < d->boxes.n; i++) {
        const box_t *b = vec_at(&d->boxes, i);
        const char *file = path_within(b->file, root);
        char *sym = gt_box_symbol(a, file, b->name);
        buf_line(w, "    { \"%s\", \"%s:%s\", %s__place },",
                 b->name, file, b->name, sym);
    }
    if (d->boxes.n == 0)
        buf_line(w, "    { NULL, NULL, NULL },");
    buf_line(w, "};");
    buf_line(w, "const int n_box_places = %d;", d->boxes.n);
    buf_line(w, "");
}
/* }}} */

/* {{{ static const box_t *box_named() */
/*
 * Which box a map line means, resolved **at build time, on the
 * author's machine, naming the map line** (issue 311d step 4).
 *
 * None of this was checkable before, because the build had never seen
 * a map. A name that matches nothing and a name that matches two
 * files are both stopped here rather than at somebody else's startup.
 *
 * A bare name is what a map line carries today. When the format
 * learns to say `file:function` (issue 311a) this gains a path to
 * settle a tie with, and the ambiguity below stops being fatal for
 * anybody who says which they meant.
 */
static const box_t *box_named(const description_t *d, const char *name,
                              const char *root,
                              const char *map_path, int line)
{
    /*
     * **Three forms, and the path is not a fallback** (issue 311a).
     * A bare function name, a basename and a function, or a path and
     * a function — the last being a more specific way of saying the
     * same thing, so nobody has to guess which form is the real one
     * and an author who prefers paths everywhere is not fighting the
     * format.
     */
    const char *colon = strrchr(name, ':');
    const char *want_fn = colon ? colon + 1 : name;

    const box_t *found = NULL;
    int matches = 0;
    for (int i = 0; i < d->boxes.n; i++) {
        const box_t *b = vec_at(&d->boxes, i);
        if (strcmp(b->name, want_fn) != 0)
            continue;
        if (colon) {
            /* The part before the colon must be the box's path, or a
             * suffix of it beginning at a slash — which is what makes
             * `math.c:add` reach `src/boxes/math.c:add` while
             * `path.c` cannot reach `mypath.c`. */
            const char *where = path_within(b->file, root);
            size_t want = (size_t)(colon - name);
            size_t have = strlen(where);
            int same = (have == want && strncmp(where, name, want) == 0)
                    || (have > want && where[have - want - 1] == '/'
                        && strncmp(where + have - want, name, want) == 0);
            if (!same)
                continue;
        }
        found = b;
        matches++;
    }

    if (matches == 0) {
        fprintf(stderr, "generator: %s:%d: nothing this build was given "
                        "answers to '%s'\n", map_path, line, name);
        exit(65);
    }
    if (matches > 1) {
        /*
         * **A collision is fatal, and it names both paths** (issue
         * 311a), because the author's fix is to write one of them out
         * in full and they cannot do that without being told which
         * two files are in question. It is not a warning and it does
         * not pick one.
         */
        fprintf(stderr, "generator: %s:%d: '%s' names a box in more than "
                        "one source, and the line does not say which:\n",
                map_path, line, name);
        for (int i = 0; i < d->boxes.n; i++) {
            const box_t *b = vec_at(&d->boxes, i);
            if (strcmp(b->name, want_fn) == 0)
                fprintf(stderr, "generator:   %s:%s\n",
                        path_within(b->file, root), b->name);
        }
        fprintf(stderr, "generator: write one of those out in full\n");
        exit(65);
    }
    return found;
}
/* }}} */

/* {{{ static void emit_maps() */
/*
 * **A map compiled into the calls it describes** (issue 311d).
 *
 * The text was a thing a program parsed while it ran; it becomes a
 * blueprint for the compilation instead. Every box name in it is
 * resolved here, on the author's machine, and becomes a direct call
 * to that box's placement function — so no box name survives into the
 * running program, and a misspelled one fails the build rather than
 * somebody else's startup.
 *
 * **Station names do survive**, and that is not an inconsistency. A
 * box name is a question the engine had to answer at run time and no
 * longer does. A station name is *data the program carries about
 * itself*, so it can be written back out as a file that reads in
 * again — the same status the box's own name literal already has.
 *
 * **The emitted function records where each station landed**, exactly
 * as reading a file does, rather than assuming they are numbered from
 * zero. Adding a station hands back a freed place before it grows the
 * table, so a description built into a program that has had removals
 * gets whatever holes exist; the array is what makes the same
 * generated function work on an empty program and a crowded one.
 *
 * The order is the reader's order — create, name, place, mark a door,
 * set depths and sources, then draw every wire — so that a program
 * built from this and the same program read from the file are the
 * same program rather than two similar ones.
 */
static void emit_maps(buf_t *w, const description_t *d, arena_t *a,
                      const char **maps, int n_maps, const char *root,
                      int external_boxes)
{
    if (n_maps == 0) {
        buf_line(w, "/* No maps were named to this build (issue 311d). */");
        buf_line(w, "const map_build_t sora_map_builds[] = { { 0, 0 } };");
        buf_line(w, "const int sora_n_map_builds = 0;");
        buf_line(w, "");
        return;
    }

    /*
     * Two helpers the generated build functions lean on, so every
     * refusal is handled the same way and each emitted line stays one
     * call. Emitted only when there are maps, because a build with
     * warnings as errors rejects a function nobody calls.
     */
    /*
     * **When the boxes are already in the process, say so rather than
     * carrying them** (issue 311d). Every station-builder a map calls
     * is declared here and defined elsewhere — in the program this
     * file is about to be loaded into, which was built with those
     * boxes and published the functions that build stations from them.
     *
     * Declared once each, in the order the maps name them, skipping
     * repeats: a name declared twice is legal C but reads as though
     * something is uncertain about it.
     *
     * Naming a box the loading program does not actually hold fails at
     * load, naming the symbol. That is later than the build-time
     * refusal and it is the honest place for it: whether a function is
     * present in a program is not a question this generator can answer
     * about a program it is not part of.
     */
    if (external_boxes) {
        buf_line(w, "/* Built into the program this will be loaded into,");
        buf_line(w, " * which published them so this could bind (issue 311d). */");
        for (int mi = 0; mi < n_maps; mi++) {
            map_description_t *md = mapfile_parse(maps[mi]);
            for (desc_station_t *st = md->stations; st; st = st->next) {
                const box_t *b = box_named(d, st->box, root, maps[mi],
                                           st->line);
                char *place = gt_box_symbol(a, path_within(b->file, root),
                                            b->name);
                int said = 0;
                for (int pm = 0; pm <= mi && !said; pm++) {
                    map_description_t *earlier = pm == mi
                                              ? md : mapfile_parse(maps[pm]);
                    for (desc_station_t *e = earlier->stations;
                         e && !said; e = e->next) {
                        if (pm == mi && e == st)
                            break;
                        const box_t *eb = box_named(d, e->box, root,
                                                    maps[pm], e->line);
                        char *esym = gt_box_symbol(a,
                                        path_within(eb->file, root), eb->name);
                        if (strcmp(esym, place) == 0)
                            said = 1;
                    }
                    if (pm != mi)
                        mapfile_free(earlier);
                }
                if (!said)
                    buf_line(w, "void %s__place(map_t *m, int station, "
                                "int kind);", place);
            }
            mapfile_free(md);
        }
        buf_line(w, "");
    }

    buf_line(w, "/* A refusal from a generated build ends the program");
    buf_line(w, " * (issue 106): it is an invalid operation, and the caller");
    buf_line(w, " * is generated code with nothing better to decide. */");
    buf_line(w, "static void sora_built_take(const char *refusal)");
    buf_line(w, "{");
    buf_line(w, "    if (refusal)");
    buf_line(w, "        sora_stop_now(0, SORA_EXIT_BAD_CALL, refusal);");
    buf_line(w, "}");
    buf_line(w, "");

    for (int mi = 0; mi < n_maps; mi++) {
        map_description_t *md = mapfile_parse(maps[mi]);
        const char *shortened = path_within(maps[mi], root);
        char *sym = gt_box_symbol(a, shortened, "build");

        buf_line(w, "/* builds %s */", shortened);
        buf_line(w, "static void %s(map_t *m)", sym);
        buf_line(w, "{");
        buf_line(w, "    int at[%d];", md->n_stations > 0 ? md->n_stations : 1);

        int index = 0;
        for (desc_station_t *s = md->stations; s; s = s->next, index++) {
            const box_t *b = box_named(d, s->box, root, maps[mi], s->line);
            char *place = gt_box_symbol(a, path_within(b->file, root),
                                        b->name);
            const char *kind = s->kind == STATION_COMPARATOR
                             ? "STATION_COMPARATOR"
                             : s->kind == STATION_ITERATOR
                             ? "STATION_ITERATOR" : "STATION_PLAIN";

            buf_line(w, "    at[%d] = map_add_station(m);", index);
            buf_line(w, "    sora_built_take(map_name_station(m, at[%d], "
                        "\"%s\"));", index, s->name);
            buf_line(w, "    %s__place(m, at[%d], %s);", place, index, kind);
            if (s->door == DOOR_IN)
                buf_line(w, "    sora_built_take(map_designate_input(m, "
                            "at[%d]));", index);
            else if (s->door == DOOR_OUT)
                buf_line(w, "    sora_built_take(map_designate_output(m, "
                            "at[%d]));", index);

            for (desc_input_t *in = s->inputs; in; in = in->next) {
                if (in->depth > 0)
                    buf_line(w, "    sora_built_take(map_in_port_start_depth"
                                "(m, at[%d], %d, %d));",
                             index, in->port, in->depth);
                if (in->is_none) {
                    buf_line(w, "    sora_built_take(map_configure_port(m, "
                                "at[%d], %d, IN_PORT_NONE, 0));",
                             index, in->port);
                    continue;
                }
                const char *text = in->text;
                if (in->is_static) {
                    for (desc_static_t *e = md->statics; e; e = e->next)
                        if (e->id == in->static_id) {
                            text = e->text;
                            break;
                        }
                    if (!text) {
                        fprintf(stderr, "generator: %s:%d: 'in %d $%d' names "
                                        "a statics entry the file does not "
                                        "give a value for\n",
                                maps[mi], in->line, in->port, in->static_id);
                        exit(65);
                    }
                }
                if (!text)
                    continue;   /* a depth and nothing else */
                buf_addstr(w, "    sora_built_take(map_configure_port(m, "
                              "at[");
                buf_addf(w, "%d], %d, IN_PORT_STATIC, \"", index, in->port);
                for (const char *c = text; *c; c++) {
                    if (*c == '\\')     buf_addstr(w, "\\\\");
                    else if (*c == '"') buf_addstr(w, "\\\"");
                    else                buf_addch(w, *c);
                }
                buf_addstr(w, "\"));\n");
            }
        }

        /* Every wire, after every station exists — which is all that
         * survives of the reader's two passes. */
        index = 0;
        for (desc_station_t *s = md->stations; s; s = s->next, index++) {
            for (desc_output_t *out = s->outputs; out; out = out->next) {
                int dest = 0, found = -1;
                for (desc_station_t *t = md->stations; t; t = t->next, dest++)
                    if (strcmp(t->name, out->dest_station) == 0) {
                        found = dest;
                        break;
                    }
                if (found < 0) {
                    fprintf(stderr, "generator: %s:%d: arrow to '%s', which "
                                    "this map does not declare\n",
                            maps[mi], out->line, out->dest_station);
                    exit(65);
                }
                buf_line(w, "    sora_built_take(map_wire(m, at[%d], %d, "
                            "at[%d], %d));",
                         index, out->port, found, out->dest_port);
            }
        }

        buf_line(w, "}");
        buf_line(w, "");
        mapfile_free(md);
    }

    buf_line(w, "/* Which built function belongs to which description. */");
    buf_line(w, "const map_build_t sora_map_builds[] = {");
    for (int mi = 0; mi < n_maps; mi++) {
        const char *shortened = path_within(maps[mi], root);
        char *sym = gt_box_symbol(a, shortened, "build");
        buf_line(w, "    { \"%s\", %s },", shortened, sym);
    }
    buf_line(w, "};");
    buf_line(w, "const int sora_n_map_builds = %d;", n_maps);
    buf_line(w, "");
}
/* }}} */

/* {{{ ge_emit() */
void ge_emit(const description_t *d, const char **sources, int n_sources,
             const char **maps, int n_maps,
             const char *out_path, const char *root, int external_boxes);

/* {{{ static void emit_sources() */
/*
 * **Every box source, a second time, as text** (issue 311c).
 *
 * The generated file already `#include`s each source whole, so the
 * compiler can see the types and inline each box into its shim — and
 * the text is thrown away at that point, surviving only as compiled
 * code. So a running program could not say what its boxes look like,
 * and a program handed to somebody else was a binary that needed a
 * source tree beside it before it could do anything with new code.
 *
 * The method is chosen for being unremarkable. A C array in the
 * generated file is portable, needs no post-build step, and adds
 * nothing anybody has to learn. A named ELF section would load
 * smaller and is ELF-only; appending past the last segment works
 * anywhere but needs a program to find its own executable, and
 * `/proc` is Linux-only while `argv[0]` lies.
 *
 * **The text is emitted exactly as it was read**, which is what makes
 * it worth carrying: a name reported from it can never come from a
 * source that has since changed on disk, because this is the source
 * that was compiled.
 */
static void emit_sources(buf_t *w, arena_t *a, const char **sources,
                         int n_sources, const char *root)
{
    buf_line(w, "/* The box sources again, as text (issue 311c): the");
    buf_line(w, " * compiled program carries the C it was made from. */");

    for (int i = 0; i < n_sources; i++) {
        size_t n = 0;
        char *text = gp_read_file(a, sources[i], &n);
        char *sym = gt_box_symbol(a, path_within(sources[i], root),
                                  "source");

        buf_line(w, "static const char %s[] =", sym);
        /*
         * One C string per line of the original, so the generated
         * file stays readable and a compiler's line limit is never
         * approached. Escaped by hand rather than by a library,
         * because the set of characters that matter inside a C string
         * literal is small and known: the backslash, the quote, and
         * the newline that ends each piece.
         */
        buf_addstr(w, "    \"");
        for (size_t k = 0; k < n; k++) {
            char c = text[k];
            if (c == '\\')      buf_addstr(w, "\\\\");
            else if (c == '"')  buf_addstr(w, "\\\"");
            else if (c == '\n') buf_addstr(w, "\\n\"\n    \"");
            else if (c == '\r') buf_addstr(w, "\\r");
            else if (c == '\t') buf_addstr(w, "\\t");
            else                buf_addch(w, c);
        }
        buf_addstr(w, "\";\n");
        buf_line(w, "");
    }

    buf_line(w, "/* Which text belongs to which source. The path is the one");
    buf_line(w, " * the build handed the generator, shortened against the");
    buf_line(w, " * project root so two machines emit the same file. */");
    buf_line(w, "const box_source_t sora_box_sources[] = {");
    for (int i = 0; i < n_sources; i++) {
        const char *shortened = path_within(sources[i], root);
        char *sym = gt_box_symbol(a, shortened, "source");
        buf_line(w, "    { \"%s\", %s },", shortened, sym);
    }
    buf_line(w, "};");
    buf_line(w, "const int sora_n_box_sources = %d;", n_sources);
    buf_line(w, "");
}
/* }}} */

void ge_emit(const description_t *d, const char **sources, int n_sources,
             const char **maps, int n_maps,
             const char *out_path, const char *root, int external_boxes)
{
    buf_t w;
    buf_init(&w);

    buf_line(&w, "/* GENERATED by scripts/070-generate.c — do not edit, do not commit.");
    buf_line(&w, " * Derived entirely from the box sources named below; every size and");
    buf_line(&w, " * offset is a sizeof/offsetof expression the compiler computes. */");
    buf_line(&w, "#include <stddef.h>");
    buf_line(&w, "#include <string.h>");
    buf_line(&w, "#include <time.h>");
    buf_line(&w, "#include \"026-emitted.h\"");
    /* The placement functions call the station layer directly, which
     * is the point of them (issue 311b). */
    buf_line(&w, "#include \"018-station.h\"");
    buf_line(&w, "#include <stdio.h>");
    buf_line(&w, "#include <stdlib.h>");
    buf_line(&w, "#include \"049-observe.h\"");
    /* A box that edits a program stops the program when it is refused
     * (issue 106), so the box sources included below need to see the
     * call that does the stopping. */
    buf_line(&w, "#include \"091-stopping.h\"");
    /* A box that brings a described part inside a program reads that
     * description through the reader (issue 217). */
    buf_line(&w, "#include \"040-mapfile.h\"");
    buf_line(&w, "");

    /*
     * **Everything below this line exists to make boxes**, and a map
     * compiled for a program that already has them needs none of it
     * (issue 311d). No box source included, so no second copy of any
     * box compiled; no call wrappers, no field tables, no comparisons,
     * and no source text, because the loading program is already
     * carrying all of that.
     *
     * What is left is the build functions and the table naming them,
     * which is the entire useful content of a compiled map.
     */
    if (!external_boxes) {
        buf_line(&w, "/* The box sources, included whole: their types become visible, and");
        buf_line(&w, " * the compiler can inline each box into its shim. */");
        for (int i = 0; i < n_sources; i++)
            buf_line(&w, "#include \"%s\"", sources[i]);
        buf_line(&w, "");
    }

    const char **compare_of = calloc((size_t)(d->boxes.n + 1),
                                     sizeof *compare_of);
    if (!compare_of) {
        fprintf(stderr, "generator: out of memory\n");
        exit(71);
    }

    if (!external_boxes) {
        emit_sources(&w, d->arena, sources, n_sources, root);
        emit_compares(&w, d, compare_of);
        emit_shims(&w, d);
        emit_structs(&w, d);
        emit_placements(&w, d, compare_of, d->arena, root);
    }

    emit_maps(&w, d, d->arena, maps, n_maps, root, external_boxes);

    free(compare_of);

    /* Written whole to a temporary name and moved into place on
     * success: a failing generator must never leave yesterday's
     * output, or half of today's, where the build can find it. */
    size_t plen = strlen(out_path);
    char *tmp = malloc(plen + 8);
    if (!tmp) {
        fprintf(stderr, "generator: out of memory\n");
        exit(71);
    }
    memcpy(tmp, out_path, plen);
    memcpy(tmp + plen, ".tmp", 5);

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        fprintf(stderr, "generator: cannot write %s\n", tmp);
        exit(71);
    }
    if (fwrite(w.data, 1, w.len, f) != w.len) {
        fprintf(stderr, "generator: cannot write %s\n", tmp);
        fclose(f);
        remove(tmp);
        exit(71);
    }
    fclose(f);
    if (rename(tmp, out_path) != 0) {
        fprintf(stderr, "generator: cannot move output into place\n");
        remove(tmp);
        exit(71);
    }

    free(tmp);
    buf_free(&w);
}
/* }}} */
