/*
 * 096-test-a-map-becomes-code.c — a description compiled into the
 * calls it describes (issue 311d).
 *
 * What this is: the proof that a map read as text while a program
 * runs and the same map compiled into C at build time produce the
 * **same program**. Not equivalent — the same, by dumping both and
 * comparing the text byte for byte.
 *
 * Comparing dumps is the cheapest proof available, because the dump
 * walks the live station table and writes what is actually there. Two
 * identical dumps are two identical tables, including every field a
 * hand-written comparison would forget to check.
 *
 * Why it matters that they are the same: the generated function calls
 * the same construction surface a person calling C would, so reading
 * a map and compiling one stop being two paths that must agree and
 * become one path with two authors. If they ever diverge — a default
 * applied on one side, a port left in a different state, arrows
 * attached in a different order — the text differs and this fails,
 * naming the first line where they part.
 *
 * **What the compiled form removes is names.** Every box name in the
 * description was resolved on the author's machine and became a
 * direct call to that box's placement function, so a misspelled box
 * fails the build rather than somebody else's startup, and no box
 * name is looked up while the program runs.
 */
#include "cera.h"

#include <stdlib.h>
#include <string.h>

#include "149-same-program.h"

/* {{{ static char *say_where_the_program_keeps_it() */
/*
 * Rewrites a description's shortcut so it names the box sources the way
 * the program that will read it does — `../src/boxes/` becomes
 * `src/boxes/`.
 *
 * **Not tidiness, and not an absolute path either.** A program handed
 * text writes out the sources it carries, under the paths they were
 * filed under when it was built, and compiles the text against those.
 * The symbol a box compiles to carries that path, and it has to be the
 * path the program already published or the result loads and fails to
 * resolve. An absolute path names the real file and produces a
 * different symbol, which is the failure this exists to avoid.
 *
 * So text handed to a running program is written against *that
 * program's* source layout. A caller who does not want to think about
 * it hands over the file instead, which has a home and needs no help.
 *
 * The caller frees what comes back.
 */
static char *say_where_the_program_keeps_it(const char *text)
{
    static const char relative[] = "../src/boxes/";
    static const char absolute[] = "src/boxes/";

    size_t room = strlen(text) + sizeof absolute + 64;
    char *out = malloc(room);
    if (!out)
        return NULL;

    const char *r = text;
    char *w = out;
    while (*r) {
        if (strncmp(r, relative, sizeof relative - 1) == 0) {
            memcpy(w, absolute, sizeof absolute - 1);
            w += sizeof absolute - 1;
            r += sizeof relative - 1;
            continue;
        }
        *w++ = *r++;
    }
    *w = '\0';
    return out;
}
/* }}} */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;

/* {{{ static void check() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "  FAIL: %s\n", what);
        failures++;
    }
}
/* }}} */

static char work_dir[256];

/* {{{ static void dump_to() */
static void dump_to(cera_map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    cera_map_dump(m, f);
    fclose(f);
}
/* }}} */

/* {{{ static char *slurp() */
static char *slurp(const char *path)
{
    static char buf[2][8192];
    static int which;
    char *out = buf[which++ % 2];
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    size_t n = fread(out, 1, 8191, f);
    out[n] = '\0';
    fclose(f);
    return out;
}
/* }}} */

/* {{{ main */
int main(void)
{
    snprintf(work_dir, sizeof work_dir,
             "/dev/shm/minimal-soramech/map-code-%d", (int)getpid());
    char command[512];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot make %s\n", work_dir);
        return 1;
    }

    /* The build was told about this description, so it is here. */
    const cera_map_build_t *compiled = cera_map_build_find("095-doubling.map");
    check(compiled != NULL,
          "the build compiled the description it was told about");
    check(cera_map_build_find("maps/095-doubling.map") != NULL,
          "and it is found by the path the build knew it as");
    check(cera_map_build_find("no-such.map") == NULL,
          "and a description this program was not built with is not there");
    if (!compiled)
        return 1;

    /*
     * The description, read as text — the path this engine has always
     * had, still here and still doing the same thing.
     */
    printf("  (the three warnings below are the port this map leaves "
           "unwired, once per program — and there are three programs "
           "now: read, compiled at build, compiled while running)\n");
    fflush(stdout);

    cera_map_t *read = cera_map_load_file(CERA_ROOT "/maps/095-doubling.map", 2);

    /* And the same description, as the calls the build compiled it
     * into. Nothing here parses anything. */
    cera_map_t *built = cera_map_create_empty();
    compiled->build(built, NULL, 0);
    cera_map_start(built, 2);
    const char *no = cera_map_bring_up(built);
    if (no) {
        fprintf(stderr, "the compiled program was refused: %s\n", no);
        return 1;
    }

    /*
     * **And the same description again, compiled while the program is
     * running** (issue 311d). This is the third route to one program,
     * and the one that matters for a program handed a description it
     * was not built for: the text goes out to the scratch tier, the
     * generator turns it into the calls it describes, the compiler
     * that built this binary compiles them, and the result is loaded.
     *
     * **Nothing about the boxes is compiled.** They are already in
     * this process, and what is emitted declares the functions that
     * build their stations rather than defining them — binding to the
     * ones this program published. That is why the station-builders
     * are published at all.
     */
    char *description = slurp(CERA_ROOT "/maps/095-doubling.map");

    /*
     * **Text has no home, so its shortcut is made absolute first**
     * (issues 610, 611).
     *
     * Every path in a description is relative to the description, and
     * this description's own is `../src/boxes/` — which is right where
     * it lives and wrong everywhere else. Handing over *text* is
     * handing over a description with no location, so the engine gives
     * it one: a scratch directory. The shortcut then points at
     * somewhere beside that directory, which is nowhere.
     *
     * So text is written against the layout of the program that will
     * read it, which the program knows and the text's author has to be
     * told. A caller who does not want to think about it hands over the
     * *file* instead, which has a home and needs no help.
     */
    char *absolute = say_where_the_program_keeps_it(description);
    const cera_map_build_t *compiled_now = cera_late_compile_map(absolute);
    check(compiled_now != NULL,
          "a description handed to the running program compiled into it");

    cera_map_t *late = NULL;
    if (compiled_now) {
        late = cera_map_create_empty();
        compiled_now->build(late, NULL, 0);
        cera_map_start(late, 2);
        const char *refused = cera_map_bring_up(late);
        if (refused) {
            fprintf(stderr, "the program compiled at run time was refused: "
                            "%s\n", refused);
            failures++;
        }
    }

    char from_text[512], from_code[512], from_late[512];
    snprintf(from_text, sizeof from_text, "%s/from-text.map", work_dir);
    snprintf(from_code, sizeof from_code, "%s/from-code.map", work_dir);
    snprintf(from_late, sizeof from_late, "%s/from-late.map", work_dir);
    dump_to(read, from_text);
    dump_to(built, from_code);
    if (late)
        dump_to(late, from_late);

    char *a = slurp(from_text);
    char *b = slurp(from_code);
    /* Compared on what they say about the program rather than on
     * where its code came from: the two paths compile from two
     * roots, so the same box carries two addresses (issue 611).
     * Everything else is still byte for byte. */
    drop_box_paths(a);
    drop_box_paths(b);
    if (strcmp(a, b) != 0) {
        const char *pa = a, *pb = b;
        int line = 1;
        while (*pa && *pb && *pa == *pb) {
            if (*pa == '\n') line++;
            pa++; pb++;
        }
        fprintf(stderr, "the two paths built different programs, first "
                        "differing at line %d\n", line);
        fprintf(stderr, "  read as text:  %.60s\n", pa);
        fprintf(stderr, "  compiled:      %.60s\n", pb);
        failures++;
    } else {
        printf("  a map read as text and the same map compiled into calls "
               "dumped identically\n");
    }

    /*
     * Three routes, one program. Compared against the build's own
     * compiled form rather than against the text, because that is the
     * comparison that would catch the two compiled forms drifting from
     * each other while both still matched the text loosely.
     */
    if (late) {
        char *c = slurp(from_code);
        char *e = slurp(from_late);
        check(strcmp(c, e) == 0,
              "a description compiled at build time and the same one "
              "compiled while running built the same program");
        if (strcmp(c, e) == 0)
            printf("  and compiling it again while the program ran made a "
                   "third identical copy, binding to boxes already here\n");
        cera_pool_release(late->pool);
        cera_pool_join(late->pool);
        cera_map_destroy(late);
    }

    /*
     * **And the compiled one works at an offset**, which is what lets
     * a compiled description be brought inside a program that already
     * has stations. The generated function records where each station
     * landed rather than assuming they are numbered from zero, so
     * building it twice into one program gives two independent copies
     * — the same claim instantiating from text makes.
     */
    cera_map_t *twice = cera_map_create_empty();
    compiled->build(twice, NULL, 0);
    int after_first = twice->n_stations;
    compiled->build(twice, NULL, 0);
    check(twice->n_stations == 2 * after_first,
          "building a compiled description twice made two of everything");

    /* A door is a port now, so the count walks ports rather than
     * stations — and both copies mark result zero, which is exactly
     * the collision a program built from one description twice is
     * supposed to have. */
    int doors = 0;
    for (int i = 0; i < twice->n_stations; i++) {
        cera_station_t *s = cera_map_station(twice, i);
        if (!s->call)
            continue;
        for (cera_out_port_t *p = s->out_ports; p; p = p->next)
            if (p->result != CERA_NOT_A_DOOR)
                doors++;
    }
    check(doors == 2,
          "and each copy brought its own way out, so the second was not "
          "built on top of the first");

    cera_map_destroy(twice);

    cera_pool_release(read->pool);
    cera_pool_join(read->pool);
    cera_pool_release(built->pool);
    cera_pool_join(built->pool);
    cera_map_destroy(read);
    cera_map_destroy(built);

    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    if (system(command) != 0)
        fprintf(stderr, "could not clean up %s\n", work_dir);

    if (failures) {
        fprintf(stderr, "%d checks failed\n", failures);
        return 1;
    }
    return 0;
}
/* }}} */
