# 906 — An error reaches the host before the engine stops

## Current behaviour

**Done.** A host installs a handler and is told the engine's own message
and the code the process is about to exit with, immediately before it
exits. Passing NULL removes it; with none installed the behaviour is
exactly what it was.

**The engine still stops.** The handler cannot return control, suppress
the refusal, or change the code.

### The funnel had to be built first

This issue's first step was to find the single place a refusal is
reported and confirm it was single. It was not: **thirty places wrote a
sentence to stderr and then called abort or exit**, each its own little
ending. So the funnel is the work, and the handler is the small part
that hangs off it.

Every one of the thirty now goes through one call that formats the
message, writes it to stderr as before, hands it to the handler, and
ends the process. Sorting them turned up a distinction the code was
already making without naming: most refusals are a fault outside the
engine and exit with a code a caller can act on, while six in the
delivery path are the engine finding a fault in itself and abort,
because a core is the evidence. That second kind got a name and a code
of its own.

**One death deliberately does not go through it.** The quit signal's
path takes no locks, because the reason it arrived may be that a lock is
held by something that will never release it — and an error handler is
somebody else's code, which may take a lock. It aborts directly, and now
says so.

### What the test holds down

That the handler is called, called exactly once, told the right code,
handed a message with no trailing newline, and that the process still
ends with that same code afterwards. The last one is the one that would
rot unnoticed, because it is the one somebody will eventually read as a
bug.

Each case runs in a forked child, since each ends in a dead process, and
the child reports down a pipe. The removal case installs a handler that
shouts if it runs, so "not called" is proven rather than inferred from
silence.

## Intended behaviour

**An installable handler, called with the message immediately before the
engine dies.** The host logs it, flushes its own state, and knows what
happened. **The engine still stops.**

That last sentence is the whole design and it is not a compromise. What
this must never become is an error code returned up a call chain that a
consumer may ignore — that is a fallback wearing a return type. Breaking
loudly is the feature; a map that is wrong should stop being a running
program. A consumer who wants recovery restarts the engine, and that is
the entire recovery story.

**What the handler adds is legibility, and it opens no other door.** It
cannot return. It cannot suppress. It runs, and then the engine ends as
it would have.

### What the handler is given

The message the engine would have printed, and the exit code it is about
to use. The codes already exist and are named — finished, a map file
refused, an invalid construction call, out of resources, interrupted by
a person — so a host can tell "your file is wrong" from "this machine is
out of memory" without parsing English.

### Where it is called from

One place. Every refusal in the engine already funnels through the same
reporting path, which is what makes this a small change in code and a
large one in decision. Handler absent, the behaviour is exactly today's.

## Suggested implementation steps

1. **Find the funnel** — the single place a refusal is reported and the
   process ended — and confirm it is single. If it is not, that is the
   first fix and it is worth doing regardless of this issue.
2. **Add the installer** to the public header: one call taking a
   function pointer, one taking nothing to remove it.
3. **Call it before ending**, with the message and the exit code.
4. **Test that it is called**, that it is called once, and that the
   process still ends with the same code afterwards. The third is the
   one that will rot if it is not tested, because it is the one somebody
   will later think is a bug.

## Related

- [106 — Stopping on purpose](106-stopping-on-purpose.md), the
  existing exit codes and the report a dying program writes
- [057 — Packaging](../../docs/implementation-notes/057-packaging.md),
  where this was settled as a decision before being written as an issue
