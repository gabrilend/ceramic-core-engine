# 305 — Compare functions

## Current behavior

Nothing compares anything. Comparators arrive in phase 5 and will need
this to exist.

## Intended behavior

For every type that can be a box's return value, a function that
compares two of them and returns `-1`, `0`, or `1`.

**Three-way, not less-than.** A comparator routes three ways — less,
equal, greater — so a less-than function would need a second
equality call to distinguish the last two. One function returning a
sign gives all three outcomes in one call and maps straight onto the
three ports.

**The primitives get theirs generated.** Nobody should write a compare
function for `int`.

**A struct supplies its own**, found by the `__compare` suffix on its
type name. This is deliberately the same trick as the shims: a naming
convention the generator recognizes, so the author writes ordinary C
and the wiring is discovered rather than registered.

**A type with no compare function is a build failure**, but only when a
comparator actually uses it. Most types are never compared and should
not have to explain themselves.

## Why raw bytes are not an option

Comparing the underlying bytes of two values always produces an answer,
and the answer is wrong often enough to be dangerous. Two floats
differing only in sign compare backwards, because the sign bit is the
most significant one — so a negative number reads as larger than a
positive one. Negative integers on a two's-complement machine have the
same problem when compared as unsigned bytes. A struct with padding
between its fields compares against whatever happens to be sitting in
the padding.

A comparator routing on a wrong answer produces a program that runs,
produces output, and is silently incorrect — the worst failure this
project can have. Refusing at build time is the whole point.

## Suggested implementation steps

1. Emit compare functions for every primitive type used as a box return
   value.
2. Recognize author-written compare functions by suffix in issue 301's
   parse and associate each with its type.
3. Add compare-function availability to the registry, so the check in
   step 4 is a lookup.
4. When phase 5 lands, a comparator whose box returns a type with no
   compare function fails the build and names the type.
5. Tests covering signed and unsigned integers, floating-point numbers
   including negatives and zero, and a struct with a hand-written
   compare that orders on a field other than the first.

## Related

- [005 — Routing](../docs/005-routing.md)
- [007 — The build path](../docs/007-datapath-build.md)
- Issue 503 — the comparator that calls these
