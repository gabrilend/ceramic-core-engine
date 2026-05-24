/* tests/maps/246-c-shim/src/echo.c — fixture C box for issue 246.
 *
 * Receives one input ("msg") and returns its bytes prefixed with a
 * marker. The port has a custom_translation shim attached
 * (translations/echo__msg.c) that uppercases the bytes BEFORE this
 * function sees them. The expected output proves the shim ran. */

#include <string.h>

/* {{{ echo */
int echo(const void **inputs, const int *sizes, int n,
         void *out_buf, int out_capacity, int *out_size)
{
    if (n < 1) return -1;
    const char *prefix = "seen:";
    int prefix_len = (int)strlen(prefix);
    int total = prefix_len + sizes[0];
    if (total > out_capacity) return -1;
    memcpy((char *)out_buf,              prefix,    (size_t)prefix_len);
    memcpy((char *)out_buf + prefix_len, inputs[0], (size_t)sizes[0]);
    *out_size = total;
    return 0;
}
/* }}} */
