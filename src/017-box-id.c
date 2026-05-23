/* src/017-box-id.c — auto-generated box id strings, implementation.
 *
 * The whole story: one atomic uint32 counter, fetch-add it, format
 * the result as "auto_%08x". 4 billion ids per process run is more
 * than any reasonable workload needs; if a run somehow burns through
 * that many runtime-created boxes the counter wraps and we'd start
 * seeing collisions — but at that point the wrap is the smallest
 * problem the run has.
 *
 * Designed in issue 319 (Q4); landed in issue 319c.
 */
#include "017-box-id.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

/* {{{ box_id_counter — process-wide unique-id source */
/* Starts at 0; first id minted is "auto_00000000". The counter is
 * intentionally zero-initialized rather than seeded from time(NULL)
 * or any other entropy source: same-shape ids across runs are
 * easier to grep for in transcripts and last-run.jsonl, and we
 * don't need cross-run uniqueness here. */
static _Atomic uint32_t box_id_counter = 0;
/* }}} */

/* {{{ box_id_generate() */
int box_id_generate(char *buf, size_t buf_size)
{
    if (!buf || buf_size < BOX_ID_GEN_BUF_SIZE) return -1;

    /* relaxed memory order: the counter has no synchronization
     * relationship with any other memory; only its own atomicity
     * matters. */
    uint32_t id = atomic_fetch_add_explicit(&box_id_counter, 1u,
                                            memory_order_relaxed);

    int n = snprintf(buf, buf_size, "auto_%08x", (unsigned)id);
    /* snprintf returns the number it would have written if the
     * buffer were unlimited; n == 13 is success at buf_size >= 14. */
    if (n < 0 || (size_t)n + 1u > buf_size) return -1;
    return 0;
}
/* }}} */
