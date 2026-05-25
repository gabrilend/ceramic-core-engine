# 422 — Box delete: the user-facing experience

## Status
open · phase 4 · sub-issue of [419](419-runtime-graph-mutation.md).
The underlying mechanism is part of [420](420-utility-box-kind.md);
this issue covers the user-facing API and the
explicit-vs-implicit-deletion design decision. Renumbered from
322 when runtime graph mutation moved to phase 4 as part of the
phase-3 release-candidate rollback.

## Current behavior

Once issue 320 lands, the runtime has a unified
`runtime_apply_box_spec(target_id, spec_json)` entry point that
handles create / reconfigure / delete based on the spec's
content. The delete path is: install NULL at the graph slot
via the same supersession mechanism, let the refcount drive
the old box's eventual free.

What 320 does NOT cover, and what this issue is for:

- The user-facing spec shape that triggers deletion.
- A convenience wrapper for the common case.
- Validation that catches "I forgot to fill in fields"
  accidents from masquerading as deletes.
- The downstream effects when consumers wired to a deleted box
  try to push values into the now-gone graph slot.

## Why deletion needs more than just "any minimum spec"

The natural shortcut would be: "if the spec only has an id and
no other fields, treat it as a delete." Compact and elegant.

But it's also dangerous: a user who forgets to populate fields
in their reconfigure spec accidentally deletes the box. Defaults
should fail-closed (do nothing surprising) rather than fail-open
(do something destructive). The minimum-spec-means-delete shape
fails open.

The safer alternative is an explicit marker in the spec:

```json
{"action": "delete", "id": "target_box_id"}
```

The runtime parses the spec, sees `action == "delete"`, and
routes through the delete path. Specs without an explicit
`action` field default to create-or-reconfigure (the inferred
action based on whether target_id exists). Explicit beats
implicit; destructive operations are always explicit.

## Convenience wrapper

For the common case where a user just wants to delete a box
they know the id of, add a sugar function:

```c
int runtime_destroy_box(const char *target_id, char **err_out);
```

Internally, this constructs the delete spec JSON and calls
`runtime_apply_box_spec`. Same code path, friendlier signature.

Same shape per language bridge:

- Lua: `soramech.destroy_box(target_id)`
- C:   `soramech_destroy_box(target_id, err_buf, err_cap)`
- Bash: `soramech_destroy_box "$target_id"`

## Downstream effects of deletion

A box X is deleted. Some other box Y has a connection pointing
at X. Y fires, produces an output, attempts to push to X's
input slots — but X is gone from the graph slot.

Two possible behaviours:

- **Silent drop:** the push fails because the target lookup
  returns NULL. Producer continues; output is lost. A push
  event with `result=target-gone` records the loss in the
  JSONL transcript.
- **Hard error:** the push fails and the producer's fire is
  treated as a failure. The run aborts (or the producer's
  spec returns an error code).

The user's stated preference throughout the project is "errors
over fallbacks." So hard error is the strict reading. But
deletion is a deliberate user action — if they deleted X, they
probably know Y depended on it. Hard-error-on-every-push would
mean a single deleted leaf-box could crash the entire run.

A middle path: silent drop by default, with the JSONL event
making the loss observable, AND a runtime knob (env var or
spec field) to upgrade the drop to a hard error for users who
want strict behaviour.

## Suggested implementation steps

1. **Define the spec shape for delete.** `{"action": "delete",
   "id": "..."}`. Document that the `action` field defaults to
   "apply" (which means create-or-reconfigure based on target
   existence) and the only other recognised value is "delete".
2. **Implement the delete path inside
   `runtime_apply_box_spec`.** Already part of 320's
   implementation; this is just adding the action dispatch.
3. **Add `runtime_destroy_box` as the sugar wrapper.** Three
   language bridges plus the C entry point.
4. **Add the push-target-gone JSONL event.** Same shape as
   other push events; `result=target-gone` is the new value.
5. **Add an opt-in strict mode** for users who want push-to-
   deleted-box to hard-error rather than silent-drop. Env var
   `SORAMECH_STRICT_DELETE=1` or per-call flag, TBD.
6. **Fixtures:**
   - `tests/maps/322-delete-then-orphaned-push` — A box X is
     created; B is wired to X; X is deleted; B fires once more;
     the push fails with `result=target-gone` in the JSONL.
   - `tests/maps/322-delete-and-recreate` — A box X is created,
     deleted, then created again with the same id and different
     behaviour. The recreate succeeds at the now-NULL graph
     slot; subsequent fires use the new behaviour.

## Relevant files

- `src/018-runtime-builtins.{c,h}` — `runtime_destroy_box`
  sugar wrapper.
- `src/010-graph-loader.{c,h}` — `parse_box_file` extended to
  recognise the `action` field on the spec.
- `src/013-jsonl-events.{c,h}` / `src/014-event-queue.{c,h}` —
  `target-gone` as a new push result.
- `langs/lua/spec.c`, `langs/c/spec.c`, `langs/bash/spec.c` —
  one new bridge function per language for `destroy_box`.

## Open questions

- **Strict-mode default.** Should `SORAMECH_STRICT_DELETE=0`
  (silent drop default) or `=1` (hard error default) be the
  shipped default? Silent drop matches "don't crash the run on
  a leaf-box deletion"; hard error matches "errors over
  fallbacks." Lean toward silent-drop-by-default + JSONL event
  + opt-in strict; revisit if a project disagrees.
- **Implicit recreation.** A delete followed by a create at
  the same id — should this work seamlessly, or require an
  explicit timing barrier? My instinct: works seamlessly. The
  graph slot transitions NULL → new-box-pointer atomically;
  any worker that arrives at the in-between state (graph slot
  is NULL) just exits gracefully.
