/*
 * 125-mechanism-boxes.c — boxes for the program the viewer is pointed at.
 *
 * Ordinary C functions, like every box. They exist so there is a graph
 * whose ring buffers visibly fill at different rates: one station that
 * takes three values and cannot run until all three have arrived, fed
 * by paths that deliver at wildly different speeds.
 */
#include <stdio.h>

/* {{{ int three_way(int a, int b, int c) */
/*
 * Three inputs, one result. The point of it is the waiting: a station
 * placed with this box has three ring buffers and runs only when every
 * one of them holds a value, so two of them pile up while the third
 * trickles.
 */
int three_way(int a, int b, int c)
{
    return a + b + c;
}
/* }}} */

/* {{{ int cycle_thirty(int x) */
/*
 * Counts 0 to 29 and starts again. A comparator placed on this against
 * a threshold of ten splits its output ten ways to one to nineteen,
 * which is the uneven feed the buffers are there to show.
 */
int cycle_thirty(int x)
{
    return x % 30;
}
/* }}} */
