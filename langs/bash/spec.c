/* langs/bash/spec.c — Bash language spec for SoraMech.
 *
 * What it is, in a sentence: lets the pool runner call Bash box
 * functions out-of-process by holding a Unix-domain-socket
 * connection to a persistent bash subprocess per worker thread.
 *
 * Designed in issue 308. Current state: stub — exports the spec
 * symbol with name/file_ext set and all callbacks NULL. The real
 * implementation (length-prefix framing, persistent bash-server.sh
 * subprocess per worker, socket connection lifecycle) lands when
 * 308 is built.
 */

#include "lang-spec.h"

/* {{{ soramech_lang_spec */
lang_spec_t soramech_lang_spec = {
    .name     = "bash",
    .file_ext = ".sh",
    .init     = 0,
    .teardown = 0,
    .compile  = 0,
    .invoke   = 0,
};
/* }}} */
