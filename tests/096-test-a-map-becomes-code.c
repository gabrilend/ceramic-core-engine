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
#include "018-station.h"
#include "026-registry.h"
#include "040-mapfile.h"
#include "049-observe.h"

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
static void dump_to(map_t *m, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    map_dump(m, f);
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
    const map_build_t *compiled = registry_map_build("095-doubling.map");
    check(compiled != NULL,
          "the build compiled the description it was told about");
    check(registry_map_build("maps/095-doubling.map") != NULL,
          "and it is found by the path the build knew it as");
    check(registry_map_build("no-such.map") == NULL,
          "and a description this program was not built with is not there");
    if (!compiled)
        return 1;

    /*
     * The description, read as text — the path this engine has always
     * had, still here and still doing the same thing.
     */
    printf("  (the two warnings below are the port this map leaves "
           "unwired, once per program)\n");
    fflush(stdout);

    map_t *read = map_load_file(SORA_ROOT "/maps/095-doubling.map", 2);

    /* And the same description, as the calls the build compiled it
     * into. Nothing here parses anything. */
    map_t *built = map_create_empty();
    compiled->build(built);
    map_start(built, 2);
    const char *no = map_bring_up(built);
    if (no) {
        fprintf(stderr, "the compiled program was refused: %s\n", no);
        return 1;
    }

    char from_text[512], from_code[512];
    snprintf(from_text, sizeof from_text, "%s/from-text.map", work_dir);
    snprintf(from_code, sizeof from_code, "%s/from-code.map", work_dir);
    dump_to(read, from_text);
    dump_to(built, from_code);

    char *a = slurp(from_text);
    char *b = slurp(from_code);
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
     * **And the compiled one works at an offset**, which is what lets
     * a compiled description be brought inside a program that already
     * has stations. The generated function records where each station
     * landed rather than assuming they are numbered from zero, so
     * building it twice into one program gives two independent copies
     * — the same claim instantiating from text makes.
     */
    map_t *twice = map_create_empty();
    compiled->build(twice);
    int after_first = twice->n_stations;
    compiled->build(twice);
    check(twice->n_stations == 2 * after_first,
          "building a compiled description twice made two of everything");

    int doors = 0;
    for (int i = 0; i < twice->n_stations; i++)
        if (map_station(twice, i)->door == DOOR_OUT)
            doors++;
    check(doors == 2,
          "and each copy brought its own way out, so the second was not "
          "built on top of the first");

    map_destroy(twice);

    pool_release(read->pool);
    pool_join(read->pool);
    pool_release(built->pool);
    pool_join(built->pool);
    map_destroy(read);
    map_destroy(built);

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
