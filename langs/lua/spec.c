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
#include "json.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdio.h>
#include <stdlib.h>
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

/* {{{ encode_value() — write one Lua value at `abs_idx` as JSON */
/* Walk the value at absolute stack index `abs_idx` and emit JSON
 * via the streaming writer `w`. Recurses into tables. Stack-neutral:
 * the value at abs_idx stays put (we only push/pop scratch keys
 * during table traversal). Returns 0 ok, -1 on unsupported type or
 * writer truncation. The "unsupported type" path is loud — function,
 * userdata, thread, lightuserdata produce a stderr error rather
 * than a silent fallback (project convention forbids those). The
 * richer primitives — $ref, $function_pointer, $lang_opaque — land
 * later; this first cut only handles the JSON-clean scalars and
 * containers. */
static int encode_value(lua_State *L, int abs_idx, json_writer_t *w)
{
    int t = lua_type(L, abs_idx);
    switch (t) {
    case LUA_TNIL:
        json_writer_null(w);
        return 0;
    case LUA_TBOOLEAN:
        json_writer_bool(w, lua_toboolean(L, abs_idx));
        return 0;
    case LUA_TNUMBER: {
        /* Integer-shaped numbers go via writer_int so JSON doesn't
         * show "3.0" where "3" reads better. The float path uses
         * %g internally inside the writer. */
        lua_Number n = lua_tonumber(L, abs_idx);
        long long  i = (long long)n;
        if ((lua_Number)i == n) json_writer_int(w, i);
        else                    json_writer_number(w, (double)n);
        return 0;
    }
    case LUA_TSTRING:
        json_writer_string(w, lua_tostring(L, abs_idx));
        return 0;
    case LUA_TTABLE: {
        /* Array vs object: we treat a table as an array when its
         * # length is positive AND a full key walk turns up exactly
         * that many entries (no holes, no extra string keys). Empty
         * tables encode as {} — a deliberate choice; JSON's [] vs {}
         * distinction has no Lua equivalent and the object form
         * keeps round-trips honest when downstream code expects a
         * map. Future: an explicit array marker (issue 317 follow-up)
         * if this becomes a footgun. */
        int n = (int)lua_objlen(L, abs_idx);
        int total = 0;
        lua_pushnil(L);
        while (lua_next(L, abs_idx) != 0) {
            total++;
            lua_pop(L, 1);
        }
        int is_array = (n > 0 && total == n);

        if (is_array) {
            json_writer_array(w);
            for (int i = 1; i <= n; i++) {
                lua_rawgeti(L, abs_idx, i);
                int top = lua_gettop(L);
                int r = encode_value(L, top, w);
                lua_pop(L, 1);
                if (r != 0) return -1;
            }
            json_writer_end(w);
        } else {
            json_writer_object(w);
            lua_pushnil(L);
            while (lua_next(L, abs_idx) != 0) {
                /* Key at -2, value at -1. We must NOT call
                 * lua_tostring on the key in-place because that
                 * would coerce numbers to strings and corrupt
                 * lua_next's iteration state — render numeric keys
                 * into a local buffer instead. */
                int kt = lua_type(L, -2);
                if (kt == LUA_TSTRING) {
                    json_writer_key(w, lua_tostring(L, -2));
                } else if (kt == LUA_TNUMBER) {
                    char keybuf[32];
                    lua_Number kn = lua_tonumber(L, -2);
                    long long  ki = (long long)kn;
                    if ((lua_Number)ki == kn) snprintf(keybuf, sizeof(keybuf), "%lld", ki);
                    else                      snprintf(keybuf, sizeof(keybuf), "%g", (double)kn);
                    json_writer_key(w, keybuf);
                } else {
                    fprintf(stderr, "lua spec: table key of type %s is not JSON-encodable\n",
                            lua_typename(L, kt));
                    lua_pop(L, 2);
                    return -1;
                }
                int top = lua_gettop(L);   /* points at the value */
                int r = encode_value(L, top, w);
                lua_pop(L, 1);             /* drop value, keep key for lua_next */
                if (r != 0) {
                    lua_pop(L, 1);          /* drop the key too */
                    return -1;
                }
            }
            json_writer_end(w);
        }
        return 0;
    }
    default:
        /* Function / userdata / thread / lightuserdata. The richer
         * wire primitives ($function_pointer, $lang_opaque) decompose
         * these into data the destination can rebuild, but that
         * decomposition lands as a follow-on. Today: hard error. */
        fprintf(stderr, "lua spec: cannot JSON-encode value of type %s\n",
                lua_typename(L, t));
        return -1;
    }
}
/* }}} */

/* {{{ lua_native_to_json() — top-of-stack Lua value → JSON bytes */
/* Per the bridge stack-protocol convention (lang-spec.h): the
 * producer leaves its native value at the top of its working store.
 * For Lua, that's the top of `lua_State`. The bridge consumes it —
 * one pop on success, on failure the stack is left untouched so the
 * caller can inspect / log before tearing down. */
static int lua_native_to_json(void *handle,
                              const void *src, int src_size,
                              void *dst, int dst_capacity, int *dst_size)
{
    (void)src;
    (void)src_size;
    lua_State *L = (lua_State *)handle;
    if (!L || !dst || dst_capacity <= 0) return -1;
    if (lua_gettop(L) < 1) {
        fprintf(stderr, "lua spec: native_to_json called with empty stack\n");
        return -1;
    }

    json_writer_t w;
    json_writer_init(&w, (char *)dst, dst_capacity);

    int top = lua_gettop(L);
    if (encode_value(L, top, &w) != 0) return -1;

    int n = json_writer_finish(&w);
    if (n < 0) {
        fprintf(stderr, "lua spec: native_to_json: output buffer (%d bytes) too small\n",
                dst_capacity);
        return -1;
    }
    if (dst_size) *dst_size = n;
    lua_pop(L, 1);
    return 0;
}
/* }}} */

/* {{{ decode_node() — push one JSON node onto the Lua stack */
/* Recursive walk over the parsed DOM. Each call adds exactly one
 * value to the stack on success; on failure the stack is wound back
 * to where this call started so partial decodes don't pollute it. */
static int decode_node(lua_State *L, const json_node_t *node)
{
    int baseline = lua_gettop(L);
    switch (json_kind(node)) {
    case JSON_NULL:
        lua_pushnil(L);
        return 0;
    case JSON_BOOL:
        lua_pushboolean(L, json_bool_value(node));
        return 0;
    case JSON_NUMBER:
        lua_pushnumber(L, json_number_value(node));
        return 0;
    case JSON_STRING:
        lua_pushstring(L, json_string_value(node));
        return 0;
    case JSON_ARRAY: {
        int n = json_array_size(node);
        lua_createtable(L, n, 0);
        for (int i = 0; i < n; i++) {
            if (decode_node(L, json_array_at(node, i)) != 0) {
                lua_settop(L, baseline);
                return -1;
            }
            lua_rawseti(L, -2, i + 1);
        }
        return 0;
    }
    case JSON_OBJECT: {
        int n = json_object_size(node);
        lua_createtable(L, 0, n);
        for (int i = 0; i < n; i++) {
            if (decode_node(L, json_object_value(node, i)) != 0) {
                lua_settop(L, baseline);
                return -1;
            }
            lua_setfield(L, -2, json_object_key(node, i));
        }
        return 0;
    }
    }
    fprintf(stderr, "lua spec: decode_node: unknown JSON kind\n");
    lua_settop(L, baseline);
    return -1;
}
/* }}} */

/* {{{ lua_json_to_native() — JSON bytes → push value onto Lua stack */
/* The bridge parses `src` into a transient arena, walks the DOM
 * onto the lua_State stack, then frees the arena. The arena is
 * created and destroyed inside the call — no per-bridge cache yet.
 * If parse-time perf shows up in profiles, we can stash an arena
 * inside the worker handle later.
 *
 * `src` may not be NUL-terminated (the caller passes us a slice
 * out of a wire buffer), so we copy into a local NUL-terminated
 * scratch before parsing — json_parse requires NUL termination
 * (see libs/json/json.h). */
static int lua_json_to_native(void *handle,
                              const void *src, int src_size,
                              void *dst, int dst_capacity, int *dst_size)
{
    (void)dst;
    (void)dst_capacity;
    (void)dst_size;
    lua_State *L = (lua_State *)handle;
    if (!L || !src || src_size <= 0) return -1;

    char *scratch = (char *)malloc((size_t)src_size + 1);
    if (!scratch) {
        fprintf(stderr, "lua spec: json_to_native: out of memory\n");
        return -1;
    }
    memcpy(scratch, src, (size_t)src_size);
    scratch[src_size] = '\0';

    json_arena_t *arena = json_arena_create();
    if (!arena) {
        free(scratch);
        fprintf(stderr, "lua spec: json_to_native: arena allocation failed\n");
        return -1;
    }

    int err_offset = 0;
    const char *err_msg = NULL;
    json_node_t *node = json_parse(arena, scratch, &err_offset, &err_msg);
    free(scratch);
    if (!node) {
        fprintf(stderr, "lua spec: json_to_native: parse error at byte %d: %s\n",
                err_offset, err_msg ? err_msg : "(no message)");
        json_arena_destroy(arena);
        return -1;
    }

    int r = decode_node(L, node);
    json_arena_destroy(arena);
    return r;
}
/* }}} */

/* {{{ soramech_lang_spec */
lang_spec_t soramech_lang_spec = {
    .name           = "lua",
    .file_ext       = ".lua",
    .init           = lua_init,
    .teardown       = lua_teardown,
    .compile        = 0,
    .invoke         = lua_invoke,
    .native_to_json = lua_native_to_json,
    .json_to_native = lua_json_to_native,
};
/* }}} */
