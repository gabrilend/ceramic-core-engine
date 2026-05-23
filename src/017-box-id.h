/* src/017-box-id.h — auto-generated box id strings, public API.
 *
 * What it does, in one sentence: hands out unique short string
 * identifiers of the form "auto_<8 hex chars>", suitable for use as
 * the `id` field of a runtime-created box.
 *
 * Designed in issue 319 (Q4 resolution); landed in issue 319c.
 *
 * Why a string and not an opaque handle: the rest of SoraMech
 * already identifies boxes by `id` string everywhere (box JSON
 * filenames, connection entries, dispatch log lines, the
 * `last-run.jsonl` event stream). Returning a string keeps
 * runtime-created boxes interchangeable with on-disk-defined ones.
 * No special "handle" type needs to cross language boundaries via
 * the JSON wire format.
 *
 * Uniqueness is process-wide: a single atomic counter feeds the
 * hex suffix, so no two calls within one process run return the
 * same string. Cross-run uniqueness is not a goal — these ids
 * exist only for the duration of the run that mints them.
 */
#ifndef SORAMECH_017_BOX_ID_H
#define SORAMECH_017_BOX_ID_H

#include <stddef.h>

/* Required buffer size for box_id_generate output: 5 prefix bytes
 * ("auto_") + 8 hex chars + 1 null terminator. Callers can
 * stack-allocate `char buf[BOX_ID_GEN_BUF_SIZE]` to be safe. */
#define BOX_ID_GEN_BUF_SIZE 14u

/* Fill `buf` with a freshly-minted box id of the form "auto_<hex>".
 * Returns 0 on success, -1 if buf_size is too small or buf is NULL.
 * Safe to call concurrently from many threads — uses an atomic
 * fetch-add for the counter. */
int box_id_generate(char *buf, size_t buf_size);

#endif /* SORAMECH_017_BOX_ID_H */
