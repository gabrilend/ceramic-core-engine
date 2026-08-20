/*
 * 082-test-one-construction-path.c — a program read from a file and a
 * program built by calling the surface are the same program (issue
 * 212).
 *
 * What this is: the proof that there is **one** way structure comes
 * into existence, rather than two that agree by coincidence and drift
 * the first time somebody changes one of them.
 *
 * Reading a file used to be able to do things nothing else could. It
 * counted the stations before allocating, it validated in a phase
 * only it could enter, and it seeded from inside that phase — a state
 * called *still loading* that no other caller could be in. Each of
 * those has been taken out and made an ordinary operation, and the
 * claim is that what remains of reading a file is a reader: each line
 * becomes calls anybody could make.
 *
 * How it does it, in general terms: write a small map to disk and
 * load it; build the same program by calling the surface; dump both
 * and compare the text byte for byte. Comparing dumps is the cheapest
 * proof available, because the dump already walks the live station
 * table and writes what is actually there — so two identical dumps
 * are two identical station tables, including the things a
 * hand-written comparison would forget to check.
 *
 * **Not equivalent. The same.** If the two paths ever diverge — a
 * default applied on one side only, a port left in a different state,
 * an arrow attached in a different order — the text differs and this
 * fails, naming the first line where they part.
 */
#include "018-station.h"
#include "026-registry.h"
#include "040-mapfile.h"
#include "049-observe.h"
#include "073-latebox.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>

/* {{{ static char *slurp() */
static char *slurp(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    static char buf[2][8192];
    static int which;
    char *out = buf[which++ % 2];
    size_t n = fread(out, 1, 8191, f);
    out[n] = '\0';
    fclose(f);
    return out;
}
/* }}} */

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

/* {{{ static void must_take() */
static void must_take(const char *refusal, const char *what)
{
    if (refusal) {
        fprintf(stderr, "the surface refused %s: %s\n", what, refusal);
        exit(1);
    }
}
/* }}} */

int main(void)
{
    char dir[256], map_path[320], from_file[320], from_calls[320];
    snprintf(dir, sizeof dir, "%s/one-path-%d",
             registry_late_source_dir(), (int)getpid());
    char cmd[512];
    snprintf(cmd, sizeof cmd, "mkdir -p %s", dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "cannot make %s\n", dir);
        return 1;
    }
    snprintf(map_path, sizeof map_path, "%s/both.map", dir);
    snprintf(from_file, sizeof from_file, "%s/from-file.map", dir);
    snprintf(from_calls, sizeof from_calls, "%s/from-calls.map", dir);

    /*
     * Deliberately not the simplest program that could work. It has a
     * constant, a port given a starting depth, a fan-out, and a
     * station left with a port nobody has wired — each of which is
     * something one path could get right and the other wrong.
     */
    static const char *const text =
        "source seven p\n"
        "  out 0 - adder.0\n"
        "adder add p\n"
        "  in 1 = 1000\n"
        "  out 0 - twice.0\n"
        "  out 0 - waiting.0\n"
        "twice double_it p\n"
        "  in 0 x64\n"
        "waiting add p\n"
        "  in 1 -\n";
    FILE *f = fopen(map_path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", map_path);
        return 1;
    }
    fputs(text, f);
    fclose(f);

    /* The program deliberately leaves one port unwired, so that the
     * two paths are compared on something incomplete as well as on
     * something finished. Both the warning below and its twin from
     * the second program are the engine noticing exactly that. */
    printf("  (the two warnings below are what this program is for)\n");
    fflush(stdout);

    /* One program, read. */
    map_t *read = map_load_file(map_path, 2);
    dump_to(read, from_file);

    /*
     * The same program, said out loud. Every one of these is an
     * operation a debugger or a workbench could perform on a running
     * program — which is the point: there is nothing here that only
     * a file is allowed to do.
     */
    map_t *built = map_create_empty();
    const char *const names[] = { "source", "adder", "twice", "waiting" };
    const char *const boxes[] = { "seven", "add", "double_it", "add" };
    for (int i = 0; i < 4; i++) {
        int at = map_add_station(built);
        if (at != i) {
            fprintf(stderr, "stations came out in a different order: "
                            "wanted %d, got %d\n", i, at);
            return 1;
        }
        map_place_box(built, at, boxes[i], STATION_PLAIN);
        must_take(map_name_station(built, at, names[i]), "a name");
    }

    must_take(map_configure_port(built, 1, 1, IN_PORT_STATIC, "1000"),
              "a constant");
    map_in_port_start_depth(built, 2, 0, 64);
    must_take(map_configure_port(built, 3, 1, IN_PORT_NONE, NULL),
              "a port left unwired");

    map_connect(built, 0, 0, 1, 0);   /* source -> adder.0 */
    map_connect(built, 1, 0, 2, 0);   /* adder  -> twice.0 */
    map_connect(built, 1, 0, 3, 0);   /* adder  -> waiting.0, a fan-out */

    map_start(built, 2);
    must_take(map_bring_up(built), "the finished program");
    dump_to(built, from_calls);

    char *a = slurp(from_file);
    char *b = slurp(from_calls);
    if (strcmp(a, b) != 0) {
        /* Name the first line where they part, because "these differ"
         * about two hundred-line files is not a finding anybody can
         * act on. */
        const char *pa = a, *pb = b;
        int line = 1;
        while (*pa && *pb && *pa == *pb) {
            if (*pa == '\n') line++;
            pa++; pb++;
        }
        fprintf(stderr, "the two paths built different programs, first "
                        "differing at line %d\n", line);
        fprintf(stderr, "  from a file:  %.60s\n", pa);
        fprintf(stderr, "  from calls:   %.60s\n", pb);
        return 1;
    }

    printf("  a program read from a file and one built by calling the "
           "surface dumped identically\n");

    /*
     * A deepened buffer survives the round trip, which is what this
     * whole scene nearly failed on.
     *
     * The dump used to write such a port as a depth followed by a
     * dash, and a bare dash means a port with **no source**. So two
     * different things were spelled the same way: a program with a
     * deepened buffer could be written down and could not be read
     * back, and an arrow into that port was refused on the way in.
     * The format gained a form for it — a depth with nothing after
     * it — and this is the assertion that the two are told apart.
     */
    if (map_station(read, 2)->in_ports[0].capacity != 64) {
        fprintf(stderr, "the deepened buffer came back %d slots deep\n",
                map_station(read, 2)->in_ports[0].capacity);
        return 1;
    }
    if (atomic_load(&map_station(read, 2)->in_ports[0].kind) != IN_PORT_RING) {
        fprintf(stderr, "the deepened buffer came back as something other "
                        "than a buffer\n");
        return 1;
    }
    if (atomic_load(&map_station(read, 3)->in_ports[1].kind) != IN_PORT_NONE) {
        fprintf(stderr, "the port with no source came back as something "
                        "else\n");
        return 1;
    }
    printf("  a deepened buffer and a port with no source stayed different "
           "things across the round trip\n");

    /* Let both run down rather than tearing them apart mid-flight:
     * the fan-out puts one value into a station that can never claim
     * it, and destroying a pool with work still in it says so. */
    pool_release(read->pool);
    pool_join(read->pool);
    pool_release(built->pool);
    pool_join(built->pool);

    map_destroy(read);
    map_destroy(built);
    return 0;
}
