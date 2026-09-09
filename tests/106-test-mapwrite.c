/*
 * 106-test-mapwrite.c — a description written back out as the text it
 * came from (issue 801).
 *
 * What this is: the parser's round trip, which it did not have. Text
 * became a description and nothing turned one back into text, so
 * nothing could ask whether the two agreed.
 *
 * How it does it, in general terms: read a file, write it back, read
 * *that*, and compare the two descriptions field by field. Comparing
 * descriptions rather than comparing text is deliberate — two files
 * that differ only in blank lines describe the same program, and a
 * test that failed on those would be a test about whitespace.
 *
 * Then, as the stronger claim, write it back a second time and compare
 * the *text*: whatever this writer's spelling of a program is, it
 * must be a fixed point. A writer that changed its mind on a second
 * pass would make a file that grew every time it was opened.
 *
 * This links the generator's own pieces rather than the engine, like
 * the other test of a build tool. It is testing the thing that reads
 * and writes descriptions, not the thing that runs them.
 */
#include "099-mapparse.h"

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

static char work_dir[128];

/* {{{ static void write_text() */
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
/* }}} */

/* {{{ static int same_inputs() */
static int same_inputs(const desc_input_t *a, const desc_input_t *b)
{
    while (a && b) {
        if (a->port != b->port || a->depth != b->depth
            || a->is_none != b->is_none || a->is_argument != b->is_argument
            || a->is_source != b->is_source
            || a->is_waiting != b->is_waiting)
            return 0;
        if (a->is_argument && a->argument != b->argument)
            return 0;
        if (a->is_source
            && (a->source_port != b->source_port
                || strcmp(a->source_station, b->source_station) != 0))
            return 0;
        if ((a->text == NULL) != (b->text == NULL))
            return 0;
        if (a->text && strcmp(a->text, b->text) != 0)
            return 0;
        a = a->next; b = b->next;
    }
    return a == NULL && b == NULL;
}
/* }}} */

/* {{{ static int same_outputs() */
static int same_outputs(const desc_output_t *a, const desc_output_t *b)
{
    while (a && b) {
        if (a->port != b->port || a->is_result != b->is_result)
            return 0;
        if (a->is_result) {
            if (a->result != b->result)
                return 0;
        } else if (a->dest_port != b->dest_port
                   || strcmp(a->dest_station, b->dest_station) != 0) {
            return 0;
        }
        a = a->next; b = b->next;
    }
    return a == NULL && b == NULL;
}
/* }}} */

/* {{{ static int same_description() */
static int same_description(const map_description_t *a,
                            const map_description_t *b)
{
    if (a->n_stations != b->n_stations)
        return 0;

    const desc_station_t *x = a->stations, *y = b->stations;
    while (x && y) {
        if (strcmp(x->name, y->name) != 0 || strcmp(x->box, y->box) != 0
            || x->kind != y->kind
            || x->cursor != y->cursor)
            return 0;
        if (!same_inputs(x->inputs, y->inputs))
            return 0;
        if (!same_outputs(x->outputs, y->outputs))
            return 0;
        x = x->next; y = y->next;
    }
    return x == NULL && y == NULL;
}
/* }}} */

/* {{{ static void round_trips() */
/*
 * One description carrying every form the format has, so that a form
 * added later without a writer to match fails here rather than
 * quietly disappearing from anything written out.
 */
static void round_trips(const char *what, const char *text)
{
    char first[256], second[256];
    snprintf(first, sizeof first, "%s/first.map", work_dir);
    snprintf(second, sizeof second, "%s/second.map", work_dir);

    write_text(first, text);
    map_description_t *a = mapfile_parse(first);

    char *written = mapfile_write(a);
    check(written != NULL, "a description was written back out");
    if (!written)
        return;

    write_text(second, written);
    map_description_t *b = mapfile_parse(second);

    check(same_description(a, b), what);

    /* And the writer is a fixed point: writing what it wrote produces
     * the same text. A writer that changed its mind on a second pass
     * would make a file that grew every time it was opened. */
    char *again = mapfile_write(b);
    check(again && strcmp(written, again) == 0,
          "and writing it a second time produced the same text");

    free(written);
    free(again);
    mapfile_free(a);
    mapfile_free(b);
}
/* }}} */

/* {{{ main */
int main(void)
{
    snprintf(work_dir, sizeof work_dir,
             "/dev/shm/minimal-soramech/mapwrite-%d", (int)getpid());
    char command[256];
    snprintf(command, sizeof command, "mkdir -p %s", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot make %s\n", work_dir);
        return 1;
    }

    round_trips("the plainest description survived",
        "station only add p\n");

    round_trips("doors, kinds and arrows survived",
        "station gate keep p\n"
        "  in 0 - 0$\n"
        "  out 0 - sum.0\n"
        "\n"
        "station sum add p\n"
        "  out 0 - 0$\n"
        "  in 0 - gate.0\n"
        "  in 1 = 5\n");

    round_trips("a statics section and the ports pointing into it survived",
        "station one add p\n"
        "  in 1 = 7\n"
        "\n"
        "station two nudge p\n"
        "  in 0 = { 1.5, 2.5, 3.5 }\n");

    round_trips("depths, unfinished ports and buffers survived",
        "station wide add p\n"
        "  in 0 x64\n"
        "  in 1 -\n");

    round_trips("an iterator's place in its exits survived",
        "station spread double_it i @2\n"
        "  out 0 - a.0\n"
        "  out 1 - b.0\n"
        "  out 2 - c.0\n"
        "\n"
        "station a keep p\n"
        "  in 0 - spread.0\n"
        "\n"
        "station b keep p\n"
        "  in 0 - spread.1\n"
        "\n"
        "station c keep p\n"
        "  in 0 - spread.2\n");

    round_trips("values waiting in a buffer survived, inner commas and all",
        "station gate keep p\n"
        "  in 0 - 0$\n"
        "\n"
        "station held nudge p\n"
        "  out 0 - 0$\n"
        "  in 0 x64 [{ 1.5, 2.5, 3.5 }, { 4.5, 5.5, 6.5 }]\n"
        "  in 1 -\n");

    snprintf(command, sizeof command, "rm -rf %s", work_dir);
    if (system(command) != 0)
        fprintf(stderr, "could not clean up %s\n", work_dir);

    if (failures) {
        fprintf(stderr, "%d mapwrite checks failed\n", failures);
        return 1;
    }
    printf("  every form the map format has survived being written back "
           "out and read again\n");
    return 0;
}
/* }}} */
