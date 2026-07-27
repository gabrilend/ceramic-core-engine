# 012-pool.c — the thread pool, from inside

The usable interface is documented in `011-pool.h.info.md`; read that
one unless you are debugging this file.

## Internal structures

**worker** — one thread's identity.
| field | type | meaning |
|---|---|---|
| thread | `pthread_t` | The operating-system thread. |
| index | `int` | Position in the worker table; what statistics key on. |
| pool | pointer | Back-reference to the owning pool. |

**pool** — everything, under one mutex.
| field | type | meaning |
|---|---|---|
| slots | array of task pointers | The ring. Holds pointers out; nothing points in. |
| capacity | `int` | Ring length. Doubles when full. |
| head, tail | `int` | Oldest task / next free cell. Equal means empty; one cell always spare so full is distinguishable. |
| mutex | `pthread_mutex_t` | Guards every other field here. One lock, one truth. |
| wake | `pthread_cond_t` | Sleeping workers wait on this; push and shutdown broadcast it. |
| start_gate, released | cond + `int` | Workers park here until release; the seeding window. |
| workers, n_workers | array + `int` | The worker table. |
| sleeping | `int` | Exact count of registered sleepers — the termination rule's input. |
| stop | `int` | Set once by the last sleeper (or early destroy); every worker that sees it returns. |
| outside | `int` | Registered outside submitters; termination waits for zero. |
| joined | `int` | Workers already collected; makes join idempotent. |
| finish, finish_ctx | fn pointer + pointer | Delivery's seat, called after each task runs. |
| high_water, growths | `int` | The queue's own story, for the demo. |

## The one piece of reasoning worth keeping

Termination: the worker whose registration as asleep makes the count
equal the worker total — while no outside submitter is registered —
looks at the queue one last time and, finding nothing, stops everyone
by broadcast. Because check-and-register happen inside a single hold
of the mutex and the condition wait releases that mutex atomically,
the lost-wakeup race the design documents warn about cannot occur
here; the final look is kept as documented protocol and as a guard
for any future refactor that splits the lock.
