/* langs/c/spec.c — C language spec for SoraMech.
 *
 * What it is, in a sentence: lets the pool runner call C box
 * functions in-process by dlopen'ing per-box compiled .so files
 * and dlsym'ing the named function.
 *
 * Designed in issue 307. Current state: stub — exports the spec
 * symbol with name/file_ext set and all callbacks NULL. The real
 * implementation (compile callback running gcc -shared -fPIC,
 * dlopen cache, generated typed wrapper) lands when 307 is built.
 */

#include "lang-spec.h"

/* {{{ soramech_lang_spec */
lang_spec_t soramech_lang_spec = {
    .name     = "c",
    .file_ext = ".c",
    .init     = 0,
    .teardown = 0,
    .compile  = 0,
    .invoke   = 0,
};
/* }}} */
