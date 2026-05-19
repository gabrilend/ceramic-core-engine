/* tests/maps/hello/src/echo.c — fixture C box function.
 *
 * Every C box function takes the same shape: inputs as byte arrays
 * with sizes, a fixed output buffer, returns 0 on success or
 * nonzero on failure. The first input is echoed straight to the
 * output buffer; concat() concatenates the first two inputs.
 */

#include <string.h>

/* {{{ echo */
int echo(const void **inputs, const int *sizes, int n,
         void *out_buf, int out_capacity, int *out_size)
{
    if (n < 1) { *out_size = 0; return 0; }
    int sz = sizes[0];
    if (sz > out_capacity) return -1;
    memcpy(out_buf, inputs[0], (size_t)sz);
    *out_size = sz;
    return 0;
}
/* }}} */

/* {{{ concat */
int concat(const void **inputs, const int *sizes, int n,
           void *out_buf, int out_capacity, int *out_size)
{
    if (n < 2) return -1;
    int total = sizes[0] + sizes[1];
    if (total > out_capacity) return -1;
    memcpy((char *)out_buf,                 inputs[0], (size_t)sizes[0]);
    memcpy((char *)out_buf + sizes[0],      inputs[1], (size_t)sizes[1]);
    *out_size = total;
    return 0;
}
/* }}} */
