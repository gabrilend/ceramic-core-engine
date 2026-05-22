# Strategem — a contract must fit every implementer, not just the simplest

## The pattern

When designing a contract that several implementers will satisfy
(spec interfaces, abstract types, system protocols), check whether
the contract's assumptions hold for *every* class of implementer,
not just the cleanest case in front of you. A contract that works
for one class and almost-works for another is a contract that
forces the awkward class into shims, hacks, or special-cases. The
right move is to weaken the contract until it fits everyone
honestly.

The trap is subtle: the cleanest implementer often comes to mind
first when you're designing. C and Bash specs in this project both
treat values as bytes — their bridges write to and read from `dst`
buffers. The proposed amendment to issue 312 designed the
dispatch-side normalisation around this: "dispatch calls the
consumer spec's `json_to_native` with a buffer, gets native bytes
back, hands them to invoke." Clean, symmetric, byte-oriented.

The amendment failed when Lua showed up. Lua's `json_to_native`
doesn't write to a buffer — it pushes a value onto the worker's
`lua_State` stack. There is no "Lua native byte form" external
to the state for dispatch to hand to invoke. And Lua isn't an
outlier: every runtime that owns its values (Python, Ruby,
JavaScript, anything with a private GC heap) would have the same
shape. The amendment assumed buffer orientation; half of all
possible future specs don't have buffer orientation; the contract
was wrong.

## How to apply

When sketching a new contract:

- List the implementers you can imagine. Not just the ones you
  have today — the ones a future user might bring.
- For each, ask: does the contract's central abstraction (bytes,
  callbacks, state ownership, etc.) actually fit how that
  implementer naturally works?
- If even one implementer needs a shim — "we'll just say its
  buffer is empty," "it'll ignore that half of the signature,"
  "we'll define a convention where..." — the contract is doing
  half the work and the implementer is doing the other half.
  That asymmetry is a smell.
- Either weaken the contract so the awkward implementer fits
  honestly, or accept that the contract is for a narrower
  category (e.g. "buffer-oriented specs only") and design a
  separate path for the others.

The litmus test: can the most awkward implementer satisfy the
contract *without lying*? If lua_json_to_native has to set
`*dst_size = 0` and leave dst untouched because Lua doesn't have
bytes to write, the signature is forcing it to satisfy a shape
that doesn't apply. That's the lie. The dispatch can't actually
use the (empty, zero-sized) result. The contract has dead
parameters for half its implementers.

## Adjacent pattern — the "agnosticism" question

When a user asks "is this design agnostic to X?", they're usually
asking exactly this question. The answer to "is the system
agnostic to runtime statefulness?" is: yes, if and only if the
contract doesn't assume statelessness. Buffer-oriented bridges
assume statelessness. Stack-protocol bridges accommodate
statefulness. Neither is wrong; what's wrong is mixing them and
hoping the awkward case will quietly degrade.

## Why this happens

When the contract is being designed, the people in the room are
usually thinking about the implementers in front of them. Future
implementers don't get to participate. So the contract drifts
toward what works for the present implementers, and the future
ones inherit the asymmetry. The discipline is to imagine those
future implementers explicitly — to write down their shape and
ask if the contract fits, before shipping.

This is especially load-bearing for plugin / extension surfaces
(spec interfaces, hook points, anything with a `register_X` API).
The whole point of those surfaces is that new implementers will
appear. If the contract was built for the first two, the third
will not fit.
