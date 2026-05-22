# Strategem — when a redesign changes the nature, change the surface

## The pattern

When a redesign changes the *nature* of a thing (a task becomes
not-a-task, a coupling becomes independence, a push becomes a pull,
a synchronous call becomes async), the user-visible reports and the
tests must also change shape to reflect that. Carrying the old
surface forward looks like back-compat, but it's actually a quiet
refusal to commit to the new design.

The classic tell: a redesign ships, an existing test fails, and the
instinct is to add a small branch in the new code that makes the old
test pass. That branch is the lie. It says "the new model is the new
model, except in the report, where it pretends to be the old model so
your tests stay green."

Where this kept showing up: under 244, read boxes stopped running as
tasks. The pool runner's `outputs:` report used to list every box's
captured output, with read boxes producing their value. The first
244 commit kept that report shape — added a branch that, when the
box was a read box with no captured output, surfaced its cached value
instead. Three integration tests with substring assertions like
`"who → World"` kept passing because the report kept pretending the
read box "ran."

The fix was not the branch. The fix was: read boxes don't run, so
they don't appear in the outputs report; the tests update to assert
the consumer's output (which is what they were really proxying for
— "did the value reach its destination") instead of the read box's
pseudo-output. The report now reflects reality and the tests now
assert what users care about.

## How to apply

When a redesign lands and an existing test breaks:

- First, look at what the test was asserting. Was it checking
  user-visible behavior, or was it checking an implementation surface
  that no longer exists?
- If the assertion was a proxy for something deeper ("did the value
  flow through?"), find the place in the new model where that
  something is observable, and assert there.
- If the assertion was just naming an internal mechanism that has
  gone away, delete the assertion — the new model is the contract
  now.
- Don't add a branch in production code whose only purpose is to
  keep an old test green. That's the smell.

If you find yourself writing a comment like "preserves observability
for tests that named the old box id," stop and look harder. The
tests are usually the wrong audience for that preservation; the user
is.

## Adjacent pattern — softening reality

There is a softer version of this trap that doesn't involve tests:
adding a back-compat shim in user-facing output ("we'll still print
the old line so people parsing logs by hand don't have to update").
This is sometimes the right call, but more often it's the same
refusal-to-commit dressed up as kindness. If the old line was a lie
under the new model, printing it still is a lie. The graceful path
is usually: print the truth, document the change, let downstream
adapt — once. Carrying both forever is the cost of having lied once,
and that cost compounds.
