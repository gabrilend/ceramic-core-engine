# 027-emitted-support.c — the hand-written half of what the generator emits

Interface in `026-emitted.h.info.md`. This file walks the generated
arrays and never changes when a box does — that division is the whole
point. Lookups are linear walks: dozens of boxes, load-time only, so
simplicity beats any table cleverness. Placement-by-name is where the
comparator's extra threshold port is appended, typed from the box's
return value, and where a sink asked to be a comparator is refused
(nothing to compare).
