# 019-station.c — the structural half, from inside

The usable interface is in `018-station.h.info.md`. This file builds
and dismantles; it never moves a value. Its private pieces:

- A construction failure routine that prints what was wrong with the
  map being described and stops — building on a wrong description
  helps nobody.
- Ring buffers start at four cells on purpose: growth is cheap and
  proven, and a generous initial size would only hide the mechanism
  the demos exist to show.
- Ports and destinations are appended at the tails of their lists so
  indices and fan-out order match the order wires were declared —
  the loader's round-trip tests depend on that stability.
- Teardown walks stations, then slots, then ports, then
  destinations, freeing leaf-first.
