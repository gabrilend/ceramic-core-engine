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
#include "020-sentinels.h"          /* sentinel emit / detect — 318 */

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

/* Forward declarations — these live further down, but lua_invoke
 * needs them. parse_json_to_stack and encode_value: slices 4 and
 * 4.5 of issue 312 (per-input native/JSON branch, per-call output
 * format). load_via_merged: issue 313 (look up the per-file
 * module inside the compile-time merged module if present). */
static int parse_json_to_stack(lua_State *L,
                               const void *src, int src_size,
                               int verbose);
static int encode_value(lua_State *L, int abs_idx, json_writer_t *w);
static int load_via_merged(lua_State *L, const char *file_path);

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
                      const box_t *box,
                      const char  *file_path,
                      const char  *fn_name,
                      const void **input_data,
                      const int   *input_sizes,
                      const int   *input_native,
                      int          n_inputs,
                      int          output_native,
                      void        *out_buf,
                      int          out_buf_capacity,
                      int         *out_size)
{
    /* The Lua spec doesn't yet read box-level fields — its encoder
     * is type-agnostic. Accepted for signature uniformity. */
    (void)box;
    /* Slice 4 of issue 312: per-input format dispatch. NATIVE inputs
     * become Lua strings (raw bytes the function reads directly);
     * JSON inputs are parsed and pushed as the resulting Lua value,
     * with a silent fallback to raw bytes when the bytes don't
     * actually parse (some cross-language producers don't yet emit
     * real JSON; the fallback keeps them working until they do).
     *
     * Slice 4.5: per-call output format dispatch. When `output_native`
     * is non-zero the spec writes the return value via the existing
     * lua_tolstring coercion (raw bytes for strings / numbers /
     * booleans / nil; reads as a Lua string at a same-language
     * consumer). When `output_native` is zero the spec serializes
     * the return via the same encoder that powers native_to_json,
     * so cross-language consumers receive a parseable JSON value
     * (tables become JSON objects / arrays; non-JSON-encodable
     * values surface a clear error). The dispatch decided which
     * mode to ask for based on whether every downstream consumer
     * shares this box's language. */
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

        /* Issue 313 — whole-program same-language merge. If the
         * compile pipeline produced a merged module
         * (`<dir>/__merged__.lua`) alongside this file, route
         * through it: load the merged module once per worker,
         * look up the per-file sub-module by basename, cache that
         * under file_path. Subsequent invokes for the same box
         * hit the cache and skip both the merged-load and the
         * per-file resolution. */
        if (load_via_merged(L, file_path) != 0) {
            /* No merged file present (or it failed to load). Fall
             * back to the per-file lazy load — same code path the
             * spec has always used. */
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

    /* Push each input. Per-input format flag picks the decoder:
     * native bytes go as Lua strings (the function reads them
     * directly); JSON bytes are parsed into Lua values. On JSON
     * parse failure we silently fall back to the raw-string path —
     * this keeps backward compatibility with cross-language
     * producers that still emit non-JSON bytes (the C and Bash
     * specs' output-side JSON encoding lands later in issues 307 /
     * 308). When `input_native` is NULL (slice 2 behaviour, kept
     * for callers that don't supply it) every input is treated as
     * native. */
    for (int i = 0; i < n_inputs; i++) {
        if (input_data[i] == NULL && input_sizes[i] > 0) {
            fprintf(stderr, "lua spec: input %d is NULL with size %d\n",
                    i, input_sizes[i]);
            lua_settop(L, baseline);
            return -1;
        }
        int is_native = (input_native == NULL) || (input_native[i] != 0);
        if (is_native) {
            /* Issue 323: tables now ride same-language wires as JSON
             * (see the output path below), so a native input that
             * looks structured — first byte '{' or '[' — is parsed
             * back into a real Lua value. Primitives keep the raw-
             * string fast path untouched. Parse failure falls back
             * to raw bytes so a legitimate brace-leading string
             * still arrives; issue 325's per-pair shims replace
             * this sniff with an explicit declaration. */
            const char *bytes = (const char *)input_data[i];
            int structured = input_sizes[i] > 0 &&
                             (bytes[0] == '{' || bytes[0] == '[');
            if (!structured ||
                parse_json_to_stack(L, bytes,
                                    input_sizes[i], /*verbose=*/0) != 0) {
                lua_pushlstring(L, bytes, (size_t)input_sizes[i]);
            }
        } else {
            /* Cross-language input: try JSON. On parse failure,
             * push the raw bytes as a string so existing
             * non-JSON-emitting producers keep working. */
            if (input_sizes[i] <= 0 ||
                parse_json_to_stack(L, input_data[i],
                                    input_sizes[i], /*verbose=*/0) != 0) {
                lua_pushlstring(L, (const char *)input_data[i],
                                (size_t)input_sizes[i]);
            }
        }
    }

    /* Call. One return value expected. */
    if (lua_pcall(L, n_inputs, 1, 0) != 0) {
        fprintf(stderr, "lua spec: %s.%s: %s\n",
                file_path, fn_name, lua_tostring(L, -1));
        lua_settop(L, baseline);
        return -1;
    }

    /* Slice 4.5 of issue 312 (now coordinated with 307): when the
     * dispatch tells us at least one downstream consumer is in a
     * different language (`output_native == 0`), serialize via the
     * same JSON encoder that powers the native_to_json bridge.
     * Cross-language consumers now know how to unwrap JSON
     * primitives on their input side (Lua's parse-with-fallback,
     * C's try_strip_json_primitive); tables / arrays become real
     * JSON in the wire and consumer-side typed wrappers (the
     * deferred half of 307) project them into typed values. When
     * `output_native == 1` (every consumer same-language), the
     * fast tostring coercion still runs. */
    if (!output_native) {
        json_writer_t w;
        json_writer_init(&w, (char *)out_buf, out_buf_capacity);
        int top = lua_gettop(L);
        if (encode_value(L, top, &w) != 0) {
            fprintf(stderr, "lua spec: %s.%s: JSON-encoding return failed\n",
                    file_path, fn_name);
            lua_settop(L, baseline);
            return -1;
        }
        int n = json_writer_finish(&w);
        if (n < 0) {
            fprintf(stderr, "lua spec: %s.%s: JSON output (%d bytes) "
                            "exceeds buffer\n",
                    file_path, fn_name, out_buf_capacity);
            lua_settop(L, baseline);
            return -1;
        }
        if (out_size) *out_size = n;
        lua_settop(L, baseline);
        return 0;
    }

    /* Fast same-language path: tostring coercion for primitives.
     * NOTE — the comment that used to sit here claimed tables "come
     * back as 'table: 0x...'". That was wrong: raw lua_tolstring
     * returns NULL for a table (the 'table: 0x...' string is what
     * luaL_tolstring / tostring() produce), and the NULL branch
     * below reported *success* with a zero-byte wire. That was bug
     * 323: every table return in an all-Lua graph silently
     * evaporated after one hop. Tables now take the same JSON
     * encoder as the cross-language path; the consumer's native
     * input sniff (above) rebuilds the value. Issue 313's
     * whole-program merge remains the by-reference ceiling, and
     * issue 325 reframes this pairing as an explicitly declared
     * Lua→Lua shim. */
    if (lua_istable(L, -1)) {
        json_writer_t w;
        json_writer_init(&w, (char *)out_buf, out_buf_capacity);
        int top = lua_gettop(L);
        if (encode_value(L, top, &w) != 0) {
            fprintf(stderr, "lua spec: %s.%s: JSON-encoding table return "
                            "on same-language wire failed\n",
                    file_path, fn_name);
            lua_settop(L, baseline);
            return -1;
        }
        int n = json_writer_finish(&w);
        if (n < 0) {
            fprintf(stderr, "lua spec: %s.%s: JSON output (%d bytes) "
                            "exceeds buffer\n",
                    file_path, fn_name, out_buf_capacity);
            lua_settop(L, baseline);
            return -1;
        }
        if (out_size) *out_size = n;
        lua_settop(L, baseline);
        return 0;
    }
    size_t len = 0;
    const char *result = lua_tolstring(L, -1, &len);
    if (!result) {
        /* Non-coercible, non-table return (nil / boolean / function
         * / userdata). Zero bytes stays the contract for a no-value
         * return, but per 323's "never silent" rule the drop now
         * announces itself on stderr instead of passing unseen. */
        fprintf(stderr, "lua spec: %s.%s: %s return coerced to empty "
                        "output on same-language wire\n",
                file_path, fn_name, lua_typename(L, lua_type(L, -1)));
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

/* {{{ load_via_merged() — issue 313 whole-program same-language merge
 *
 * If a `__merged__.lua` exists next to `file_path`, treat it as the
 * compile-time merged module produced by soramech-compile.sh. The
 * merged module's top-level returns a table keyed by each
 * source-file basename, with each value being that file's module
 * table. We load the merged module once per worker (cached under
 * a registry key), then index it by the box's source-file basename
 * to recover the per-file module. Leaves exactly one value (the
 * per-file module) on the Lua stack on success.
 *
 * Returns 0 on success (per-file module pushed), -1 if the merged
 * file isn't present or doesn't yield a per-file module for this
 * basename — the caller falls back to per-file lazy loading. The
 * stack is unchanged on -1. */
#define LUA_MERGED_CACHE_KEY "soramech.merged_module"

static int load_via_merged(lua_State *L, const char *file_path)
{
    if (!file_path) return -1;
    /* Find the last '/' to split directory from basename. */
    const char *slash = strrchr(file_path, '/');
    if (!slash) return -1;          /* bare basename, no dir to look in */
    size_t dirlen = (size_t)(slash - file_path);

    /* Build the candidate merged path. Bounded by PATH-ish 4 KB. */
    char merged_path[4096];
    if (dirlen + sizeof("/__merged__.lua") + 1 > sizeof merged_path) return -1;
    memcpy(merged_path, file_path, dirlen);
    memcpy(merged_path + dirlen, "/__merged__.lua",
           sizeof("/__merged__.lua"));   /* includes trailing NUL */

    /* Cheap existence check before paying load cost. fopen rather
     * than stat keeps the dependency surface small. */
    FILE *probe = fopen(merged_path, "r");
    if (!probe) return -1;
    fclose(probe);

    /* Derive the basename (filename without `.lua`) the merged
     * module's table is keyed by. */
    const char *base_start = slash + 1;
    const char *dot = strrchr(base_start, '.');
    size_t baselen = dot ? (size_t)(dot - base_start) : strlen(base_start);
    if (baselen == 0 || baselen >= 256) return -1;
    char basename[256];
    memcpy(basename, base_start, baselen);
    basename[baselen] = '\0';

    int baseline = lua_gettop(L);

    /* Cache the merged module under a registry key keyed by the
     * merged path. Subsequent same-merge lookups skip the load. */
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_MERGED_CACHE_KEY);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, LUA_MERGED_CACHE_KEY);
    }
    /* Stack: ..., merged_cache */

    lua_getfield(L, -1, merged_path);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        if (luaL_loadfile(L, merged_path) != 0) {
            fprintf(stderr, "lua spec: load_via_merged: loadfile('%s'): %s\n",
                    merged_path, lua_tostring(L, -1));
            lua_settop(L, baseline);
            return -1;
        }
        if (lua_pcall(L, 0, 1, 0) != 0) {
            fprintf(stderr, "lua spec: load_via_merged: exec '%s': %s\n",
                    merged_path, lua_tostring(L, -1));
            lua_settop(L, baseline);
            return -1;
        }
        if (!lua_istable(L, -1)) {
            fprintf(stderr, "lua spec: load_via_merged: '%s' did not return a table\n",
                    merged_path);
            lua_settop(L, baseline);
            return -1;
        }
        /* Stack: ..., merged_cache, merged_module. Cache it. */
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, merged_path);
    }
    /* Stack: ..., merged_cache, merged_module */

    /* Look up the per-file sub-module by basename. */
    lua_getfield(L, -1, basename);
    if (!lua_istable(L, -1)) {
        /* The merged file exists but doesn't carry this basename —
         * unusual, but caller falls back to per-file load. */
        lua_settop(L, baseline);
        return -1;
    }
    /* Stack: ..., merged_cache, merged_module, per_file_module.
     * Drop the merged_cache and merged_module — leave only the
     * per-file module above the original baseline. */
    lua_remove(L, -2);   /* drop merged_module */
    lua_remove(L, -2);   /* drop merged_cache */
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
        /* n-field convention (issue 317, fidelity directive):
         *
         * If the table carries `n` as a non-negative integer, that
         * field declares "this is a JSON array of length n; every
         * missing integer key in 1..n is an explicit null on the
         * wire." That's how the decoder records arrays it pushed
         * onto the stack — a full round-trip of [true,false,null]
         * leaves `{n=3, [1]=true, [2]=false}` here, and we have to
         * emit `[true,false,null]` from it. Idiomatic Lua tables
         * (no n field) still take the existing heuristic path.
         *
         * The intrusion of an explicit `n` field on values pushed
         * by the decoder is the price of preserving JSON fidelity
         * through Lua's nil-as-absence storage model. Documented
         * in issue 317. */
        lua_getfield(L, abs_idx, "n");
        int  has_declared_len = 0;
        long declared_len     = 0;
        if (lua_type(L, -1) == LUA_TNUMBER) {
            lua_Number nn = lua_tonumber(L, -1);
            long       ni = (long)nn;
            if ((lua_Number)ni == nn && ni >= 0) {
                has_declared_len = 1;
                declared_len     = ni;
            }
        }
        lua_pop(L, 1);

        if (has_declared_len) {
            json_writer_array(w);
            for (long i = 1; i <= declared_len; i++) {
                lua_rawgeti(L, abs_idx, (int)i);
                int top = lua_gettop(L);
                /* Missing keys land here as nil and encode_value
                 * emits json null for them — exactly the round-trip
                 * behavior the n-field convention promises. */
                int r = encode_value(L, top, w);
                lua_pop(L, 1);
                if (r != 0) return -1;
            }
            json_writer_end(w);
            return 0;
        }

        /* No n field — fall back to the existing heuristic: every
         * key in 1..N and no others → array; anything else → object.
         * Empty tables go through the object path because JSON `[]`
         * vs `{}` is a distinction Lua can't natively express, and
         * the n-field convention is the way to ask for `[]`. */
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
    default: {
        /* Function / userdata / thread / lightuserdata land here.
         * Issue 318: instead of hard-erroring, register the value in
         * this Lua state's registry and emit a $lang_opaque sentinel
         * the same-language consumer can resolve back. Cross-language
         * consumers see a sentinel they can't reconstruct and fail
         * cleanly at decode time. */
        const char *shape = lua_typename(L, t);
        /* luaL_ref pops the top of stack and stores it under a fresh
         * integer key in the registry. We need to push a copy first
         * since encode_value must NOT modify the source stack
         * position. */
        lua_pushvalue(L, abs_idx);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        if (ref == LUA_REFNIL) {
            fprintf(stderr,
                "lua spec: failed to register opaque value of type %s\n",
                shape);
            return -1;
        }
        sentinel_write_lang_opaque(w, "lua", (uint64_t)ref, shape);
        return 0;
    }
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
        /* Push as `{n = N, [1] = ..., [2] = ..., ...}` so the
         * encoder's n-field path can reconstruct the array
         * faithfully on the way out, including any embedded or
         * trailing nulls. `lua_rawseti` on a JSON `null` (which
         * decode_node pushed as Lua nil) is effectively a key
         * deletion — which is precisely the right representation
         * for "missing slot, fill with null at encode time." */
        int n = json_array_size(node);
        lua_createtable(L, n, 1);
        lua_pushinteger(L, n);
        lua_setfield(L, -2, "n");
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
        /* Issue 318: sentinel detection. A single-key object matching
         * one of the reserved sentinel keys gets routed through the
         * reconstruct path for that kind instead of materialising as
         * a plain Lua table with `$ref` / `$lang_opaque` /
         * `$function_pointer` as a literal key. */
        sentinel_kind_t skind = sentinel_detect(node);
        if (skind == SENTINEL_LANG_OPAQUE) {
            const json_node_t *inner = json_object_value(node, 0);
            const json_node_t *lang_n = inner ? json_object_get(inner, "lang") : NULL;
            const json_node_t *tag_n  = inner ? json_object_get(inner, "tag")  : NULL;
            if (!lang_n || json_kind((json_node_t *)lang_n) != JSON_STRING ||
                !tag_n  || json_kind((json_node_t *)tag_n)  != JSON_NUMBER) {
                fprintf(stderr, "lua spec: malformed $lang_opaque\n");
                lua_settop(L, baseline);
                return -1;
            }
            const char *lang = json_string_value(lang_n);
            if (strcmp(lang, "lua") != 0) {
                fprintf(stderr,
                    "lua spec: $lang_opaque lang='%s' is not reconstructable "
                    "by the Lua spec (cross-language opaque values require "
                    "matching producer/consumer language)\n", lang);
                lua_settop(L, baseline);
                return -1;
            }
            int ref = (int)json_number_value(tag_n);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (lua_isnil(L, -1)) {
                fprintf(stderr, "lua spec: $lang_opaque tag %d not in registry\n", ref);
                lua_settop(L, baseline);
                return -1;
            }
            return 0;
        }
        if (skind == SENTINEL_REF) {
            const json_node_t *inner = json_object_value(node, 0);
            const json_node_t *ptr_n = inner ? json_object_get(inner, "chunk_ptr") : NULL;
            if (!ptr_n || json_kind((json_node_t *)ptr_n) != JSON_STRING) {
                fprintf(stderr, "lua spec: malformed $ref\n");
                lua_settop(L, baseline);
                return -1;
            }
            uintptr_t ptr = (uintptr_t)strtoull(json_string_value(ptr_n), NULL, 0);
            int len = 0;
            const void *bytes = sentinel_ref_lookup(ptr, &len);
            if (!bytes) {
                fprintf(stderr, "lua spec: $ref chunk_ptr=0x%lx not in store\n",
                        (unsigned long)ptr);
                lua_settop(L, baseline);
                return -1;
            }
            lua_pushlstring(L, (const char *)bytes, (size_t)len);
            return 0;
        }
        if (skind == SENTINEL_FUNCTION_POINTER) {
            fprintf(stderr, "lua spec: $function_pointer reconstruction needs "
                            "the wrapper-binary subsystem (issue 318 amendment), "
                            "not yet implemented\n");
            lua_settop(L, baseline);
            return -1;
        }
        /* Plain object — fall through to the existing table path. */
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

/* {{{ parse_json_to_stack() — shared JSON→Lua-stack helper */
/* Parses `src` (length `src_size`, not necessarily NUL-terminated)
 * as JSON and pushes the resulting value onto L. Returns 0 on
 * success, -1 on parse failure. When `verbose`, parse errors emit a
 * stderr message — used by the bridge (where a parse failure is a
 * cross-language wire error worth surfacing). When `verbose` is 0,
 * parse failures are silent — used by lua_invoke's per-input branch
 * where the caller falls back to raw-bytes-as-string if JSON parsing
 * fails (some cross-language producers don't emit real JSON yet —
 * keeps backward compatibility while the input-format contract
 * tightens).
 *
 * On failure the Lua stack is unchanged. On success exactly one
 * value is added. The transient JSON arena is created and freed
 * inside this call. */
static int parse_json_to_stack(lua_State *L,
                               const void *src, int src_size,
                               int verbose)
{
    if (!L || !src || src_size <= 0) return -1;

    char *scratch = (char *)malloc((size_t)src_size + 1);
    if (!scratch) {
        if (verbose) fprintf(stderr, "lua spec: parse_json_to_stack: out of memory\n");
        return -1;
    }
    memcpy(scratch, src, (size_t)src_size);
    scratch[src_size] = '\0';

    json_arena_t *arena = json_arena_create();
    if (!arena) {
        free(scratch);
        if (verbose) fprintf(stderr, "lua spec: parse_json_to_stack: arena allocation failed\n");
        return -1;
    }

    int err_offset = 0;
    const char *err_msg = NULL;
    json_node_t *node = json_parse(arena, scratch, &err_offset, &err_msg);
    free(scratch);
    if (!node) {
        if (verbose) fprintf(stderr, "lua spec: parse_json_to_stack: parse error at byte %d: %s\n",
                             err_offset, err_msg ? err_msg : "(no message)");
        json_arena_destroy(arena);
        return -1;
    }

    int r = decode_node(L, node);
    json_arena_destroy(arena);
    return r;
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
    /* The bridge version is loud on parse failure (this is the
     * cross-language path; if the bytes don't parse, the wire is
     * wrong and the user should see why). */
    return parse_json_to_stack(L, src, src_size, /*verbose=*/1);
}
/* }}} */

/* {{{ lua_translate() — per-port custom translation shim (issue 246)
 *
 * The shim is a .lua file the user wrote. It is expected to return
 * a single function with the signature
 *
 *   function(raw_bytes, raw_native) -> translated_bytes
 *
 * Cached per-worker in a registry table keyed by shim path so
 * repeat invocations skip the load. The cache table is created
 * lazily on first use. */
#define LUA_SHIM_CACHE_KEY "soramech.shim_cache"

static int lua_translate(void *handle,
                         const char *shim_path,
                         const void *raw, int raw_size, int raw_native,
                         void *out_buf, int out_capacity, int *out_size)
{
    lua_State *L = (lua_State *)handle;
    if (!L || !shim_path) return -1;
    int baseline = lua_gettop(L);

    /* Find or create the cache table. */
    lua_getfield(L, LUA_REGISTRYINDEX, LUA_SHIM_CACHE_KEY);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, LUA_SHIM_CACHE_KEY);
    }
    /* Stack: ..., cache */

    /* Cache miss → load. */
    lua_getfield(L, -1, shim_path);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        if (luaL_loadfile(L, shim_path) != 0) {
            fprintf(stderr, "lua spec: shim loadfile('%s'): %s\n",
                    shim_path, lua_tostring(L, -1));
            lua_settop(L, baseline);
            return -1;
        }
        if (lua_pcall(L, 0, 1, 0) != 0) {
            fprintf(stderr, "lua spec: shim chunk exec '%s': %s\n",
                    shim_path, lua_tostring(L, -1));
            lua_settop(L, baseline);
            return -1;
        }
        if (!lua_isfunction(L, -1)) {
            fprintf(stderr, "lua spec: shim '%s' did not return a function\n",
                    shim_path);
            lua_settop(L, baseline);
            return -1;
        }
        /* Cache: store function under the path. */
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, shim_path);
    }
    /* Stack: ..., cache, fn */

    /* Call fn(raw_bytes, raw_native) → translated string. */
    lua_pushlstring(L, (const char *)raw, (size_t)raw_size);
    lua_pushboolean(L, raw_native ? 1 : 0);
    if (lua_pcall(L, 2, 1, 0) != 0) {
        fprintf(stderr, "lua spec: shim '%s' call: %s\n",
                shim_path, lua_tostring(L, -1));
        lua_settop(L, baseline);
        return -1;
    }
    /* Stack: ..., cache, result */

    size_t out_len = 0;
    const char *out_ptr = NULL;
    if (lua_isstring(L, -1)) {
        out_ptr = lua_tolstring(L, -1, &out_len);
    } else if (lua_isnumber(L, -1)) {
        out_ptr = lua_tolstring(L, -1, &out_len);  /* coerces */
    } else if (lua_isnil(L, -1)) {
        out_len = 0;
        out_ptr = "";
    } else {
        fprintf(stderr, "lua spec: shim '%s' returned non-string\n", shim_path);
        lua_settop(L, baseline);
        return -1;
    }
    if ((int)out_len > out_capacity) {
        fprintf(stderr, "lua spec: shim '%s' output %zu > capacity %d\n",
                shim_path, out_len, out_capacity);
        lua_settop(L, baseline);
        return -1;
    }
    if (out_len > 0) memcpy(out_buf, out_ptr, out_len);
    if (out_size) *out_size = (int)out_len;
    lua_settop(L, baseline);
    return 0;
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
    .translate      = lua_translate,
    /* Issue 318: Lua spec emits and reconstructs $ref and
     * $lang_opaque. $function_pointer is parsed but rejected
     * pending the amendment's wrapper-binary subsystem. */
    .sentinel_emit_mask        = SENTINEL_MASK_REF | SENTINEL_MASK_LANG_OPAQUE,
    .sentinel_reconstruct_mask = SENTINEL_MASK_REF | SENTINEL_MASK_LANG_OPAQUE,
    /* Issue 325: Lua values translate to C and Bash consumers via
     * the JSON encoder — an explicit per-pair declaration (JSON is
     * the chosen shim for these pairs, not an implicit fallback). */
    .translate_targets         = (const char *const[]){ "c", "bash", NULL },
};
/* }}} */
