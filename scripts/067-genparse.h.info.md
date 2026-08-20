# 067-genparse.h — what the generator found

The description a parse produces and an emission consumes.

- **`tkind_t`** — what a type fundamentally is: `TKIND_INT`,
  `TKIND_UINT`, `TKIND_FLOAT`, `TKIND_STRING` (a fixed char array
  inside a struct), `TKIND_STRUCT`, and `TKIND_STRING_PTR` — a
  borrowed `const char *`, legal as a parameter and never legal inside
  a struct, because a field table describes bytes the reader must be
  able to fill and a pointer is bytes pointing somewhere else.
- **`field_t`** — type, name, array length (zero when not an array),
  kind, and a link to the nested struct when it is one.
- **`sdef_t`** — a struct: name, where it came from, its fields.
- **`param_t`** / **`box_t`** — a box's name, origin, return type,
  parameters, and the kinds those resolve to.
- **`cmp_t`** — an author-written `type__compare`, which is never a box.
- **`description_t`** — the three vectors plus the arena everything in
  them lives in.

`gp_parse_file` appends what one source holds; `gp_validate` resolves
types, links nested structs, and refuses duplicates, run once after
every file; `gp_describe` prints the findings. `gp_fail` ends the
program naming file and line, and every refusal goes through it so
they all read the same way.
