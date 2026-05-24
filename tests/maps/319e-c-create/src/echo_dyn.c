/* tests/maps/319e-c-create/src/echo_dyn.c — the C box that the
 * trigger box creates at runtime. Receives a string on its "x"
 * input, returns it with a prefix so the test runner can grep for
 * it. C-to-C is the simplest end-to-end shape: the worker thread
 * already has the C spec's per-worker state initialised, so the
 * new box has somewhere to run. (Cross-language runtime-create
 * needs spec auto-init for languages not in the static graph —
 * documented limit, follow-on slice.) */

#include <string.h>
#include <stdio.h>

/* {{{ echo */
int echo(const void **inputs, const int *sizes, int n,
         void *out_buf, int out_capacity, int *out_size)
{
    if (n < 1) return -1;
    const char *prefix = "c-echo-received:";
    int prefix_len = (int)strlen(prefix);
    int total = prefix_len + sizes[0];
    if (total > out_capacity) return -1;
    memcpy((char *)out_buf,              prefix,    (size_t)prefix_len);
    memcpy((char *)out_buf + prefix_len, inputs[0], (size_t)sizes[0]);
    *out_size = total;
    return 0;
}
/* }}} */
