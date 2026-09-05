# cera.h — the engine's interface

The whole of what a program built with this engine can call, in one
file. With `cera.c` beside it this is the engine; there is nothing else
to install and no include path to configure beyond the directory the two
files sit in.

## What replaced what

Seven numbered headers, in dependency order, each now a section:

| section | what it covers |
|---|---|
| 011 — the pool | worker threads, the task ring, sleeping and waking, termination |
| 018 — stations | the station table, ports, wires, and the construction surface |
| 026 — emitted | what the generator produces, and how a value becomes text and back |
| 040 — mapfile | reading a description, and placing a whole map inside another |
| 049 — observe | the reports, the observer thread, the dump, and rewiring |
| 073 — latebox | boxes and maps compiled while the program runs |
| 091 — stopping | signals, capture, and ending a program |

The numbers are kept because they are the reading order: the pool knows
nothing of stations, stations know nothing of the generator, and each
section stands on the ones above it.

## What is in here, and what is not

**100 symbols, every one of them beginning `cera_`, and the engine
exports exactly those 100.** Not "roughly
these" — checked on every test run by
`tests/112-test-public-surface.sh`, which compiles the engine alone,
asks the object file what it publishes, and fails naming anything that
is published without being declared here.

That is the property this file exists for: **private by default, public
only by a deliberate act**, the act being a line in this file. It
cannot decay, because breaking it means writing a function and
forgetting one word, and something checks.

**What is not here** is how the engine reaches itself — the slot state
machine, the pages a ring grows by, the constant a port holds, the text
the dump prints through, the output port lookup and its destination
sets, task construction, the pool's own callback, the scrapyard, the
box-table matcher. Twenty-three of them, in `cera.c` under a banner
saying so, with the documentation they always had.

## The one thing that is already known about the boundary

**Generated code is a separate translation unit and always will be.**
It is derived at build time from box sources this engine's author has
never seen, so it cannot be inside `cera.c`. Twenty-three functions are
therefore public by necessity rather than by choice — building a station
from a compiled map, marking doors, placing a map inside a map, a box
ending its own program, charging a box's time to its station, and the
eleven calls that turn a value into text and back.

A box or map compiled *while the program runs* binds to those same names
through the executable's dynamic symbol table, which is what
[098-engine-surface.syms](098-engine-surface.syms.info.md) publishes.
That file and this one describe the same boundary from two directions —
what a shared object may bind to, and what a program may call. They
agree today by both naming the same families; [905](../issues/completed/905-the-prefix.md) gave every public name one
prefix, so the linker's list is now a single pattern — and it stayed a
file, because what will not fit in a link command is the explanation
around the pattern rather than the pattern.

## Using it

```c
#include "cera.h"
```

One include path, or none if the two files sit beside your own source.

Two linker settings are required and are not optional:

```
-Wl,--dynamic-list=098-engine-surface.syms -Wl,--gc-sections
```

The first publishes the engine so that a box or a map compiled while
the program runs can bind back into it; the second throws away what
nothing reaches. They only make sense together, and the obvious way to
write the first — `-rdynamic` — cancels the second, because an exported
symbol is a root the collector may never touch and exporting everything
declares the whole binary reachable. See
[098-engine-surface.syms](098-engine-surface.syms.info.md), which
carries the measurements.

## Every call, and how it is declared

Generated from the header itself. One hundred calls in seven
sections, in the order the header presents them — the pool knows
nothing of stations, stations nothing of the generator, and each
*101 calls.* Anything not here is private, and `tests/112-test-public-surface.sh` fails if the engine publishes something this list does not name.

section stands on the ones above it.

### 011 — The pool

| call | declared as |
|---|---|
| `cera_pool_create` | `cera_pool_t *cera_pool_create(int n_workers, cera_pool_finish_t finish, void *finish_ctx)` |
| `cera_pool_push` | `void cera_pool_push(cera_pool_t *p, cera_task_t *t)` |
| `cera_pool_pop` | `cera_task_t *cera_pool_pop(cera_pool_t *p)` |
| `cera_pool_release` | `void cera_pool_release(cera_pool_t *p)` |
| `cera_pool_join` | `void cera_pool_join(cera_pool_t *p)` |
| `cera_pool_destroy` | `void cera_pool_destroy(cera_pool_t *p)` |
| `cera_pool_submitter_register` | `void cera_pool_submitter_register(cera_pool_t *p)` |
| `cera_pool_submitter_unregister` | `void cera_pool_submitter_unregister(cera_pool_t *p)` |
| `cera_pool_worker_index` | `int cera_pool_worker_index(void)` |
| `cera_pool_worker_count` | `int cera_pool_worker_count(cera_pool_t *p)` |
| `cera_pool_worker_epoch` | `uint64_t cera_pool_worker_epoch(cera_pool_t *p, int worker)` |
| `cera_pool_signal_when_finished` | `void cera_pool_signal_when_finished(cera_pool_t *p, int signo)` |
| `cera_pool_stop` | `void cera_pool_stop(cera_pool_t *p)` |
| `cera_pool_queued` | `int cera_pool_queued(cera_pool_t *p)` |
| `cera_pool_worker_station` | `int cera_pool_worker_station(cera_pool_t *p, int worker)` |
| `cera_pool_finished` | `int cera_pool_finished(cera_pool_t *p)` |
| `cera_pool_queue_stats` | `void cera_pool_queue_stats(cera_pool_t *p, int *capacity, int *high_water, int *growths)` |

### 018 — Stations, ports and wires

| call | declared as |
|---|---|
| `cera_map_station` | `static inline cera_station_t *cera_map_station(cera_map_t *m, int n)` |
| `cera_map_add_station` | `int cera_map_add_station(cera_map_t *m)` |
| `cera_map_create` | `cera_map_t *cera_map_create(int n_stations)` |
| `cera_map_create_empty` | `cera_map_t *cera_map_create_empty(void)` |
| `cera_map_place` | `void cera_map_place(cera_map_t *m, int station, cera_task_call_t shim, int kind, int n_in_ports, const int *elem_sizes, int out_size)` |
| `cera_map_in_port_start_depth` | `const char *cera_map_in_port_start_depth(cera_map_t *m, int station, int port, int slots)` |
| `cera_map_in_port_convert` | `void cera_map_in_port_convert(cera_map_t *m, int station, int port, int kind)` |
| `cera_map_configure_port` | `const char *cera_map_configure_port(cera_map_t *m, int station, int port, int source, const char *text)` |
| `cera_map_check_sources` | `const char *cera_map_check_sources(cera_map_t *m)` |
| `cera_map_bring_up` | `const char *cera_map_bring_up(cera_map_t *m)` |
| `cera_map_designate_output` | `const char *cera_map_designate_output(cera_map_t *m, int station)` |
| `cera_map_station_set_cursor` | `const char *cera_map_station_set_cursor(cera_map_t *m, int station, int at)` |
| `cera_map_designate_input` | `const char *cera_map_designate_input(cera_map_t *m, int station)` |
| `cera_map_start_beside` | `cera_map_t *cera_map_start_beside(cera_map_t *parent)` |
| `cera_map_deliver_argument` | `const char *cera_map_deliver_argument(cera_map_t *m, int station, int port, const void *value, int size)` |
| `cera_map_deliver_argument_text` | `const char *cera_map_deliver_argument_text(cera_map_t *m, int station, int port, const char *text)` |
| `cera_map_deliver_command_line` | `const char *cera_map_deliver_command_line(cera_map_t *m, int argc, char **argv)` |
| `cera_map_output_waiting` | `int cera_map_output_waiting(cera_map_t *m, int station)` |
| `cera_map_output_take` | `int cera_map_output_take(cera_map_t *m, int station, void *into, int size)` |
| `cera_map_name_station` | `const char *cera_map_name_station(cera_map_t *m, int station, const char *name)` |
| `cera_map_wire` | `const char *cera_map_wire(cera_map_t *m, int from_station, int port, int to_station, int to_port)` |
| `cera_map_remove_station` | `const char *cera_map_remove_station(cera_map_t *m, int station)` |
| `cera_map_connect` | `void cera_map_connect(cera_map_t *m, int from_station, int port, int to_station, int to_port)` |
| `cera_map_start` | `void cera_map_start(cera_map_t *m, int n_workers)` |
| `cera_map_destroy` | `void cera_map_destroy(cera_map_t *m)` |
| `cera_map_deliver_value` | `int cera_map_deliver_value(cera_map_t *m, int station, int port, const void *value)` |
| `cera_map_in_port_depth` | `int cera_map_in_port_depth(cera_map_t *m, int station, int port)` |
| `cera_map_station_try_start` | `int cera_map_station_try_start(cera_map_t *m, int station)` |
| `cera_map_station_start_while_ready` | `int cera_map_station_start_while_ready(cera_map_t *m, int station)` |
| `cera_map_station_keep_starting` | `int cera_map_station_keep_starting(cera_map_t *m, int station)` |
| `cera_map_station_start_after` | `int cera_map_station_start_after(cera_map_t *m, int station, void (*while_locked)(void *), void *ctx)` |
| `cera_map_in_port_static_text` | `void cera_map_in_port_static_text(cera_map_t *m, int station, int port, const char *text)` |
| `cera_map_in_port_static_write` | `void cera_map_in_port_static_write(cera_map_t *m, int station, int port, const void *bytes, int size)` |
| `cera_map_in_port_queue_text` | `const char *cera_map_in_port_queue_text(cera_map_t *m, int station, int port, const char *text)` |

### 026 — Generated code, and values as text

| call | declared as |
|---|---|
| `cera_box_place_find` | `const cera_box_place_t *cera_box_place_find(const char *name)` |
| `cera_text_expect` | `const char *cera_text_expect(const char *p, char c, const cera_where_t *w, const char *what)` |
| `cera_text_signed` | `const char *cera_text_signed(const char *p, void *out, int size, const cera_where_t *w, const char *field)` |
| `cera_text_unsigned` | `const char *cera_text_unsigned(const char *p, void *out, int size, const cera_where_t *w, const char *field)` |
| `cera_text_floating` | `const char *cera_text_floating(const char *p, void *out, int size, const cera_where_t *w, const char *field)` |
| `cera_text_chars` | `const char *cera_text_chars(const char *p, char *out, int room, const cera_where_t *w, const char *field)` |
| `cera_text_put` | `void cera_text_put(cera_textbuf_t *tb, const char *literal)` |
| `cera_text_put_signed` | `void cera_text_put_signed(cera_textbuf_t *tb, const void *bytes, int size)` |
| `cera_text_put_unsigned` | `void cera_text_put_unsigned(cera_textbuf_t *tb, const void *bytes, int size)` |
| `cera_text_put_floating` | `void cera_text_put_floating(cera_textbuf_t *tb, const void *bytes, int size)` |
| `cera_text_put_chars` | `void cera_text_put_chars(cera_textbuf_t *tb, const char *chars, int room)` |
| `cera_struct_text_find` | `const cera_struct_text_t *cera_struct_text_find(const char *type_name)` |
| `cera_struct_find` | `const cera_struct_info_t *cera_struct_find(const char *type_name)` |
| `cera_emitted_print` | `void cera_emitted_print(FILE *out)` |
| `cera_map_place_box` | `void cera_map_place_box(cera_map_t *m, int station, const char *box_name, int kind)` |
| `cera_box_source_text` | `const char *cera_box_source_text(const char *path)` |
| `cera_map_build_find` | `const cera_map_build_t *cera_map_build_find(const char *path)` |

### 040 — Descriptions, and a map inside a map

| call | declared as |
|---|---|
| `cera_map_load_file` | `cera_map_t *cera_map_load_file(const char *path, int n_workers)` |
| `cera_map_load_salvage` | `cera_map_t *cera_map_load_salvage(const char *path, int n_workers)` |
| `cera_map_seed_count` | `int cera_map_seed_count(cera_map_t *m)` |
| `cera_map_instantiate_file` | `cera_map_instance_t cera_map_instantiate_file(cera_map_t *m, const char *path)` |
| `cera_map_instance_entrance` | `int cera_map_instance_entrance(cera_map_t *m, const cera_map_instance_t *in, int nth)` |
| `cera_map_instance_result` | `int cera_map_instance_result(cera_map_t *m, const cera_map_instance_t *in, int nth)` |
| `cera_map_instance_free` | `void cera_map_instance_free(cera_map_instance_t *in)` |
| `cera_map_add_part` | `const char *cera_map_add_part(cera_map_t *m, const char *what, cera_map_part_t *out)` |
| `cera_map_connect_parts` | `const char *cera_map_connect_parts(cera_map_t *m, cera_map_part_t from, int from_port, cera_map_part_t to, int to_port)` |

### 049 — Watching, dumping, rewiring

| call | declared as |
|---|---|
| `cera_map_report_buffers` | `void cera_map_report_buffers(cera_map_t *m, FILE *out)` |
| `cera_map_report_stations` | `void cera_map_report_stations(cera_map_t *m, FILE *out, int order)` |
| `cera_map_observe_start` | `void cera_map_observe_start(cera_map_t *m, const char *path, int interval_ms)` |
| `cera_map_observe_stop` | `void cera_map_observe_stop(cera_map_t *m)` |
| `cera_map_report_shutdown` | `void cera_map_report_shutdown(cera_map_t *m)` |
| `cera_stats_box_time` | `void cera_stats_box_time(cera_task_t *t, long ns)` |
| `cera_map_dump` | `void cera_map_dump(cera_map_t *m, FILE *out)` |
| `cera_map_unwire` | `const char *cera_map_unwire(cera_map_t *m, int from_station, int port, int to_station, int to_port)` |
| `cera_map_disconnect` | `void cera_map_disconnect(cera_map_t *m, int from_station, int port, int to_station, int to_port)` |

### 073 — Code that arrives after the build

| call | declared as |
|---|---|
| `cera_late_compile_source` | `int cera_late_compile_source(const char *c_source)` |
| `cera_late_box_count` | `int cera_late_box_count(void)` |
| `cera_late_box_at` | `const cera_box_place_t *cera_late_box_at(int i)` |
| `cera_late_unload_box` | `int cera_late_unload_box(cera_map_t *m, const char *name)` |
| `cera_late_source_dir` | `const char *cera_late_source_dir(void)` |
| `cera_late_compile_map` | `const cera_map_build_t *cera_late_compile_map(const char *map_text)` |
| `cera_late_spill_sources` | `int cera_late_spill_sources(const char *dir)` |
| `cera_late_source_text` | `const char *cera_late_source_text(const char *path)` |

### 091 — Ending a program, and putting one down

| call | declared as |
|---|---|
| `cera_prepare` | `void cera_prepare(const char *report_path)` |
| `cera_report_path` | `const char *cera_report_path(void)` |
| `cera_wait` | `int cera_wait(cera_map_t *m)` |
| `cera_capture` | `int cera_capture(cera_map_t *m, const char *path)` |
| `cera_capture_now` | `int cera_capture_now(cera_map_t *m, const char *path)` |
| `cera_capture_whole` | `int cera_capture_whole(cera_map_t *m, const char *dir)` |
| `cera_stop_now` | `void cera_stop_now(cera_map_t *m, int exit_code, const char *why)` |


## Related

- [cera.c](cera.c.info.md) — the implementation
- [901](../issues/completed/901-the-engine-becomes-one-file.md) — why there is one file
- [902](../issues/completed/902-the-header-says-what-is-public.md) — what is in this file, and why
- [903](../issues/completed/903-everything-else-goes-private.md) — what is not, and what checks
- [057 — Packaging](../docs/implementation-notes/057-packaging.md)
