/*
 * 064-test-cell-states.c — proves the per-cell state machine on its
 * own, with no delivery around it (issue 210c).
 *
 * What this is: the test that makes it safe to take the station's
 * mutex off the delivery path. That mutex is currently still there,
 * covering the copies, which means a bug in this machine cannot yet
 * corrupt anything — and that is exactly why this is the moment to
 * prove it. Once the lock comes off, a wrong transition stops being a
 * refused move and becomes a torn value.
 *
 * How it does it, in general terms: three questions, each asked
 * without any of the engine's ordinary machinery in the way.
 *
 * **Does exactly one thread win a contested transition?** Sixteen
 * threads reach for the same cell in the same instant. If two ever
 * won, two threads would own one cell, and the entire argument for
 * removing the lock would be false.
 *
 * **Is a move from a state the cell is not in refused?** This is what
 * makes an illegal transition impossible rather than merely
 * discouraged. Nothing in the engine asks for one; the point is that
 * the primitive would say no if something did, so a future caller
 * cannot invent a fifth path through the machine by accident.
 *
 * **Does a free-for-all lose or duplicate anything?** Producers and
 * consumers hammer one small run of cells with no lock of any kind
 * between them, and every value written must come out exactly once.
 * This is the machine doing the job the mutex does today, alone, and
 * it is a preview of the shape issue 210d builds on top of it.
 *
 * The cells come from a real station rather than from a hand-built
 * array, so that what is under test is the layout the engine actually
 * allocates — the stride, the alignment rounding, and the state's
 * position after the value.
 */
#include "018-station.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ unused__call() */
/* A station needs a shim to be placed. This one never runs: every
 * test below reaches for the station's cells directly and nothing is
 * ever delivered, so no task is ever built. */
static void unused__call(task_t *t)
{
    (void)t;
    fprintf(stderr, "cell states: a box ran, and none should have\n");
    abort();
}
/* }}} */

/* {{{ a station's first port, for reaching its cells */
static map_t *bare_map(int cells, int elem_size)
{
    map_t *m = map_create(1);
    int sizes[1] = { elem_size };
    map_place(m, 0, unused__call, STATION_PLAIN, 1, sizes, 0);
    map_slot_start_depth(m, 0, 0, cells);
    return m;
}

static slot_t *first_port(map_t *m)
{
    return &map_station(m, 0)->slots[0];
}
/* }}} */

/* {{{ test_one_winner() */
/*
 * Sixteen threads reaching for the same cells at the same moment.
 * Every cell is won exactly once, and the wins add up.
 *
 * The obvious shape for this — one cell, everybody reaches, count the
 * winners, reset, repeat — needs a barrier between rounds, and the
 * barrier turns out to be the hard part rather than the machine being
 * tested: a straggler still between "the round began" and its own
 * attempt can arrive after the cell has been reset and win a round it
 * was not in. That is a bug in the test's own scaffolding, and
 * building a correct barrier to test a thing whose whole purpose is
 * to avoid needing one is the wrong way round.
 *
 * So contention comes from breadth instead. Every thread walks the
 * same long run of cells from the same end at the same time, so they
 * pile onto each cell together and spread out only as they lose. The
 * property is unchanged and needs no coordination to check: the total
 * number of wins must equal the number of cells, because each cell
 * can leave empty exactly once and nothing puts it back.
 */
enum { RACERS = 16, ARENA_CELLS = 4096 };

typedef struct racer {
    slot_t      *port;
    _Atomic int *go;
    int          won;        /* this thread's own tally */
} racer_t;

static void *racer_main(void *arg)
{
    racer_t *r = arg;
    /* A spin rather than a condition variable: the wait is over in
     * microseconds and the point is to release everyone as close to
     * simultaneously as the hardware allows. */
    while (!*r->go)
        ;

    for (int c = 0; c < ARENA_CELLS; c++)
        if (slot_cell_move(r->port, c, CELL_EMPTY, CELL_RESERVED))
            r->won++;
    return NULL;
}

static void test_one_winner(void)
{
    map_t *m = bare_map(ARENA_CELLS, (int)sizeof(int));
    slot_t *port = first_port(m);

    _Atomic int go = 0;
    pthread_t threads[RACERS];
    racer_t racers[RACERS];
    for (int i = 0; i < RACERS; i++) {
        racers[i].port = port;
        racers[i].go = &go;
        racers[i].won = 0;
        pthread_create(&threads[i], NULL, racer_main, &racers[i]);
    }

    go = 1;
    for (int i = 0; i < RACERS; i++)
        pthread_join(threads[i], NULL);

    int total_winners = 0;
    for (int i = 0; i < RACERS; i++)
        total_winners += racers[i].won;

    if (total_winners != ARENA_CELLS) {
        fprintf(stderr,
                "%d cells produced %d winners — two threads owned one cell\n",
                ARENA_CELLS, total_winners);
        exit(1);
    }

    /* The count alone would also be satisfied by a run in which one
     * cell was won twice and another never at all, so the cells are
     * asked directly. Every one of them must refuse to be taken from
     * empty now, because none of them is empty any more. */
    for (int c = 0; c < ARENA_CELLS; c++) {
        if (slot_cell_move(port, c, CELL_EMPTY, CELL_RESERVED)) {
            fprintf(stderr, "cell %d was still empty afterwards\n", c);
            exit(1);
        }
    }

    map_destroy(m);
    printf("  %d threads over %d cells: every cell won exactly once\n",
           RACERS, ARENA_CELLS);
}
/* }}} */

/* {{{ test_illegal_moves_refused() */
/*
 * A cell in one state refuses every move that does not start from it.
 *
 * Walked over all four states rather than spot-checked, because the
 * property worth having is not "these three cases are handled" but
 * "the only way out of a state is a move that names it" — and that is
 * a statement about the whole table, so the whole table is what gets
 * asked.
 */
static void test_illegal_moves_refused(void)
{
    static const int states[4] = {
        CELL_EMPTY, CELL_RESERVED, CELL_READY, CELL_CLAIMED,
    };
    static const char *const names[4] = {
        "empty", "reserved", "ready", "claimed",
    };

    map_t *m = bare_map(4, (int)sizeof(int));
    slot_t *port = first_port(m);
    int refused = 0;

    for (int held = 0; held < 4; held++) {
        /* Put the cell into the state under examination. Cell 0 is
         * empty to begin with and is returned to empty at the end of
         * each pass, so this one move is always legal. */
        if (held != CELL_EMPTY
            && !slot_cell_move(port, 0, CELL_EMPTY, states[held])) {
            fprintf(stderr, "could not place the cell into %s\n", names[held]);
            exit(1);
        }

        for (int from = 0; from < 4; from++) {
            if (from == held)
                continue;
            /* The destination is deliberately irrelevant: what is
             * being tested is that the move is refused because of
             * where it starts, whatever it aims at. */
            for (int to = 0; to < 4; to++) {
                if (slot_cell_move(port, 0, states[from], states[to])) {
                    fprintf(stderr,
                            "a cell in %s accepted a move from %s to %s\n",
                            names[held], names[from], names[to]);
                    exit(1);
                }
                refused++;
            }
        }

        if (held != CELL_EMPTY
            && !slot_cell_move(port, 0, states[held], CELL_EMPTY)) {
            fprintf(stderr, "could not return the cell to empty from %s\n",
                    names[held]);
            exit(1);
        }
    }

    map_destroy(m);
    printf("  %d moves from a state the cell was not in, %d refused\n",
           refused, refused);
}
/* }}} */

/* {{{ test_free_for_all() */
/*
 * Producers and consumers over one small run of cells, with no lock
 * anywhere. Every value written comes out exactly once.
 *
 * The run of cells is deliberately much smaller than the number of
 * values, so producers genuinely run out of empty cells and consumers
 * genuinely run out of ready ones, and both have to go round again.
 * A generous buffer would let every producer find a free cell on its
 * first look and would test almost nothing.
 *
 * Nothing here promises the values come out in the order they went
 * in, and nothing here checks it. That is not an oversight: issue
 * 210d gives arrival order up on purpose, and a test that quietly
 * relied on it would fail later for a reason that had nothing to do
 * with this machine.
 */
enum { PRODUCERS = 4, CONSUMERS = 4, PER_PRODUCER = 25000, CELLS = 8 };
enum { TOTAL_VALUES = PRODUCERS * PER_PRODUCER };

static slot_t     *arena;
static _Atomic int produced_count;
static _Atomic int consumed_count;
static _Atomic int seen[TOTAL_VALUES];

static void *producer_main(void *arg)
{
    int base = (int)(long)arg * PER_PRODUCER;
    for (int i = 0; i < PER_PRODUCER; i++) {
        int value = base + i;
        /* Look for an empty cell, take it, fill it, publish it. A
         * cell taken by somebody else between the look and the take
         * simply refuses, and this thread tries the next one — no
         * blocking, no retry in place. */
        for (int c = 0; ; c = (c + 1) % CELLS) {
            if (!slot_cell_move(arena, c, CELL_EMPTY, CELL_RESERVED))
                continue;
            memcpy(slot_cell(arena, c), &value, sizeof value);
            if (!slot_cell_move(arena, c, CELL_RESERVED, CELL_READY)) {
                fprintf(stderr, "a reserved cell was taken from its writer\n");
                exit(1);
            }
            produced_count++;
            break;
        }
    }
    return NULL;
}

static void *consumer_main(void *arg)
{
    (void)arg;
    for (;;) {
        if (consumed_count >= TOTAL_VALUES)
            return NULL;
        for (int c = 0; c < CELLS; c++) {
            if (!slot_cell_move(arena, c, CELL_READY, CELL_CLAIMED))
                continue;
            int value;
            memcpy(&value, slot_cell(arena, c), sizeof value);
            if (!slot_cell_move(arena, c, CELL_CLAIMED, CELL_EMPTY)) {
                fprintf(stderr, "a claimed cell was taken from its reader\n");
                exit(1);
            }
            if (value < 0 || value >= TOTAL_VALUES) {
                fprintf(stderr, "a cell yielded %d, which was never written — "
                                "bytes were read before they landed\n", value);
                exit(1);
            }
            seen[value]++;
            consumed_count++;
        }
    }
}

static void test_free_for_all(void)
{
    map_t *m = bare_map(CELLS, (int)sizeof(int));
    arena = first_port(m);

    produced_count = 0;
    consumed_count = 0;
    for (int i = 0; i < TOTAL_VALUES; i++)
        seen[i] = 0;

    pthread_t writers[PRODUCERS], readers[CONSUMERS];
    for (long i = 0; i < PRODUCERS; i++)
        pthread_create(&writers[i], NULL, producer_main, (void *)i);
    for (int i = 0; i < CONSUMERS; i++)
        pthread_create(&readers[i], NULL, consumer_main, NULL);

    for (int i = 0; i < PRODUCERS; i++)
        pthread_join(writers[i], NULL);
    for (int i = 0; i < CONSUMERS; i++)
        pthread_join(readers[i], NULL);

    if (produced_count != TOTAL_VALUES || consumed_count != TOTAL_VALUES) {
        fprintf(stderr, "%d produced, %d consumed, %d expected\n",
                (int)produced_count, (int)consumed_count, TOTAL_VALUES);
        exit(1);
    }
    for (int i = 0; i < TOTAL_VALUES; i++) {
        if (seen[i] != 1) {
            fprintf(stderr, "value %d came out %d times\n", i, (int)seen[i]);
            exit(1);
        }
    }

    map_destroy(m);
    printf("  %d values through %d cells, %d writers against %d readers, "
           "no lock: each out exactly once\n",
           TOTAL_VALUES, CELLS, PRODUCERS, CONSUMERS);
}
/* }}} */

int main(void)
{
    test_one_winner();
    test_illegal_moves_refused();
    test_free_for_all();
    return 0;
}
