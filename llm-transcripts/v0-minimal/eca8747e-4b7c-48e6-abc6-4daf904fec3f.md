# Code-only transcript

```
bb404f5 314 — the project's own small C JSON parser and writer
707f5ab 310 — priority parameter wired through the spawn surface
c249ea8 309 — top-level build and the portable-artifact pipeline
43f33c6 308 — bash boxes through a persistent socket server, no per-call spawn
ab1d2b8 303 — pluggable language-runtime spec, equal footing for every language
d1bff73 301 — thread-pool lifecycle and per-worker initialization
1b14cf8 231 — language-specific signature parsers move into langs/

```

```
allocated chunk layout
offset  size  field
------  ----  -----
0       4     refcount
4       4     length
8       …     data

```

```
producer task:
   compute output
   for each downstream consumer box C this producer just touched:
      write all values to C's input slots (snapshotted into the attempt's cell at the END)
   for each distinct C:
      slab_alloc() → attempt_task for C
      pool_spawn(attempt_task)
   slab_free(this cell)

attempt task:
   if every required input on C is satisfiable (slot value or data-box pull):
      snapshot values into local vars
      invoke spec, get output
      push output to downstream consumer slots
      submit one attempt per distinct downstream consumer
      decrement input $ref counts
      decrement upstream boxes' live_predecessor_count if applicable
   slab_free(this cell)

```
