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

    if (!WIFSIGNALED(status)) {
        fprintf(stderr, "mapfile test failed: %s — the child survived\n", what);
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
        "head seven p\n"
        "  out 0 - decide.0\n"
        "\n"
        "decide keep c\n"
        "  in 1 $1\n"
        "  out 2 - doubler.0\n"
        "\n"
        "doubler double_it p\n"
        "  out 0 - sink.1\n"
        "\n"
        "sink write_int_file p\n"
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

/* {{{ the gallery of refusals */
static void test_every_refusal(void)
{
    expect_death_saying(
        "head sevn p\n",
        "no box named 'sevn'",
        "a misspelled box was accepted");

    expect_death_saying(
        "head seven p\n"
        "  out 0 - nowhere.0\n",
        "arrow to 'nowhere', which does not exist",
        "an arrow into the void was accepted");

    expect_death_saying(
        "head seven p\n"
        "  out 0 - other.5\n"
        "other double_it p\n",
        "that station has 1 slot",
        "an arrow to a slot beyond the box was accepted");

    expect_death_saying(
        "head seven p\n"
        "  out 0 - wrong.1\n"
        "wrong mix p\n",
        "box returns int, slot takes double",
        "a type-mismatched wire was accepted");

    expect_death_saying(
        "head seven p\n"
        "  in 3 $0\n",
        "names a slot that does not exist",
        "an input line beyond the box was accepted");

    expect_death_saying(
        "head seven p\n"
        "  out 0 broken.0\n",
        "expected '-' between port and destination",
        "a malformed out line was accepted");

    expect_death_saying(
        "head seven x\n",
        "kind must be p (plain), c (comparator), or i (iterator)",
        "an unknown kind letter was accepted");

    expect_death_saying(
        "statics\n"
        "  0 = 5\n"
        "head seven p\n"
        "  out 0 - eater.0\n"
        "eater double_it p\n"
        "  in 0 $0\n",
        "that slot is static",
        "an arrow onto a static slot was accepted");

    /* The pull path's grave marker (issue 210). A bare station name
     * on an 'in' line used to mean "gather from there"; it is refused
     * rather than reinterpreted, because an old map file saying it
     * meant something real, and reading it as anything else would run
     * a program nobody wrote. */
    expect_death_saying(
        "puller add p\n"
        "  in 1 fed\n"
        "fed double_it p\n",
        "there is no pull path any more",
        "a gather line from an old map was accepted");

    expect_death_saying(
        "lonely add p\n",
        "nothing to seed",
        "a map that can never start was accepted");

    expect_death_saying(
        "silent swallow c\n",
        "cannot be a comparator",
        "a void comparator was accepted");

    printf("  eleven wrong maps, eleven refusals, each naming its mistake\n");
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
    test_every_refusal();

    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    if (system(command) != 0)
        exit(1);
    return 0;
}
