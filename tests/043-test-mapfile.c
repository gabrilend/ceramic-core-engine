/*
 * 043-test-mapfile.c — proves the map file and its loader
 * (issues 601–605).
 *
 * What this is: the capstone tests. A program becomes a text file:
 * parsed, built against the registry, type-checked wire by wire,
 * validated whole, seeded, and run to a result on disk. Then every
 * way a map can be wrong is tried, and each must die with the
 * message a person could fix it from.
 *
 * How it does it, in general terms: maps are written into the RAM
 * tier and loaded for real. The happy path proves itself by what a
 * write box leaves in a file. Every failure case forks a child,
 * captures its dying words, and demands both the death and the
 * words.
 */
#include "040-mapfile.h"
#include "049-observe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char work_dir[256];

/* {{{ check() / write_text() / read_int() */
static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "mapfile test failed: %s\n", what);
        exit(1);
    }
}

static void write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fputs(text, f);
    fclose(f);
}

static int read_int(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -999999;
    int v = -999999;
    if (fscanf(f, "%d", &v) != 1)
        v = -999999;
    fclose(f);
    return v;
}
/* }}} */

/* {{{ expect_death_saying() */
/*
 * Run a map load in a child; demand it dies AND that its last words
 * contain the given fragment. The message is the product here — a
 * death with the wrong words is a failure too.
 */
static void expect_death_saying(const char *map_text, const char *fragment,
                                const char *what)
{
    char map_path[512];
    snprintf(map_path, sizeof map_path, "%s/doomed.map", work_dir);
    write_text(map_path, map_text);

    int pipefd[2];
    if (pipe(pipefd) != 0)
        exit(1);
    pid_t pid = fork();
    if (pid == 0) {
        dup2(pipefd[1], 2);
        close(pipefd[0]);
        map_t *m = map_load_file(map_path, 2);
        (void)m;
        _exit(0); /* surviving is the failure */
    }
    close(pipefd[1]);
    char words[4096] = { 0 };
    size_t filled = 0;
    for (;;) {
        ssize_t got = read(pipefd[0], words + filled,
                           sizeof words - 1 - filled);
        if (got <= 0 || filled >= sizeof words - 1)
            break;
        filled += (size_t)got;
    }
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);

    /*
     * **Exit 65, or a signal.** A refused map file now ends the
     * program with the code meaning "the input was malformed" rather
     * than aborting (issue 106), which is what lets a shell script
     * tell a mistyped box name from a machine out of memory. A signal
     * is still accepted here because a few refusals further down the
     * engine have not been moved onto the return path yet, and this
     * helper's job is to prove the map was refused rather than to
     * police how.
     *
     * What is *not* accepted is a clean exit zero, which is the
     * failure this is looking for: a bad map that ran anyway.
     */
    int refused = WIFSIGNALED(status)
               || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
    if (!refused) {
        fprintf(stderr, "mapfile test failed: %s — the child survived\n", what);
        exit(1);
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 65) {
        fprintf(stderr, "mapfile test failed: %s — refused, but with exit "
                        "code %d rather than 65, which is the code meaning "
                        "the input was malformed\n",
                what, WEXITSTATUS(status));
        exit(1);
    }
    if (!strstr(words, fragment)) {
        fprintf(stderr,
                "mapfile test failed: %s — died, but its words were:\n%s\n"
                "and the expected fragment was: %s\n",
                what, words, fragment);
        exit(1);
    }
}
/* }}} */

/* {{{ test_a_program_is_a_text_file() */
/* {{{ a half-built program round-trips */
/*
 * The two forms an `in` line gained in issue 210b: a bare dash for a
 * port with no source yet, and `x64` before the source for a starting
 * depth. Both had been decided and neither could be written.
 *
 * What this proves is the round trip on a program that **cannot run**.
 * A station holding an unconfigured port never becomes ready, which is
 * an ordinary state rather than a fault, and it is what lets a program
 * be assembled from nothing and wired one arrow at a time. Until the
 * format could spell it, the dump wrote a comment admitting it could
 * not — honest, and it lost the round trip, so a half-built program
 * was the one thing a dump could not promise to reload.
 */
static char *slurp_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;
    static char buf[8192];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void test_half_built_round_trips(void)
{
    char map_path[512], dump1[512], dump2[512], map_text[1024];
    snprintf(map_path, sizeof map_path, "%s/half.map", work_dir);
    snprintf(dump1, sizeof dump1, "%s/half-dump1.map", work_dir);
    snprintf(dump2, sizeof dump2, "%s/half-dump2.map", work_dir);

    /* `mix` takes an int and a double. Port 0 is left with no source
     * at all and given a deeper buffer than the default, so both new
     * forms appear on one line and their interaction shows; port 1
     * takes an ordinary value. That station can never run, and that is
     * the point.
     *
     * A second station that *can* run rides along, and it has to: the
     * seed sweep refuses a map where nothing could ever start. A
     * program mid-assembly looks exactly like this anyway — some of it
     * working, some of it not yet wired. A program where *nothing* is
     * wired is still refused at load, which issue 212 changes when it
     * stops the seed sweep being a phase. */
    snprintf(map_text, sizeof map_text,
        /* Marked as the way out, because a program that never says
         * where its results come from is not finished (issue 209).
         * Nothing is wired out of it, which is exactly the case the
         * requirement is built to make legible: the declaration is
         * the interface, and what flows through it is separate. */
        "station runner add p result\n"
        "  in 0 = 3\n"
        "  in 1 = 4\n"
        "\n"
        "station waiting mix p\n"
        "  in 0 x64 -\n"
        "  in 1 = 2.5\n");
    write_text(map_path, map_text);

    map_t *m = map_load_file(map_path, 2);

    check(map_station(m, 1)->in_ports[0].kind == IN_PORT_NONE,
          "a dash left the port with no source");
    check(map_station(m, 1)->in_ports[0].capacity == 64,
          "and the depth before it still sized the slots");
    check(map_station(m, 1)->in_ports[1].kind == IN_PORT_STATIC,
          "the other port took its value as usual");
    check(map_seed_count(m) == 1,
          "only the runnable station was seeded; the half-built one was not");

    FILE *d1 = fopen(dump1, "w");
    map_dump(m, d1);
    fclose(d1);
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    /* The dump has to reload into the same program, which is the
     * whole claim: the file says what is actually there. */
    map_t *again = map_load_file(dump1, 2);
    check(map_station(again, 1)->in_ports[0].kind == IN_PORT_NONE,
          "reloading the dump gave a port with no source, not a buffer");
    check(map_station(again, 1)->in_ports[0].capacity == 64,
          "and the depth survived the trip");
    check(map_station(again, 1)->in_ports[1].kind == IN_PORT_STATIC,
          "and so did the value on the other port");

    FILE *d2 = fopen(dump2, "w");
    map_dump(again, d2);
    fclose(d2);
    pool_release(again->pool);
    pool_join(again->pool);
    map_destroy(again);

    char *first = slurp_file(dump1);
    char first_copy[8192];
    snprintf(first_copy, sizeof first_copy, "%s", first ? first : "");
    char *second = slurp_file(dump2);
    check(second && strcmp(first_copy, second) == 0,
          "dump of the dump is the dump, for a program that cannot run");
    printf("  a half-built program was written down and read back\n");
}
/* }}} */

static void test_a_program_is_a_text_file(void)
{
    char result_path[512], map_path[512], map_text[2048];
    snprintf(result_path, sizeof result_path, "%s/result.txt", work_dir);
    snprintf(map_path, sizeof map_path, "%s/first.map", work_dir);

    /* Stations deliberately out of declaration order — the sink is
     * named before it exists — which is the entire reason the loader
     * reads twice. The head is a comparator splitting seven against
     * five, with only `greater` wired: seven, doubled, written to a
     * file. A comment rides along to prove comments parse. */
    snprintf(map_text, sizeof map_text,
        "# a whole program, as text\n"
        "statics\n"
        "  0 = \"%s\"\n"
        "  1 = 5\n"
        "\n"
        "station head seven p\n"
        "  out 0 - decide.0\n"
        "\n"
        "station decide keep c\n"
        "  in 1 $1\n"
        "  out 2 - doubler.0\n"
        "\n"
        /* The way out is the doubler rather than the sink, because a
         * station that returns nothing has no output port to be one
         * with (issue 209). Its value goes on to the sink as well —
         * being the way out adds a rule only when nothing is wired. */
        "station doubler double_it p result\n"
        "  out 0 - sink.1\n"
        "\n"
        "station sink write_int_file p\n"
        "  in 0 $0\n",
        result_path);
    write_text(map_path, map_text);

    unlink(result_path);
    map_t *m = map_load_file(map_path, 2);
    check(map_seed_count(m) == 1, "exactly the head was seeded");
    pool_release(m->pool);
    pool_join(m->pool);
    map_destroy(m);

    check(read_int(result_path) == 14,
          "seven, routed greater-than-five, doubled, landed on disk as 14");
    printf("  a text file became a running program and left 14 on disk\n");
}
/* }}} */

/* {{{ static void the_keywords_are_not_reserved() */
/*
 * **No word is a reserved name** (issue 607), which is the property
 * the `station` keyword exists to buy.
 *
 * A station line used to be *what remains* — anything whose first
 * word was not `in`, `out` or `statics`. A negative definition can
 * only narrow: every keyword the format ever gained would take
 * another name away from every map already written, silently, with
 * the failure showing up as a parse error about something else. A
 * station called `in` was told there was an input line before any
 * station.
 *
 * Now the first word of a line is always a keyword and the second is
 * always a name, so the four words that mean something are ordinary
 * names everywhere else. This builds a program whose stations are
 * called exactly those four, runs it, writes it down, and reads it
 * back — because a name that parses and does not round-trip is a name
 * that only half works.
 */
static void the_keywords_are_not_reserved(void)
{
    char map_path[512], dump_path[512];
    snprintf(map_path, sizeof map_path, "%s/keywords.map", work_dir);
    snprintf(dump_path, sizeof dump_path, "%s/keywords-dump.map", work_dir);

    write_text(map_path,
        "statics\n"
        "  0 = 5\n"
        "\n"
        "station in seven p\n"
        "  out 0 - out.0\n"
        "station out add p result\n"
        "  in 1 $0\n"
        "  out 0 - statics.0\n"
        "station statics double_it p\n"
        "  out 0 - station.0\n"
        "station station keep p\n");

    map_t *m = map_load_file(map_path, 2);
    check(m->n_stations == 4,
          "four stations named after the four words that mean something");
    check(strcmp(m->station_names[0], "in") == 0
          && strcmp(m->station_names[1], "out") == 0
          && strcmp(m->station_names[2], "statics") == 0
          && strcmp(m->station_names[3], "station") == 0,
          "and each kept the name it was given");

    pool_release(m->pool);
    pool_join(m->pool);

    check(atomic_load(&map_station(m, 3)->runs) == 1,
          "and the program ran end to end: seven, plus five, doubled, "
          "kept");

    FILE *f = fopen(dump_path, "w");
    map_dump(m, f);
    fclose(f);
    map_destroy(m);

    /* Written down and read back, which is where a half-working name
     * would show. */
    map_t *again = map_load_file(dump_path, 2);
    check(again->n_stations == 4
          && strcmp(again->station_names[0], "in") == 0,
          "and a program named that way survives being written down");
    pool_release(again->pool);
    pool_join(again->pool);
    map_destroy(again);

    printf("  a program whose stations are called in, out, statics and "
           "station ran and round-tripped\n");
}
/* }}} */

/* {{{ the gallery of refusals */
static void test_every_refusal(void)
{
    expect_death_saying(
        "station head sevn p\n",
        "no box named 'sevn'",
        "a misspelled box was accepted");

    expect_death_saying(
        "station head seven p\n"
        "  out 0 - nowhere.0\n",
        "arrow to 'nowhere', which does not exist",
        "an arrow into the void was accepted");

    expect_death_saying(
        "station head seven p\n"
        "  out 0 - other.5\n"
        "station other double_it p\n",
        /* The refusal moved into the wiring operation (issue 210g)
         * and gained the box's name on the way, which is what let the
         * loader stop keeping a second copy of this check. What is
         * asserted is the fact, not the sentence: the arrow named a
         * port, and the box has one. */
        "'double_it' has 1 port",
        "an arrow to a port beyond the box was accepted");

    expect_death_saying(
        "station head seven p\n"
        "  out 0 - wrong.1\n"
        "station wrong mix p\n",
        /* Both ends, by position and by size (issues 311b, 311c).
         * A type name is not what makes a wire legal or illegal — the
         * width is — so the message points at the two places that
         * disagree and at how much each of them counts. */
        "head output 0 produces 4 bytes and wrong input 1 takes 8 bytes",
        "a wire between different widths was accepted");

    /*
     * The entry this line names exists, and it did not have to
     * before. Reading a file resolves what the *file* says — which
     * statics entry, which text — and only then asks the program to
     * take it, so a line that is wrong in both ways is now answered
     * about the entry rather than about the port. This map has one
     * fault instead of two, which is what it was always meant to
     * have: a port number past the end of a box that takes nothing.
     */
    expect_death_saying(
        "statics\n"
        "  0 = 5\n"
        "\n"
        "station head seven p\n"
        "  in 3 $0\n",
        /* Likewise the input side: the port check is the
         * configuration surface's now, so the words are the ones it
         * speaks (issue 210g). */
        "has no port 3",
        "an input line beyond the box was accepted");

    expect_death_saying(
        "station head seven p\n"
        "  out 0 broken.0\n",
        "expected '-' between port and destination",
        "a malformed out line was accepted");

    expect_death_saying(
        "station head seven x\n",
        "kind must be p (plain), c (comparator), or i (iterator)",
        "an unknown kind letter was accepted");

    /*
     * **A refusal that used to be here is gone, and the property it
     * guarded went with it** (issue 405).
     *
     * An arrow whose destination held a static was refused, on the
     * grounds that a static already holds its value and has nowhere
     * to queue one. A value arriving there now *overwrites* the
     * constant — which is how a constant gets computed at startup
     * rather than written down, and is a property of the wire rather
     * than of the box, so it shows up in the map file instead of
     * happening invisibly inside C.
     *
     * What survives is the refusal below: an arrow onto a port with
     * **no source**, which has nothing to queue into and nothing to
     * overwrite. The two used to share one message that named which
     * of them it had found; only one of them is still a mistake.
     *
     * The scene proving the new behaviour lives with the statics
     * tests, where the write it performs lives.
     */
    expect_death_saying(
        "station head seven p result\n"
        "  out 0 - eater.0\n"
        "station eater double_it p\n"
        "  in 0 -\n",
        "that port is a port with no source yet",
        "an arrow onto a port with no source was accepted");

    /* The pull path's grave marker (issue 210). A bare station name
     * on an 'in' line used to mean "gather from there"; it is refused
     * rather than reinterpreted, because an old map file saying it
     * meant something real, and reading it as anything else would run
     * a program nobody wrote. */
    expect_death_saying(
        "station puller add p\n"
        "  in 1 fed\n"
        "station fed double_it p\n",
        "there is no pull path any more",
        "a gather line from an old map was accepted");

    /* Declared a way out, so that this map has exactly the one fault
     * it is testing for. Without the word it fails earlier, for never
     * having said where its results come from, and the refusal under
     * test never runs. */
    expect_death_saying(
        "station lonely add p result\n",
        "nothing to seed",
        "a map that can never start was accepted");

    expect_death_saying(
        "station head seven p\n",
        "never says where its results come from",
        "a program that never says what it produces was accepted");

    expect_death_saying(
        "station silent swallow c\n",
        "cannot be a comparator",
        "a void comparator was accepted");

    printf("  twelve wrong maps, twelve refusals, each naming its mistake\n");
}
/* }}} */

int main(void)
{
    snprintf(work_dir, sizeof work_dir,
             "/dev/shm/minimal-soramech/mapfile-test-%d", (int)getpid());
    char command[512];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0)
        exit(1);

    test_a_program_is_a_text_file();
    test_half_built_round_trips();
    test_every_refusal();
    the_keywords_are_not_reserved();

    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    if (system(command) != 0)
        exit(1);
    return 0;
}
