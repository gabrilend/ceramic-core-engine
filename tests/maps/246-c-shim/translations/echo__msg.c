/* tests/maps/246-c-shim/translations/echo__msg.c — issue 246 shim
 *
 * Custom translation for the "msg" input port on the "echo" box.
 * Uppercases every byte in the input before the box's main
 * function sees it. The contract: sm_translate is the entry
 * symbol; output goes in out_buf (native bytes); raw_native tells
 * us whether the input was already this language's native form
 * (1) or JSON from a cross-language producer (0). For this
 * fixture we treat both identically — uppercase regardless. */

#include <string.h>

/* {{{ sm_translate */
int sm_translate(const void *raw, int raw_size, int raw_native,
                 void *out_buf, int out_capacity, int *out_size)
{
    (void)raw_native;
    if (raw_size > out_capacity) return -1;
    const unsigned char *in  = (const unsigned char *)raw;
    unsigned char       *out = (unsigned char *)out_buf;
    for (int i = 0; i < raw_size; i++) {
        unsigned char c = in[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        out[i] = c;
    }
    *out_size = raw_size;
    return 0;
}
/* }}} */
