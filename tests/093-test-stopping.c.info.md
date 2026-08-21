# 093-test-stopping.c — what it proves

Eight scenes for issue 106. Each one that involves a signal runs in a
**child process**, because what is being tested is how a process dies,
and a test that could observe that from inside would be testing
something else.

The handshake is a pipe, not a sleep. A sleep long enough to be
reliable on a loaded machine is long enough to make the suite
unpleasant, and one short enough to be pleasant is a race that fails
on somebody else's laptop and nowhere else. Every scene is capped, so
a program that fails to stop reports a failure rather than hanging the
suite.

| Scene | What it proves |
|---|---|
| the entrance shuts | A closing program refuses an argument and says why. This is the polite path's only mechanism. |
| a polite shutdown | Exit zero, and **no report** — the absence is the assertion, because a supervisor stopping a healthy program did not ask for one. |
| an interrupt gathers | Exit 130, a report naming stations by their authors' names, where each worker was, and the program written out as a map file. |
| a quit takes no locks | A body left behind, and a report naming stations by index only. |
| every worker wedged | The situation all of this was designed for: no thread free, a queue that will never drain, and a full report anyway. |
| a second interrupt | A station's mutex held forever, so the first interrupt's gather never finishes — and the second one still gets out. |
| a quit through that same lock | The pair to the scene above: what the other path can still do when everything is stuck. |
| an ignored refusal | A caller asks for an impossible wire, ignores the answer, and does not reach the next line. Exit 70. |

## What writing it found

**Two of the scenes passed for the wrong reason, twice.**

First, the pool could finish between being released and anybody
sitting down to wait for it, and a waiter that then waited for a
signal nobody would ever raise waited forever. Asking to be told now
raises it immediately when it has already happened, so it does not
matter which side of the finish the waiter arrives on.

Second — and this is the one worth keeping — the child released the
workers *before* the feeder registered its standing promise. The
program seeds nothing, so in that window the queue was empty and
nobody was promising anything, and the last-sleeper rule correctly
declared the program over before it had begun. Three scenes were
asserting things about a program that had already finished. The rule
was working exactly as written; the fix is the clause it always
provided, which is to make the promise before opening the gate.

**And one guarantee was nominal until a scene demanded it.** The
second interrupt could not have worked: with every signal blocked, a
second one arriving during a gather would sit pending behind a gather
that never returns — so the escape hatch would have opened only in the
cases where nobody needed it. That is what the one handler in the
engine exists for.
