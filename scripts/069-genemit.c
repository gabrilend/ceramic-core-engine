/*
 * 069-genemit.c — the description becomes C.
 *
 * What this is: the half of the generator that writes. Four kinds of
 * output, in the order the engine's header expects to find them: a
 * three-way comparison per orderable return type (issue 305), a shim
 * per box (302), a field table per struct (304), and the registry
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
 * must never leave yesterday's registry, or half of today's, where a
 * build can find it (issue 306).
 */
#include "067-genparse.h"

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
 * null in its registry row. The refusal happens at load, when a
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
 * extern in the registry header.
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
                snprintf(nested, sizeof nested, "&registry_structs[%d]",
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

    buf_line(w, "const struct_info_t registry_structs[] = {");
    for (int i = 0; i < d->structs.n; i++) {
        const sdef_t *s = vec_at(&d->structs, i);
        buf_line(w, "    { \"%s\", (int)sizeof(%s), %d, %s__fields },",
                 s->name, s->name, s->n_fields, s->name);
    }
    if (d->structs.n == 0)
        buf_line(w, "    { NULL, 0, 0, NULL },");
    buf_line(w, "};");
    buf_line(w, "const int registry_n_structs = %d;", d->structs.n);
    buf_line(w, "");
}
/* }}} */

/* {{{ static void emit_registry() */
/*
 * The registry itself (issue 303). Type names ride along as text
 * because "4 bytes versus 4 bytes" is not an error message anybody
 * can act on — the engine runs on the sizes and reports the names.
 */
static void emit_registry(buf_t *w, const description_t *d,
                          const char **compare_of)
{
    for (int i = 0; i < d->boxes.n; i++) {
        const box_t *b = vec_at(&d->boxes, i);
        if (b->n_params == 0)
            continue;
        buf_line(w, "static const box_param_t %s__params[] = {", b->name);
        for (int j = 0; j < b->n_params; j++)
            buf_line(w, "    { \"%s\", (int)sizeof(%s) },",
                     b->params[j].type, b->params[j].type);
        buf_line(w, "};");
    }
    buf_line(w, "");

    buf_line(w, "const box_info_t registry_boxes[] = {");
    for (int i = 0; i < d->boxes.n; i++) {
        const box_t *b = vec_at(&d->boxes, i);
        int is_void = strcmp(b->ret, "void") == 0;

        char params_ref[256];
        if (b->n_params > 0)
            snprintf(params_ref, sizeof params_ref, "%s__params", b->name);
        else
            snprintf(params_ref, sizeof params_ref, "NULL");

        char ret_size[128];
        if (is_void)
            snprintf(ret_size, sizeof ret_size, "0");
        else
            snprintf(ret_size, sizeof ret_size, "(int)sizeof(%s)", b->ret);

        buf_line(w, "    { \"%s\", %s__call, %d, %s, \"%s\", %s,",
                 b->name, b->name, b->n_params, params_ref, b->ret, ret_size);

        /* The exact allocation for one invocation, spelled out so the
         * compiler adds it up: the task header, the pointer array,
         * every argument, and the return value. */
        buf_addstr(w, "      sizeof(task_t)");
        buf_addf(w, " + %d * sizeof(void *)", b->n_params);
        for (int j = 0; j < b->n_params; j++)
            buf_addf(w, " + sizeof(%s)", b->params[j].type);
        if (!is_void)
            buf_addf(w, " + sizeof(%s)", b->ret);
        buf_line(w, ", %s },",
                 (!is_void && compare_of[i]) ? compare_of[i] : "NULL");
    }
    if (d->boxes.n == 0)
        buf_line(w, "    { NULL, NULL, 0, NULL, NULL, 0, 0, NULL },");
    buf_line(w, "};");
    buf_line(w, "const int registry_n_boxes = %d;", d->boxes.n);
    buf_line(w, "");
}
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

        buf_line(w, "/* places %s:%s */", file, b->name);
        buf_line(w, "static void %s__place(map_t *m, int station, int kind)",
                 sym);
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
        buf_line(w, "    s->box_name = \"%s\";", b->name);
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
                buf_line(w, "    s->in_ports[%d].fields = &registry_structs[%d];",
                         j, si);
        }
        if (!is_void && compare_of[i]) {
            int rsi = struct_index_by_name(d, b->ret);
            buf_line(w, "    if (extra) {");
            buf_line(w, "        s->in_ports[%d].type_name = \"%s\";",
                     b->n_params, b->ret);
            if (rsi >= 0)
                buf_line(w, "        s->in_ports[%d].fields = "
                            "&registry_structs[%d];", b->n_params, rsi);
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
    buf_line(w, "const box_place_t registry_places[] = {");
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
    buf_line(w, "const int registry_n_places = %d;", d->boxes.n);
    buf_line(w, "");
}
/* }}} */

/* {{{ ge_emit() */
void ge_emit(const description_t *d, const char **sources, int n_sources,
             const char *out_path, const char *root);

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
             const char *out_path, const char *root)
{
    buf_t w;
    buf_init(&w);

    buf_line(&w, "/* GENERATED by scripts/070-generate.c — do not edit, do not commit.");
    buf_line(&w, " * Derived entirely from the box sources named below; every size and");
    buf_line(&w, " * offset is a sizeof/offsetof expression the compiler computes. */");
    buf_line(&w, "#include <stddef.h>");
    buf_line(&w, "#include <string.h>");
    buf_line(&w, "#include <time.h>");
    buf_line(&w, "#include \"026-registry.h\"");
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
    buf_line(&w, "/* The box sources, included whole: their types become visible, and");
    buf_line(&w, " * the compiler can inline each box into its shim. */");
    for (int i = 0; i < n_sources; i++)
        buf_line(&w, "#include \"%s\"", sources[i]);
    buf_line(&w, "");

    const char **compare_of = calloc((size_t)(d->boxes.n + 1),
                                     sizeof *compare_of);
    if (!compare_of) {
        fprintf(stderr, "generator: out of memory\n");
        exit(71);
    }

    emit_sources(&w, d->arena, sources, n_sources, root);
    emit_compares(&w, d, compare_of);
    emit_shims(&w, d);
    emit_structs(&w, d);
    emit_registry(&w, d, compare_of);
    /* Emitted beside the record rather than instead of it, so the two
     * can be compared before either is trusted (issue 311b). */
    emit_placements(&w, d, compare_of, d->arena, root);

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
