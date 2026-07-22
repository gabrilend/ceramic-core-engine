/* Pair-matrix fixture functions, C side (issue 325). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ produce — append "-p" to the seed */
int produce(const void **inputs, const int *sizes, int n,
            void *out_buf, int out_capacity, int *out_size)
{
    if (n < 1) return -1;
    int written = snprintf((char *)out_buf, (size_t)out_capacity,
                           "%.*s-p", sizes[0], (const char *)inputs[0]);
    if (written < 0 || written >= out_capacity) return -1;
    *out_size = written;
    return 0;
}
/* }}} */

/* {{{ addone — parse a numeric string, add one */
int addone(const void **inputs, const int *sizes, int n,
           void *out_buf, int out_capacity, int *out_size)
{
    if (n < 1) return -1;
    char tmp[64];
    int len = sizes[0] < (int)(sizeof tmp - 1) ? sizes[0]
                                               : (int)(sizeof tmp - 1);
    memcpy(tmp, inputs[0], (size_t)len);
    tmp[len] = '\0';
    long v = atol(tmp) + 1;
    int written = snprintf((char *)out_buf, (size_t)out_capacity, "%ld", v);
    if (written < 0 || written >= out_capacity) return -1;
    *out_size = written;
    return 0;
}
/* }}} */

/* {{{ reflect — report the received bytes */
int reflect(const void **inputs, const int *sizes, int n,
            void *out_buf, int out_capacity, int *out_size)
{
    if (n < 1) return -1;
    int written = snprintf((char *)out_buf, (size_t)out_capacity,
                           "c-saw:%.*s", sizes[0], (const char *)inputs[0]);
    if (written < 0 || written >= out_capacity) return -1;
    *out_size = written;
    return 0;
}
/* }}} */
