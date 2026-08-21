# 503 — Three-way comparison on the delivery path

## Current behavior

Built. The compare function is resolved from the registry once, at
placement, and stored on the station, so the delivery path does one
call and a switch on the sign — minus one to the first port, zero to
the second, one to the third. The comparator itself carries no
comparison logic. A comparator whose return type has no ordering is
refused at placement, naming the type and the fix; as this issue
predicted, the check cannot live at build time because the generator
never sees the map — placement (and phase 6's load) is the earliest
moment, and the comment sits there as asked. Proven against the
byte-order lie twice: a negative double routes below a positive
threshold, and a vec3 with a large first field routes by its
author's magnitude ordering.

## Intended behavior

The delivery path calls the compare function for the box's return type,
generated or author-written in issue 305, and routes on its result:
`-1` to the first port, `0` to the second, `1` to the third.

**Three-way rather than less-than.** A less-than function would need a
second equality call to distinguish equal from greater. One call
returning a sign gives all three outcomes and maps straight onto the
three ports.

**The function is found through the registry**, keyed on the box's
return type. The comparator itself carries no comparison logic — it
looks up, calls, and switches on the sign.

**A comparator whose return type has no compare function fails the
build**, naming the type. Not at load, not at runtime: at build, when
the generator can see that a station is a comparator and that its box
returns something uncomparable.

## Why not compare the bytes

Comparing the underlying bytes of two values always produces an answer,
and the answer is wrong often enough to be dangerous.

Two floating-point numbers differing only in sign compare backwards,
because the sign bit is the most significant one — a negative number
reads as larger than a positive one. Negative integers have the same
problem when their bytes are read as unsigned. A struct with padding
between fields compares against whatever happens to be sitting in the
padding, which is not part of the value at all and may differ between
two structs that are equal in every field.

A comparator routing on a wrong answer produces a program that runs,
produces output, and is silently incorrect. That is the worst failure
this project can have, and refusing at build time is the whole point.

## Suggested implementation steps

1. Add the compare function pointer to the registry entry for each
   type that has one.
2. Resolve it at load and store it on the comparator station, so the
   delivery path does a call rather than a lookup.
3. Switch on the sign in the comparator row of issue 501's dispatch.
4. The build-time refusal, which needs the generator to know which
   stations are comparators — meaning it needs to see the map, which it
   does not. Until phase 6 this check lives at load; note in a comment
   that it should move earlier if the map ever becomes a build input.
5. Tests covering signed and unsigned integers, floating-point numbers
   including negatives and zero, and a struct ordered on a field other
   than its first — each asserting the routing matches what the
   comparison should say rather than what the bytes would say.

## Related

- [005 — Routing](../../docs/005-routing.md)
- Issue 305 — where compare functions come from
- Issue 502 — the comparator that calls this
