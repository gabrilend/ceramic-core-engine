# 906 — An error reaches the host before the engine stops

## Current behaviour

**Every refusal writes to standard error and ends the process.** A
malformed map file, a wire whose two ends disagree about a size, an
out-of-memory task — each prints a sentence naming what was wrong and
then the program is over.

Inside this repository that is right and it is the house rule: errors
over fallbacks, fail loudly, never silently substitute a default. A
wrong answer that keeps flowing is worse than no answer.

**In somebody else's program it is right and unusable at the same
time.** A host with its own log has no way to capture what happened; the
message goes to a stream they may have redirected, and then their
process is gone. The engine's diagnosis is correct and illegible.

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

- [106 — Stopping on purpose](completed/106-stopping-on-purpose.md), the
  existing exit codes and the report a dying program writes
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where this was settled as a decision before being written as an issue
