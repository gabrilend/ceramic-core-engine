# 027-registry-support.c — the registry's hand-written half

Interface in `026-registry.h.info.md`. This file walks the generated
arrays and never changes when a box does — that division is the whole
point. Lookups are linear walks: dozens of boxes, load-time only, so
simplicity beats any table cleverness. Placement-by-name is where the
comparator's extra threshold slot is appended, typed from the box's
return value, and where a sink asked to be a comparator is refused
(nothing to compare).
