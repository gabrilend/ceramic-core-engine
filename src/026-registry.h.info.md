# 026-registry.h — the registry, from outside

The joint between compiled C and text maps: for every box, its name,
its shim, and its full type story — all derived from the C that will
actually run, which is why maps never need to mention a type.

## Data structures

**box_param** — `{ type_name: text, size: int }`, one per parameter,
in declaration order.

**box_info** — one box:
| field | type | meaning |
|---|---|---|
| name | text | As it appears in a map. |
| shim | function pointer | The generated per-box call site. |
| n_params, params | int + array | The parameter story. |
| return_type, return_size | text + int | "void" and 0 for a sink. |
| task_size | `size_t` | Exact allocation for one invocation. |
| compare | function pointer or null | Three-way compare for the return type. |

**field_info / struct_info** — per-struct field tables: name, offset
(offsetof), size, kind (int/uint/float/string/struct), nested table
pointer, string length. What lets the statics reader turn brace text
into bytes without a parser per type.

## Functions

**registry_find(name) → box_info or null** — the lookup every map
name goes through.

**struct_find(type name) → struct_info or null**

**registry_print(stream)** — every box and struct, sizes and all,
for reading what was emitted.

**map_place_box(map, station, box name, kind)** — placement by name
with sizes drawn from the registry; comparators get their extra
threshold slot here, typed to the box's return. Aborts loudly on an
unknown name — the most common map mistake there is.
