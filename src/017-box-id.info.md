# 017-box-id

Generates short unique string identifiers used as the `id` field of
runtime-created boxes (the kind that `create_box` from issue 319d
will mint when the caller doesn't supply an explicit id).

## Functions

### `box_id_generate(char *buf, size_t buf_size) -> int`

Fills `buf` with a string of the form `auto_<8 hex chars>`.

- **Returns** `0` on success, `-1` if `buf` is NULL or `buf_size` is
  less than `BOX_ID_GEN_BUF_SIZE`.
- **Thread safety**: safe to call from any thread concurrently.
- **Output length**: always 13 characters plus the null terminator.

### `BOX_ID_GEN_BUF_SIZE` (macro)

The minimum buffer size for `box_id_generate` output (14 bytes).
Callers can stack-allocate `char buf[BOX_ID_GEN_BUF_SIZE]` to be
sure the call will succeed.

## Uniqueness contract

A process-wide atomic counter feeds the hex suffix, so no two calls
within a single process run return the same string. Cross-run
uniqueness is not provided — these ids exist only for the duration
of the run that mints them.

The counter is a `uint32_t`, so the practical ceiling is ~4 billion
ids per run. Beyond that the counter wraps; in practice no
SoraMech workload approaches this.
