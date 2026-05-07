# 108 — Branch box and predicate routing

## Status

open

## Blockers

- 104 (executor must exist; predicate evaluation is added to it here)
- 107 (wire UI must exist; branch port wires need visual distinction)

## Current behavior

No branch box type exists. All routing is linear (one output, one wire).
There is no mechanism to conditionally fire one box vs. another based on
a value.

## Intended behavior

A box with "kind": "branch" has named output ports instead of positional
outputs. Each port (except "else") carries a predicate. The executor
evaluates each predicate against the input value and fires the first port
whose predicate is true. If no predicate matches, the "else" port fires.

Port predicates:
  { "op": "eq"|"lt"|"gt"|"lte"|"gte"|"contains"|"matches", "value": <literal> }

"matches" uses lua pattern matching against a string input.
"contains" checks whether a string input contains the literal as a substring.

Branch box behavior in two scenarios:

  Numeric/string predicate branch:
    - "else" port must be wired to something, or the runner refuses to start
    - If no predicate matches and "else" fires, the program continues from
      the "else" port's connection
    - The runner logs which port fired for each branch box in last-run.json

  LLM output classification:
    - "else" port may be left unwired in the box file (omit the connection)
    - If "else" fires on an unwired port, the runner re-queues the box
      whose output fed this branch box (the immediately upstream box in the
      path that led here)
    - The upstream box is re-invoked with any fields marked "retry_vary"
      in its box file pseudo-randomized (e.g., temperature, top_p)
    - Retry limit: configurable per branch box, default 3. Exceeded retry
      limit is a hard error, not a silent loop.

The "retry_vary" field on a box:
  "retry_vary": { "temperature": { "min": 0.5, "max": 1.2 } }
  On retry, any field listed here is replaced with a random value in range.

## Suggested implementation steps

1. Add branch box handling to src/004-executor.lua:
   - When a branch box fires, evaluate each port's predicate in order.
   - Fire the first matching port's connection.
   - If no match, fire "else". If "else" has no connection, trigger retry.
2. Add retry state to the executor: a per-box retry counter in run state.
   On retry trigger, decrement counter; if zero, halt with error.
3. Implement retry_vary: before re-invoking the upstream box, for each
   field in its retry_vary table, generate a random value in the given
   range and override the corresponding input before invocation.
4. Update src/003-loader.lua validation: branch boxes without an "else"
   connection are valid only if at least one upstream box has a retry_vary
   field. If neither condition is met, the validator rejects the map.
5. Update assets/js/006-wires.js: branch box ports each get a distinct
   color from a cycling palette. Port labels ("rogue", "wizard", "else")
   are drawn alongside the port dot on the canvas.
6. Update assets/js/004-inspector.js: branch box inspector shows port list
   with add/remove, predicate editor per port (op dropdown + value input),
   and retry limit input.

## Related documents

- docs/001-architecture.md — branch box file format
- issues/104 — executor (predicate evaluation added here)
- issues/107 — wire UI (port color and label rendering)

## Notes

Port predicate evaluation order matters: ports are evaluated in the order
they appear in the "ports" array. The first matching predicate fires. This
is intentional — the user controls priority by ordering ports. Document
this in the inspector UI (e.g., "ports are evaluated top to bottom").

The retry mechanism is deliberately minimal: it re-runs one upstream box
with varied parameters. It is not a general loop construct. If the user
needs complex retry logic (backoff, multiple retries with different
strategies), they should model it with explicit boxes wired in a cycle
through a branch box. The built-in retry is for the common case of
"ask the LLM again with a different temperature."
