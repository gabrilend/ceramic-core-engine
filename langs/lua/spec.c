/* langs/lua/spec.c — Lua language spec for SoraMech.
 *
 * What it is, in a sentence: lets the pool runner call Lua box
 * functions in-process by holding one lua_State per worker thread.
 *
 * Designed in issue 306. Current state: stub — exports the spec
 * symbol with name/file_ext set and all callbacks NULL, so the
 * registry can dlopen this and verify the build pipeline. The real
 * implementation (lua_State per worker, luaL_loadfile cache,
 * pcall + serialize) lands when 306 is implemented.
 */

#include "lang-spec.h"

/* {{{ soramech_lang_spec */
lang_spec_t soramech_lang_spec = {
    .name     = "lua",
    .file_ext = ".lua",
    .init     = 0,
    .teardown = 0,
    .compile  = 0,
    .invoke   = 0,
};
/* }}} */
