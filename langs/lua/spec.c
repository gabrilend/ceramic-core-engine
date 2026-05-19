/* langs/lua/spec.c — Lua language spec for SoraMech.
 *
 * Per-worker handle is a `lua_State` with the standard libraries
 * opened. Invocation convention: the box's source file is expected
 * to return a Lua table (`return M` at the bottom), and the
 * fn_name is a key into that table. Inputs arrive as byte arrays;
 * each one is pushed as a Lua string. The function's single return
 * value is converted to a string (via tostring semantics) and
 * memcpy'd into the caller's output buffer.
 *
 * Designed in issue 306. Implementation choices that matter:
 *
 *  - One lua_State per worker thread. lua_State is not
 *    thread-safe, but per-thread requires no locking.
 *  - The state's stack is rewound to its baseline depth at every
 *    invoke entry/exit so cross-call leaks don't accumulate.
 *  - File caching (luaL_loadfile once, replayed via lua_call) is
 *    deferred — this iteration loads the chunk on every invoke.
 *    Lands as a follow-on once profiling justifies it.
 *  - The chunk's side effects run on every invoke too (since
 *    we re-load and re-execute). Box source files should be pure
 *    "return M" modules for now.
 */

#include "lang-spec.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <string.h>

/* Registry key for the per-worker file → module-table cache. The
 * key is a string in the Lua registry; the value is a Lua table
 * keyed by file path. Each entry's value is the module table the
 * file's chunk returned.
 *
 * Caching is per-`lua_State` (i.e. per-worker), which matches the
 * thread model: each worker has its own state, never shared. No
 * locking needed because only the worker that owns the state ever
 * touches the cache. */
#define LUA_CACHE_KEY "soramech.module_cache"

/* {{{ lua_init() — create a state, open standard libs, seed cache */
static void *lua_init(int worker_idx)
{
    (void)worker_idx;
    lua_State *L = luaL_newstate();
    if (!L) return NULL;
    luaL_openlibs(L);
    /* Seed the empty cache table. */
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_CACHE_KEY);
    return L;
}
/* }}} */

/* {{{ lua_teardown() */
static void lua_teardown(void *handle)
{
    if (handle) lua_close((lua_State *)handle);
}
/* }}} */

/* {{{ lua_invoke() */
static int lua_invoke(void *handle,
                      const char  *file_path,
                      const char  *fn_name,
                      const void **input_data,
                      const int   *input_sizes,
                      int          n_inputs,
                      void        *out_buf,
                      int          out_buf_capacity,
                      int         *out_size)
{
    lua_State *L = (lua_State *)handle;
    if (!L || !file_path || !fn_name) return -1;

    int baseline = lua_gettop(L);

    /* Module cache lookup. The cache table lives in the registry
     * under LUA_CACHE_KEY; each key is a file path, each value is
     * the module table the file's chunk returned. Stack after this
     * block: ..., cache, module. */
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_CACHE_KEY);
    lua_getfield(L, -1, file_path);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);   /* discard nil */
        /* Stack: ..., cache */

        if (luaL_loadfile(L, file_path) != 0) {
            fprintf(stderr, "lua spec: loadfile('%s'): %s\n",
                    file_path, lua_tostring(L, -1));
            lua_settop(L, baseline);
            return -1;
        }
        if (lua_pcall(L, 0, 1, 0) != 0) {
            fprintf(stderr, "lua spec: chunk exec '%s': %s\n",
                    file_path, lua_tostring(L, -1));
            lua_settop(L, baseline);
            return -1;
        }
        if (!lua_istable(L, -1)) {
            fprintf(stderr, "lua spec: '%s' did not return a table\n", file_path);
            lua_settop(L, baseline);
            return -1;
        }
        /* Stack: ..., cache, module. Cache the module under
         * file_path. */
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, file_path);
    }
    /* Stack at this point: ..., cache, module. */

    /* Pull the function out of the module table. */
    lua_getfield(L, -1, fn_name);
    if (!lua_isfunction(L, -1)) {
        fprintf(stderr, "lua spec: '%s' is not a function in %s\n",
                fn_name, file_path);
        lua_settop(L, baseline);
        return -1;
    }
    /* Stack: ..., cache, module, function. Drop module + cache. */
    lua_remove(L, -2);   /* drop module */
    lua_remove(L, -2);   /* drop cache  */

    /* Push each input as a Lua string. */
    for (int i = 0; i < n_inputs; i++) {
        if (input_data[i] == NULL && input_sizes[i] > 0) {
            fprintf(stderr, "lua spec: input %d is NULL with size %d\n",
                    i, input_sizes[i]);
            lua_settop(L, baseline);
            return -1;
        }
        lua_pushlstring(L, (const char *)input_data[i], (size_t)input_sizes[i]);
    }

    /* Call. One return value expected. */
    if (lua_pcall(L, n_inputs, 1, 0) != 0) {
        fprintf(stderr, "lua spec: %s.%s: %s\n",
                file_path, fn_name, lua_tostring(L, -1));
        lua_settop(L, baseline);
        return -1;
    }

    /* Convert the return to a string. tostring handles numbers /
     * booleans / nil; if the spec needs richer marshaling later
     * (tables, structs), that's an issue-312-shaped change. */
    size_t len = 0;
    const char *result = lua_tolstring(L, -1, &len);
    if (!result) {
        /* Non-coercible value. For now, surface as zero-length
         * output rather than an error — the box returned something
         * we can't serialize, but the call itself succeeded. */
        if (out_size) *out_size = 0;
        lua_settop(L, baseline);
        return 0;
    }
    if ((int)len > out_buf_capacity) {
        fprintf(stderr, "lua spec: %s.%s returned %zu bytes; "
                        "buffer is %d\n",
                file_path, fn_name, len, out_buf_capacity);
        lua_settop(L, baseline);
        return -1;
    }
    if (out_buf && len > 0) memcpy(out_buf, result, len);
    if (out_size) *out_size = (int)len;

    lua_settop(L, baseline);
    return 0;
}
/* }}} */

/* {{{ soramech_lang_spec */
lang_spec_t soramech_lang_spec = {
    .name     = "lua",
    .file_ext = ".lua",
    .init     = lua_init,
    .teardown = lua_teardown,
    .compile  = 0,
    .invoke   = lua_invoke,
};
/* }}} */
