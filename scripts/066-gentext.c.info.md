# 066-gentext.c — the support machinery, from inside

Interface in `065-gentext.h.info.md`. The arena keeps a singly linked
list of blocks and bumps through the newest, starting a bigger one
whenever a request will not fit — so an allocation larger than a block
gets a block of its own rather than failing. Hand-outs are rounded up
to twice a pointer's width, because a misaligned struct pointer is
undefined behaviour that usually works, which is the worst kind.

Every allocation failure exits immediately with the resource code
rather than propagating, which is what keeps the callers readable.

`gt_normalize_type` is two passes and has to be: whitespace collapses
first, then stars are rewritten against the already-collapsed text.
The spellings it produces were checked against the retired Lua
generator's output one by one, including `char **` becoming
`char * *`, because those strings are compared for equality and a
difference would be two incompatible types with one name.
