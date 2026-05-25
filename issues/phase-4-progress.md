# Phase 4 Progress — Runtime graph mutation

## Goal

A running map can reconfigure or delete its own boxes, and
create new ones, through one unified utility-box kind. The
mechanism is a gather-flag + refcount + atomic-pointer swap at
the per-id graph slot. The user-facing surface is declarative —
boxes wired together — with no language-bridge functions that
import the runtime as a library.

## Prior art (rolled-back phase 3 attempt)

Phase 3 shipped an earlier shape of this feature under what
was originally the 319 family. Those issues live under
425 / 426 / 427 / 428 / 429 now — renumbered into phase 4
during the consolidation. Each carries a "Rolled back" header
explaining why the prior shape was reverted; their
historical-behavior sections describe what the prior code did,
preserved as reference.

The two pieces of infrastructure that stayed in the runner
(319a's input-cap removal and 319b's slot-store growth) are
genuinely useful regardless of runtime mutation and remain in
`completed/` as shipped phase-3 work.

## In-scope issues

### Active design

| ID  | Title                                                  | Status   |
|-----|--------------------------------------------------------|----------|
| [419](419-runtime-graph-mutation.md) | runtime graph mutation (parent) | open · ground rules settled, eight open questions |
| [420](420-utility-box-kind.md)       | utility-box kind                | open · sub of 419 · design substantially settled |
| [421](421-unify-connections-mutation-through-box-swap.md) | unify connections-array mutation through box swap | open · sub of 419 · waits on 420 |
| [422](422-box-delete-via-runtime-spec-api.md) | box delete: user-facing experience | open · sub of 419 · two open questions |
| [423](423-ring-reconfigure-slot-tag.md) | RING_RECONFIGURE slot-store tag | open · sub of 419 · waits on 420 mutator flow |
| [424](424-runtime-mutation-jsonl-events.md) | runtime-mutation JSONL events | open · sub of 419 · mechanical once 420 lands |

### Rolled-back prior attempts (historical reference)

| ID  | Title                                                  | Status   |
|-----|--------------------------------------------------------|----------|
| [425](425-builtin-library-for-map-self-construction.md) | built-in library for map self-construction (was 319) | rolled back · parent of the prior attempt |
| [426](426-box-id-and-compile-cache.md) | box id generator and compile cache (was 319c) | rolled back · only the id generator ever shipped |
| [427](427-create-box-and-connect-builtins.md) | create_box and connect runtime built-ins, Lua bindings (was 319d) | rolled back · language-bridge surface reverted |
| [428](428-c-and-bash-bindings-and-end-to-end.md) | C bindings + end-to-end test (was 319e) | rolled back · C bindings reverted |
| [429](429-create-box-and-connect-as-box-kinds.md) | create_box and connect as language-agnostic box kinds (was 319f) | rolled back · dispatch-kind surface reverted |

## Phase goal checklist

- [ ] Utility-box kind dispatches correctly for delete (empty
      spec) and install (populated spec)
- [ ] Gather flag prevents concurrent gather-or-mutator entries
      to the same box; CAS losers yield-and-requeue
- [ ] Refcount-based reclamation frees superseded boxes after
      all in-flight tasks finish, with the mutator's +1 hold
      preventing premature free
- [ ] RING_RECONFIGURE tag rides existing input slots; no
      separate control queue
- [ ] Connections-array mutation collapses to the box-swap path;
      the dispatch reads connections as plain fields with no
      atomic loads
- [ ] Multi-target operations work: one utility-box with N
      outgoing wires applies the same spec to N target slots
- [ ] Box delete works end-to-end; the
      push-to-deleted-target policy is decided and documented
- [ ] Phase-4 demo: a self-healing map demonstrates install
      and delete through one utility-box
- [ ] All phase-3 integration fixtures still pass under the new
      mechanism (regression-safe)

## Design history pointer

The four planning documents at `/tmp/soramech-320-321-322-{plan,mid,dense,voice}.md`
captured the design conversation that produced this phase's
ground rules. Those are scratch documents — promote to
`docs/datapath-graph-mutation.md` once the open questions in 419
resolve and the shape is locked in.
