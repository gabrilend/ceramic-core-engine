/* tests/maps/319e-c-create/src/trigger.c — C box that spawns a
 * downstream Lua box at runtime, mirroring the 319d Lua fixture
 * but driven from a C box function instead. Proves the C bindings
 * for runtime self-construction (issue 319e) work end-to-end.
 *
 * The trigger box receives one input ("input"), uses
 * soramech_create_box to mint a new Lua box, uses soramech_connect
 * to wire its own output to the new box's "x" input, and returns
 * a marker string. The runtime then pushes the marker through the
 * freshly-added connection and the dynamically-created Lua box
 * fires.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "soramech.h"

/* {{{ trigger */
int trigger(const void **inputs, const int *sizes, int n,
            void *out_buf, int out_capacity, int *out_size)
{
    if (n < 1) return -1;

    /* Build the spec for a new Lua box. The shape mirrors what
     * would be on disk at boxes/<id>.json. */
    const char *spec =
        "{"
          "\"kind\":\"call\","
          "\"lang\":\"c\","
          "\"ref\":\"src/echo_dyn.c\","
          "\"fn\":\"echo\","
          "\"inputs\":["
            "{\"name\":\"x\",\"type\":\"string\"}"
          "]"
        "}";

    char  new_id[SORAMECH_BOX_ID_BUF_SIZE];
    char *err = NULL;
    if (soramech_create_box(spec, (int)strlen(spec),
                            new_id, sizeof new_id, &err) != 0) {
        fprintf(stderr, "trigger.c: create_box failed: %s\n",
                err ? err : "(no message)");
        free(err);
        return -1;
    }

    /* Build the connection entry: wire trigger.output → new.x */
    char conn[256];
    int  n_conn = snprintf(conn, sizeof conn,
        "{\"from_box\":\"trigger\","
         "\"from_branch\":null,"
         "\"to_box\":\"%s\","
         "\"to_input\":\"x\"}", new_id);
    if (n_conn < 0 || n_conn >= (int)sizeof conn) return -1;

    if (soramech_connect(conn, n_conn, &err) != 0) {
        fprintf(stderr, "trigger.c: connect failed: %s\n",
                err ? err : "(no message)");
        free(err);
        return -1;
    }

    /* Output a marker so the test runner can verify both that
     * trigger ran AND what the new id was. */
    int marker_n = snprintf((char *)out_buf, (size_t)out_capacity,
                            "c-trigger-fired:%s:%.*s",
                            new_id, sizes[0], (const char *)inputs[0]);
    if (marker_n < 0 || marker_n >= out_capacity) return -1;
    *out_size = marker_n;
    return 0;
}
/* }}} */
