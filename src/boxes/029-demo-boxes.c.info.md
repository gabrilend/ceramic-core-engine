# 029-demo-boxes.c — the boxes maps place, from outside

Ordinary functions; everything else is derived. What a map can name:

| box | takes | returns | notes |
|---|---|---|---|
| add | int, int | int | two of one type |
| mix | int, double | double | two different types |
| make_vec3 | float ×3 | vec3 | returns a struct |
| nudge | vec3, float | vec3 | struct both ways |
| magnitude_squared | vec3 | float | struct in, primitive out |
| stamp_record | int, vec3, unsigned long | record | every field kind |
| swallow | int | nothing | a sink |

Structs: **vec3** (three floats), **padded** (char, double, int —
deliberate padding hole), **record** (int, nested vec3, char[16]
string, unsigned long). **vec3__compare** orders by squared
magnitude — deliberately not the first field, so tests can tell the
semantic compare ran.
