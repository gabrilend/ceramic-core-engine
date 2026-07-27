# 042-loader.c — the loader, from inside

Interface in `040-mapfile.h.info.md`. The shape:

1. **First pass** — every station created from the registry (the
   misspelled-box message is deliberately the best in the program),
   slots defaulting to ring buffers, the comparator's threshold
   appended and typed, statics bound. The name table lives here and
   dies when loading ends.
2. **Second pass** — arrows resolved by name (forward references are
   why passes exist), every wire type-checked by type *name* at the
   first moment both ends are known, gather wires bound through the
   cycle check.
3. **Whole-map validation** — collected, then one stop: arrows onto
   non-buffer slots, stations both pushed and pulled, gathered
   stations with ring inputs; plus the loud-but-not-fatal warning
   for buffered stations nothing feeds.
4. **Seed** — the one scan the engine ever makes: stations with no
   ring slots and nobody pulling them are enqueued through the same
   task construction delivery uses, announced by name, counted. Zero
   seeded is fatal: the map could never start.

The pool exists before the seed (somewhere to push) with workers
parked (the termination rule's outside-pusher clause stays true).
