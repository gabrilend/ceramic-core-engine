/* src/tag.c — C worker that prefixes its input with a language
 * tag and the input length, mirroring the Lua and Bash workers
 * so the three outputs are visually comparable. */
#include <stdio.h>
#include <string.h>

int tag_c(const void **inputs, const int *sizes, int n_inputs,
          void *out_buf, int out_capacity, int *out_size)
{
    (void)n_inputs;
    /* Inputs[0] is the bytes from the upstream fan-out wire. We
     * copy into a local buffer to NUL-terminate for the
     * format. */
    char tmp[128];
    int n = sizes[0] < (int)(sizeof tmp - 1) ? sizes[0] : (int)(sizeof tmp - 1);
    memcpy(tmp, inputs[0], n);
    tmp[n] = '\0';
    int written = snprintf((char *)out_buf, out_capacity,
                           "c:      %s  (%d bytes)", tmp, n);
    if (written < 0 || written >= out_capacity) return -1;
    *out_size = written;
    return 0;
}
