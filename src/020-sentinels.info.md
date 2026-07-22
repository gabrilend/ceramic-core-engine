# 020-sentinels.c/.h — public surface

Cross-language JSON sentinel primitives (issue 318). Three reserved
single-key object shapes carry values plain JSON can't represent
structurally:

- `{"$ref": {"chunk_ptr": "0x...", "len": N}}` — a handle to raw
  bytes held in a process-wide store.
- `{"$lang_opaque": {"lang": ..., "tag": N, "shape": ...}}` — a
  language-private value that only the same language reconstructs.
- `{"$function_pointer": {...}}` — reserved; see slice-1 scope.

A producing language spec emits the right sentinel when its encode
walker hits an uncarriable value; the consuming spec detects the
shape in the parsed tree and rebuilds the value with its own
machinery. This file is the spec-independent core: detection,
canonical emission, the shared bytes store, and the compile-time
capability check. Per-language emission/reconstruction lives in
each spec.

## Detection

- `sentinel_kind_t sentinel_detect(const json_node_t *node)` —
  returns the kind for an object with exactly one top-level key
  matching a sentinel name; `SENTINEL_NONE` for anything else
  (arrays, scalars, multi-key objects). Payload shape is the
  reconstructor's job, not the detector's.

## Emission writers

Each writes the canonical bytes for its kind through the bounded
JSON writer, leaving the cursor where surrounding context expects.

- `sentinel_write_ref(w, chunk_ptr, len)` — the pointer is rendered
  as a hex string so it survives JSON canonicalisation at any
  platform pointer width.
- `sentinel_write_lang_opaque(w, lang, tag, shape)` — `shape` is
  optional and omitted when empty.
- `sentinel_write_function_pointer_stub(w, signature)` — emits a
  self-describing stub the consumer will refuse with a clear
  "wrapper-binary system not yet implemented" message; the stable
  shape lets future fixture data migrate without touching producers.

## $ref bytes store (process-wide, refcounted)

- `uintptr_t sentinel_ref_alloc(bytes, len)` — copies the bytes
  into the store, refcount 1, returns the chunk pointer to embed
  in the sentinel. 0 on failure.
- `const void *sentinel_ref_lookup(chunk_ptr, &out_len)` —
  read-only; returns the byte pointer and length without touching
  the refcount. NULL/0 if unknown.
- `sentinel_ref_inc` / `sentinel_ref_dec` — bump/drop; at zero the
  bytes free and the table slot becomes reusable, so long-running
  processes stay bounded provided owners drop what they're done
  with.
- `sentinel_ref_store_clear()` — bulk teardown at end of run.

The refcount machinery is opt-in: callers that never inc/dec get
leak-per-run behaviour with one clear at the end, exactly as
before. A single mutex guards table mutation; increments and the
returned byte pointers are safe outside it (the bytes never move).

## Capability validation

- `unsigned int sentinel_validate(producer_mask, consumer_mask)` —
  0 if every kind the producer may emit is reconstructable by the
  consumer; otherwise the lowest offending capability bit, so the
  compile-time wire walker can name one specific kind in its
  warning. Masks are built from `SENTINEL_BIT`/`SENTINEL_MASK_*`.
- `const char *sentinel_kind_name(k)` — human-readable kind name
  for diagnostics.

## Slice-1 scope (what's NOT here)

- `$lang_opaque` and `$ref` are end-to-end for intra-language and
  in-process cases respectively.
- `$function_pointer` is parsed and **rejected** pending the
  amendment's wrapper-binary subsystem; only the stub writer above
  exists on the emit side.

## Related

- Issue 318 — design and amendment.
- `json.h` — the parser nodes and bounded writer this speaks.
