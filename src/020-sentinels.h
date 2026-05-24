/* src/020-sentinels.h — cross-language JSON sentinel primitives.
 *
 * Three reserved JSON object shapes for values JSON can't represent
 * structurally (issue 318):
 *
 *   {"$ref":              {"chunk_ptr": "0x...", "len": N}}
 *   {"$function_pointer": {...}}    -- amendment: wrapper-binary path
 *   {"$lang_opaque":      {"lang": "lua", "tag": N, "shape": "..."}}
 *
 * The producer's spec emits the appropriate sentinel when its
 * encode walker hits a value JSON can't carry. The consumer's spec
 * detects sentinels in the parsed JSON tree and reconstructs the
 * value using language-specific machinery. Wires whose producer
 * may emit a sentinel kind the consumer can't reconstruct surface
 * at compile time as a wire-validation warning.
 *
 * This header exposes:
 *  - the sentinel kind enum + capability bitmask
 *  - JSON shape detection (parse a node, return the kind)
 *  - JSON shape writers (emit canonical bytes for each kind)
 *  - a process-wide $ref bytes store (allocate, lookup, release)
 *  - capability validation helpers used by the compile-time wire
 *    walker
 *
 * Designed and amended in issue 318. Sliced down for one-shot
 * implementation: $lang_opaque and $ref are functional end-to-end
 * for intra-language and in-process cases; $function_pointer is
 * parsed and rejected with a clear "wrapper-binary system not yet
 * implemented" error until the amendment's design lands.
 */
#ifndef SORAMECH_020_SENTINELS_H
#define SORAMECH_020_SENTINELS_H

#include <stddef.h>
#include <stdint.h>

#include "json.h"

/* {{{ Sentinel kinds and capability bitmask */
typedef enum {
    SENTINEL_NONE             = 0,
    SENTINEL_REF              = 1,
    SENTINEL_LANG_OPAQUE      = 2,
    SENTINEL_FUNCTION_POINTER = 3,
} sentinel_kind_t;

/* Capability bits combined into a per-spec mask. A spec's
 * `sentinel_emit_mask` says which kinds its native_to_json may
 * produce; its `sentinel_reconstruct_mask` says which kinds its
 * json_to_native can rebuild. */
#define SENTINEL_BIT(K)            (1u << (unsigned)(K))
#define SENTINEL_MASK_REF          SENTINEL_BIT(SENTINEL_REF)
#define SENTINEL_MASK_LANG_OPAQUE  SENTINEL_BIT(SENTINEL_LANG_OPAQUE)
#define SENTINEL_MASK_FN_POINTER   SENTINEL_BIT(SENTINEL_FUNCTION_POINTER)
/* }}} */

/* {{{ Detection — parse-side
 *
 * Returns the sentinel kind for a JSON object that has exactly one
 * top-level key matching a sentinel name, or SENTINEL_NONE for
 * anything else (including arrays / scalars / multi-key objects). */
sentinel_kind_t sentinel_detect(const json_node_t *node);
/* }}} */

/* {{{ Emission — write-side helpers
 *
 * Each writer emits the canonical bytes for the kind. The producing
 * spec calls these from inside its encode walker; the writer leaves
 * the cursor where any surrounding context expects it. */
void sentinel_write_ref         (json_writer_t *w,
                                 uintptr_t chunk_ptr, int len);
void sentinel_write_lang_opaque (json_writer_t *w,
                                 const char *lang, uint64_t tag,
                                 const char *shape);

/* The function-pointer shape lands when the amendment's wrapper-
 * binary system is implemented. Until then this writer emits a
 * sentinel that will refuse to reconstruct, with a self-describing
 * stub payload that future fixture data can be migrated from. */
void sentinel_write_function_pointer_stub(json_writer_t *w,
                                          const char *signature);
/* }}} */

/* {{{ $ref bytes store (process-wide, leak-per-run)
 *
 * `sentinel_ref_alloc` copies `bytes` into a process-wide pool and
 * returns a chunk pointer that the producer embeds in its sentinel.
 * The consumer's `sentinel_ref_lookup` returns the same byte
 * pointer.
 *
 * Slice-1 lifetime model: bytes live until process exit (or until
 * `sentinel_ref_store_clear()` is called by the test harness).
 * Proper refcount-driven release tied to consumer reads is a
 * follow-on — the issue documents the lifetime concern but the
 * simpler shape unblocks end-to-end testing today. */
uintptr_t        sentinel_ref_alloc (const void *bytes, int len);
const void      *sentinel_ref_lookup(uintptr_t chunk_ptr, int *out_len);
void             sentinel_ref_store_clear(void);
/* }}} */

/* {{{ Capability validation
 *
 * Returns 0 if every emit bit set on `producer_mask` also appears
 * in `consumer_mask`; returns the offending bit (>0) if any kind
 * the producer may emit is unreconstructable by the consumer. The
 * compile-time wire walker uses this to surface mismatches. */
unsigned int sentinel_validate(unsigned int producer_mask,
                               unsigned int consumer_mask);

/* Human-readable name of a sentinel kind for diagnostics. */
const char *sentinel_kind_name(sentinel_kind_t k);
/* }}} */

#endif /* SORAMECH_020_SENTINELS_H */
