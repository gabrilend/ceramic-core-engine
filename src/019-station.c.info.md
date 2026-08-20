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
- Ports are appended at the tail of their list so indices match the
  order wires were declared. **Destinations are not a list at all**
  (issue 214): a port holds one immutable array behind an atomic
  pointer, and drawing or removing a wire builds a whole new array and
  swaps the pointer. `port_dests` reads it with one atomic load and no
  lock; `dest_set_build` makes the replacement; `dest_set_retire`
  files what it replaced. New wires are appended within the array, so
  fan-out order still matches declaration order and the dump still
  round-trips.
- **The scrapyard** holds sets a rewire replaced, because a walker may
  be inside one. `map_scrap_sweep` frees every set no worker can still
  be inside — a worker whose epoch is even is not in a task, and one
  whose epoch has moved has left the task it was in. Retiring sweeps
  first, so a program that rewires forever reclaims as it goes.
  `map_scrap_free_all` empties the rest at teardown. Both unfile a set
  and free it under one hold of the scrap lock, so a second toucher
  does not find it and cannot free it twice; the lock is a leaf and
  nothing is acquired while it is held.
- Teardown walks stations, then slots, then ports, freeing leaf-first,
  and then empties the scrapyard.
