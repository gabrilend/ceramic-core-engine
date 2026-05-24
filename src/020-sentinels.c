/* src/020-sentinels.c — sentinel primitives implementation.
 *
 * Three sentinel kinds, three responsibilities each (detect, emit,
 * for two of them also reconstruct via the per-spec json_to_native
 * path). This file is the spec-independent machinery; per-spec
 * emission and reconstruction lives in each language's spec.c.
 *
 * Slice-1 scope (issue 318): $lang_opaque and $ref are end-to-end
 * for intra-language and in-process cases respectively;
 * $function_pointer is parsed and rejected pending the amendment's
 * wrapper-binary subsystem.
 */
#include "020-sentinels.h"
#include "json.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ sentinel_detect()
 *
 * Single-key match against the three reserved key names. We accept
 * any value type for the sentinel's payload — checking the inner
 * shape is the reconstructor's job, not the detector's. */
sentinel_kind_t sentinel_detect(const json_node_t *node)
{
    if (!node) return SENTINEL_NONE;
    json_node_t *n = (json_node_t *)node;
    if (json_kind(n) != JSON_OBJECT) return SENTINEL_NONE;
    if (json_object_size(n) != 1) return SENTINEL_NONE;
    const char *key = json_object_key(n, 0);
    if (!key) return SENTINEL_NONE;
    if (strcmp(key, "$ref") == 0)              return SENTINEL_REF;
    if (strcmp(key, "$lang_opaque") == 0)      return SENTINEL_LANG_OPAQUE;
    if (strcmp(key, "$function_pointer") == 0) return SENTINEL_FUNCTION_POINTER;
    return SENTINEL_NONE;
}
/* }}} */

/* {{{ Emission writers */
void sentinel_write_ref(json_writer_t *w, uintptr_t chunk_ptr, int len)
{
    /* The pointer is rendered as hex inside a JSON string so it
     * survives JSON-string-canonicalisation regardless of platform
     * pointer width. Decoders parse via strtoull. */
    char ptr_buf[32];
    snprintf(ptr_buf, sizeof ptr_buf, "0x%lx", (unsigned long)chunk_ptr);
    json_writer_object(w);
      json_writer_key(w, "$ref");
      json_writer_object(w);
        json_writer_key   (w, "chunk_ptr"); json_writer_string(w, ptr_buf);
        json_writer_key   (w, "len");       json_writer_int   (w, len);
      json_writer_end(w);
    json_writer_end(w);
}

void sentinel_write_lang_opaque(json_writer_t *w,
                                const char *lang, uint64_t tag,
                                const char *shape)
{
    json_writer_object(w);
      json_writer_key(w, "$lang_opaque");
      json_writer_object(w);
        json_writer_key (w, "lang"); json_writer_string(w, lang ? lang : "");
        json_writer_key (w, "tag");  json_writer_int   (w, (long long)tag);
        if (shape && *shape) {
            json_writer_key(w, "shape"); json_writer_string(w, shape);
        }
      json_writer_end(w);
    json_writer_end(w);
}

void sentinel_write_function_pointer_stub(json_writer_t *w,
                                          const char *signature)
{
    /* Per the amendment: the wrapper-binary subsystem hasn't landed,
     * so this writer emits a self-describing stub that the
     * consumer's json_to_native is expected to reject with a clear
     * "not yet implemented" message. Keeping the shape stable lets
     * future wrapper-binary work migrate fixture data without
     * rewriting the producer side. */
    json_writer_object(w);
      json_writer_key(w, "$function_pointer");
      json_writer_object(w);
        json_writer_key(w, "signature");
        json_writer_string(w, signature ? signature : "void(void)");
        json_writer_key(w, "status");
        json_writer_string(w, "wrapper-binary system not yet implemented");
      json_writer_end(w);
    json_writer_end(w);
}
/* }}} */

/* {{{ $ref bytes store
 *
 * Process-wide hash-less append-only table. We don't need fast
 * lookup; producers and consumers reference chunks by raw pointer,
 * which IS the lookup key. Keeping a table at all lets us
 * (a) own the lifetime explicitly and (b) bulk-free at run end
 * via sentinel_ref_store_clear so memory doesn't leak across
 * fresh invocations of the runner in long-lived tests.
 *
 * Concurrency: single mutex guards both append and clear. The
 * lookup path returns a stable pointer (the chunks themselves
 * never move) so reads outside the mutex are safe. */
typedef struct ref_entry {
    void  *bytes;
    int    len;
} ref_entry_t;

static ref_entry_t  *ref_table;
static int           ref_count;
static int           ref_capacity;
static pthread_mutex_t ref_mu = PTHREAD_MUTEX_INITIALIZER;

uintptr_t sentinel_ref_alloc(const void *bytes, int len)
{
    if (len < 0) return 0;
    void *copy = malloc((size_t)len);
    if (!copy && len > 0) return 0;
    if (len > 0) memcpy(copy, bytes, (size_t)len);

    pthread_mutex_lock(&ref_mu);
    if (ref_count == ref_capacity) {
        int new_cap = ref_capacity == 0 ? 16 : ref_capacity * 2;
        ref_entry_t *grown = realloc(ref_table,
                                     (size_t)new_cap * sizeof *ref_table);
        if (!grown) { pthread_mutex_unlock(&ref_mu); free(copy); return 0; }
        ref_table    = grown;
        ref_capacity = new_cap;
    }
    ref_table[ref_count].bytes = copy;
    ref_table[ref_count].len   = len;
    ref_count++;
    pthread_mutex_unlock(&ref_mu);
    return (uintptr_t)copy;
}

const void *sentinel_ref_lookup(uintptr_t chunk_ptr, int *out_len)
{
    pthread_mutex_lock(&ref_mu);
    for (int i = 0; i < ref_count; i++) {
        if ((uintptr_t)ref_table[i].bytes == chunk_ptr) {
            if (out_len) *out_len = ref_table[i].len;
            pthread_mutex_unlock(&ref_mu);
            return ref_table[i].bytes;
        }
    }
    pthread_mutex_unlock(&ref_mu);
    if (out_len) *out_len = 0;
    return NULL;
}

void sentinel_ref_store_clear(void)
{
    pthread_mutex_lock(&ref_mu);
    for (int i = 0; i < ref_count; i++) free(ref_table[i].bytes);
    free(ref_table);
    ref_table    = NULL;
    ref_count    = 0;
    ref_capacity = 0;
    pthread_mutex_unlock(&ref_mu);
}
/* }}} */

/* {{{ Capability validation */
unsigned int sentinel_validate(unsigned int producer_mask,
                               unsigned int consumer_mask)
{
    unsigned int gap = producer_mask & ~consumer_mask;
    if (gap == 0) return 0;
    /* Return the lowest-numbered offending bit so callers can name
     * one specific kind in their diagnostic. */
    for (unsigned int b = 1; b != 0; b <<= 1) {
        if (gap & b) return b;
    }
    return gap;   /* unreachable in practice */
}

const char *sentinel_kind_name(sentinel_kind_t k)
{
    switch (k) {
        case SENTINEL_NONE:             return "(none)";
        case SENTINEL_REF:              return "$ref";
        case SENTINEL_LANG_OPAQUE:      return "$lang_opaque";
        case SENTINEL_FUNCTION_POINTER: return "$function_pointer";
        default:                        return "(unknown)";
    }
}
/* }}} */
