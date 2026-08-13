# 019-station.c — the structural half, from inside

The usable interface is in `018-station.h.info.md`. This file builds
and dismantles; it never moves a value. Its private pieces:

- A construction failure routine that prints what was wrong with the
  map being described and stops — building on a wrong description
  helps nobody.
- Ring buffers start at ten cells on purpose: growth is cheap and
  proven, and a generous initial size would only hide the mechanism
  the demos exist to show. Ten is a magic number and is meant to be
  one — it lives as a single named constant beside the record in the
  header, because a port can now ask for a different depth and two
  places have to agree on what the default is.
- Every port gets its cells at placement whatever it is for, and
  nothing frees them until the map is torn down. A conversion is then
  a field write, and values already waiting survive one.
- One table names the three kinds in the words a person would use, so
  every refusal that turns somebody away from a port can say which of
  the three it found. "Not a buffer" describes two different
  situations with two different fixes.
- Ports and destinations are appended at the tails of their lists so
  indices and fan-out order match the order wires were declared —
  the loader's round-trip tests depend on that stability.
- Teardown walks stations, then slots, then ports, then
  destinations, freeing leaf-first.
