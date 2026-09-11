# 108 — Choosing where a box runs

> **Confer before building any of this.** Nothing here exists in code,
> and this issue is downstream of
> [107](107-several-queues-a-station-may-name.md), which is itself
> unsettled. Two of its own decisions have already been reversed: the
> pool's placement was written as something a creator hands out, before
> it turned out there is no creator to hand it out; and the question
> underneath that — what *shared memory* means here — had to be settled
> in note 090 before any of the steps could be trusted. Seven open
> questions remain unanswered at the bottom. **Ask explicitly before
> starting any step below.**

A station may say which hardware it is willing to run on, and the pool
obeys it.

Today a task can be run by any worker and a worker can be run by any
processor, and both of those are the same sentence read from two ends.
This issue keeps that as the default and adds a way for one station to
say otherwise — *this one runs on the second socket*, *this one runs
anywhere except the core the interrupts land on*, *these two never
share a core* — written in the map file beside the station it applies
to, checked before the program runs, and reported afterwards in terms
of where the box actually ran.

**It is a placement of a station, not of a box.** A box is a C
function compiled into the binary and it has no location; a station is
one placement of that box in one map, and it is the thing that owns
buffers, a mutex, and now a home. Two stations placing the same box
may sit on different sockets. (The one reading under which a *box*
would carry the constraint — a box that touches a device attached to
one socket — is open question 1.)

---

## Current behavior

**Nothing is placed and nothing is asked.** The pool decides a worker
count — the argument if positive, else the `CERAMIC_WORKERS`
environment variable, else the online processor count — spawns that
many threads, and the kernel puts them wherever it likes and may move
any of them to a different processor at any moment. There is one
queue, strictly first-in-first-out, and every worker is eligible for
every task in it.

A station has no field that could hold a constraint. The map file has
no line that could express one. The pool has no call that could be
told one, and by [P7](../docs/058-guarantees.md) it interprets nothing
about a map, which is the layering this issue has to change carefully
rather than break.

**Memory lands wherever the thread that first touched it happened to
be.** A station's slot pages and every task allocation are ordinary
heap, so under the kernel's default policy the physical page comes
from the node of whichever core faulted it in — which is whichever
core the kernel had put that worker on at that instant. Nothing is
wrong with the result; nothing chose it either.

**The intent already exists and says so.**
[090 — One station table per processor](../docs/implementation-notes/090-one-table-per-processor.md)
states that a station table belongs to one physical processor and that
a program spanning two of them is two programs talking through their
doors — and then says plainly that nothing pins a table to a processor
yet, that there is no affinity call and no allocation on a particular
node, and that the note exists so that when any of it is built it is
built toward something. **This is that.**

---

## What a constraint may name

The vocabulary is the hardware, from the smallest nameable thing
upward. The right-hand column is where the truth is read from on
Linux, because the engine should learn the machine's shape by reading
the machine rather than by being told.

- [ ] **`cpu N` — a logical processor.** What the kernel calls a CPU
      and what a scheduling mask has one bit for. On a machine with
      simultaneous multithreading this is one hardware thread, not one
      core. Read from `/sys/devices/system/cpu/online`.
- [ ] **`core N` — a physical core**, meaning every logical processor
      that shares its execution units. Read from
      `/sys/devices/system/cpu/cpuN/topology/core_id` and
      `.../thread_siblings_list`. **A core id is unique only within
      its package**, which is the single most useful reason for the
      `and` operator below.
- [ ] **`package N` — a physical processor**: the socket, the thing
      with its own memory controller. This is what note 090 means by
      *processor*. Read from `.../topology/physical_package_id`.
- [ ] **`node N` — a NUMA node**, a memory-locality domain. Usually
      one per package and not reliably so. Read from
      `/sys/devices/system/node/nodeN/cpulist`.
- [ ] **`cache3 N` — an L3 cache domain**: the set of logical
      processors that share one last-level cache. Read from
      `/sys/devices/system/cpu/cpuN/cache/indexK/{level,shared_cpu_list}`.
      This is the level that matters most to this engine and the one
      an author is least likely to know the number of, which is why
      the relative terms below exist.
- [ ] **`worker N` — one of the engine's own threads.** The only term
      that names something in software. A worker is a thread this
      program made; a cpu is a place the kernel may run a thread. They
      are different nouns and the word *thread* is ambiguous between
      them, which is why neither is spelled *thread*.
- [ ] **`any` — every logical processor this process is permitted to
      use.** The default, and the thing every other term is a subset
      of.

**Two relative terms, which are the portable ones**, because a map
naming `cpu 6` is a map about one machine:

- [ ] **`near <station>`** — the same last-level cache domain as that
      station. What a producer and its consumer want, so the value
      handed between them does not cross an interconnect.
- [ ] **`apart <station>`** — never the same physical core as that
      station. What two hot boxes want, because a pair of hardware
      threads on one core share execution units and each runs at
      roughly half speed when both are busy.

**Lists and ranges are written the way the kernel writes them** —
`cpu 0-3`, `cpu 0,2,4-7` — so that a line copied out of
`/sys/.../cpulist` pastes in unaltered. One spelling, and it is
somebody else's, which means it cannot drift.

---

## And, or, not

Every term above denotes **a set of logical processors**, so a
constraint is a set expression and the operators are the three set
operations. That is the whole grammar; there is nothing else to learn
and one representation — a bitmask — to implement.

| written | means | reads as |
|---|---|---|
| `a or b` | union | either will do |
| `a and b` | intersection | both must be true of it |
| `not a` | complement, within what is permitted | anywhere but there |
| `( … )` | grouping | as usual |

Precedence is `not`, then `and`, then `or`, which is the same order C
uses, so nobody has to learn a second one.

```
on package 0                       anywhere on the first socket
on cpu 0-3 or cpu 8-11             either of two explicit ranges
on package 0 and core 2            core 2 of socket 0, not core 2 of socket 1
on any and not core 0              anywhere except the core the interrupts land on
on node 1 and not cpu 12           that node, minus one processor kept clear
on near mixer and apart sampler    the portable form: close to one, off another's core
on worker 3                        this station runs on that thread and no other
```

**Intersection is not decoration.** `core 2` alone is ambiguous on any
two-socket machine because core ids repeat per package, and the two
honest answers are to refuse it or to make the author say which
package. `package 0 and core 2` is the author saying it, in the
operator that already means what they mean.

**A constraint that is empty is refused, not ignored.** An expression
that intersects to no processor at all — `package 0 and package 1`, or
a cpu number this process is not permitted to use — stops the program
and names the station, the expression, and the mask that came out of
it. This is [E1](../docs/058-guarantees.md): a silently widened
constraint is a fallback, and a fallback is a warning, and a warning
is an error.

**And what is permitted is asked for, not assumed.** The starting set
for `any` and for `not` is `sched_getaffinity` on the process, which
already reflects `taskset`, a cpuset, and a container's
`cpuset.cpus.effective`. A program in a two-core container that reads
the machine's forty cores out of `/sys` and pins to one of the
thirty-eight it may not touch fails at the pin, late and confusingly;
intersecting first fails at the parse, early and by name.

---

## Where it is written

A fourth attribute line under a station, alongside `in` and `out`:

```
station churn (math.c:grind)
  on package 0 and not core 0
  in 1 $0
  out 0 - printer.0
```

**The map format was prepared for this and it is worth saying why.**
[607](completed/607-no-reserved-words.md) made the first word of every
line a keyword and the second always a name, precisely so that a
format gaining a keyword does not take that word away from every map
already written. `on` is the first keyword added since that rule, and
it is the test of it: a station called `on` is still written
`station on keep p` and nothing about it is a special case.

**The dump has to write the line back**, or [C5](../docs/058-guarantees.md)
— dumping a dump yields the dump — stops holding. It writes the
expression the author wrote rather than the mask it resolved to, for
the same reason the dump writes a bare box name rather than a
scratch-directory path: the resolved form names this machine and this
run, and the written form is what a later process can act on. The
resolved mask goes beside it as a `#` comment, which is what comments
in this format are for.

**Absent means unconstrained**, and the dump writes nothing for it.
This format writes exceptions.

---

## What has to change underneath

The vocabulary is the easy half. A station saying where it will run
means a worker can be ineligible for a task, and that single fact
reaches the kernel call, the queue, the termination rule, and two
guarantees.

### What the affinity call actually does

Everything below rests on one kernel call, so it is written out here
rather than referred to.

**`sched_setaffinity(tid, size, mask)` takes a flat array of bits.**
The type is `cpu_set_t`, 1024 bits by default (`CPU_SETSIZE`), with
`CPU_ALLOC` for a machine holding more logical processors than that.
**Bit *n* stands for logical CPU *n* in the kernel's own numbering** —
the same number that appears as `processor:` in `/proc/cpuinfo`, as
the directory `/sys/devices/system/cpu/cpu6`, and as the `cpu6` line
in `/proc/stat`. A logical CPU is one hardware thread, so on a machine
with simultaneous multithreading two of these numbers are two hardware
threads sharing one core's execution units.

**Those numbers are enumeration order from firmware, not topology.**
CPU 0 and CPU 1 may be two threads of one core or two cores on
different sockets, and nothing about the number says which. That is
why 108a reads `/sys/devices/system/cpu/cpuN/topology/` instead of
doing arithmetic on CPU numbers, and it is the reason `core`,
`package` and `node` are separate terms rather than divisions of a
CPU number.

**The first argument is a thread**, despite being typed `pid_t`: it is
a TID from `gettid()`, and zero means the calling thread.
`pthread_setaffinity_np` is glibc's wrapper taking a `pthread_t`, and
is what a worker pinning itself will use.

**What the mask constrains is three distinct moments.** The scheduler
keeps one run queue per logical CPU; a runnable thread sits in exactly
one of them at any instant and executes on that CPU when selected.

| moment | what the mask does |
|---|---|
| **wakeup** | when a sleeping thread becomes runnable, the CPU whose run queue it is put on is chosen from the set bits |
| **load balancing** | the scheduler moves runnable threads between run queues to even out load; migration targets are chosen from the set bits |
| **the call itself** | a thread currently executing on a CPU whose bit was just cleared is migrated off immediately, not at some later convenience |

So the precise promise is: **the thread is never enqueued on, and
never executes on, a logical CPU whose bit is clear.** What the mask
does **not** do is choose among the CPUs whose bits are set — the
scheduler still picks freely and may move the thread between them at
any moment. Pinning to a set of eight is not pinning to one, and a
station constrained to `package 0` has said nothing about which of
that package's cores it lands on.

**It is a restriction and there is no preference form.** The interface
offers only the hard version, which is why open question 7 could not
have been answered by asking the kernel for a hint. And the mask is
intersected with what the thread is already permitted — a cpuset or a
container's `cpuset.cpus.effective` — so a mask containing no
permitted CPU fails the call with `EINVAL` rather than being widened.
That is the same refusal this issue wants, arriving from below, and it
is the reason for reading `sched_getaffinity` first: failing at the
parse names the station, while failing at the call names a number.

**One case bends the promise and it must be detected rather than
assumed away.** If every logical CPU in a thread's mask goes offline —
CPU hotplug, a machine reconfigured while a program runs — the kernel
does not park the thread forever. It **breaks the affinity** and runs
it somewhere else. That is a fallback happening below the engine,
silently, which is the shape this project calls an error. A worker
must therefore be able to notice that where it is running is no longer
inside what it announced, and say so, rather than continuing to serve
a class it no longer belongs to.

### How a placement takes effect

**The queue arrangement is decided elsewhere and this issue inherits
it.** [107](107-several-queues-a-station-may-name.md) makes a queue a
**destination** — a queue plus the set of servers that draw from it —
and gives each server an ordered array of the destinations it serves,
with the default one last. A placement is then not a new mechanism at
all. **It is a way of computing a server set from a description of
hardware**, and the destination that results takes its place in the
arrays of the servers it belongs to.

```
worker i:  [ ...destinations this worker serves, placements among them..., default ]
```

**Interning the masks is the whole of it.** The distinct masks
appearing in a program are few — usually exactly one, *anywhere* — so
each distinct mask becomes one destination, whose server set is every
worker whose announced home satisfies it. A station carrying no
placement delivers into the default destination, exactly as it does
today; a station carrying one delivers into the destination its mask
resolved to.

**A placement is a described destination, and 107 also allows a named
one.** `to slow-lane` states a server set; `on package 0 and not core
0` computes one. Whether those are one map-file line with two
notations or two lines with two meanings is 107's first open question,
and it is the same question read from this end.

**The destination table grows the way the station table does** —
appended shelves, nothing ever moved (issue 211) — because
construction is legal at any moment and a station placed at runtime
may carry a mask nobody has seen. A fixed maximum decided when the
workers start would be a limit that fails later, inside a program that
is already running.

**What 107 already spent, this does not spend again.** Global
first-in-first-out is weakened by 107 to per-queue order plus the
order of each server's source array, so a placement's destination is
one more first-in-first-out queue in an ordered list rather than a
further weakening. What 107 does **not** spend is U1 — its default
destination is served by every worker, so nothing there makes a server
idle while work exists. **This issue is where U1 is genuinely sold**,
and it cannot be avoided the way 107 avoids it: a destination whose
server set is narrowed by a hardware expression is a destination that
must not be widened, since widening it is precisely what a placement
forbids.
**Two alternatives, recorded because they were real.**

*One queue, workers skipping what they may not run.* Keeps a single
ring and costs a scan: a worker holding the queue mutex walks past
tasks it is ineligible for. The walk is unbounded in the length of the
queue, it happens on every take, and it happens under the one lock
every push also wants. Rejected on that alone.

*The pusher choosing a worker, plus stealing.* An idle worker takes
from a busy one, skipping what it may not run. This is the general
answer and a much larger engine — it needs a load-balancing policy
this project does not have and would then have to defend, and a
worker's queue stops being private, which is what 107 relies on to
keep the last sleeper's final look cheap. It is where this goes if
class rings ever prove too coarse.
### The workers need homes first, and nobody hands them one

A class is decided by which processor a worker is on, so **a worker
whose processor the kernel may change cannot belong to a class.** The
feature therefore requires that workers be pinned, and the question
that follows is who does the pinning.

**Not a creator, because there is no creator.** The pool and every
shared component live in shared memory; there is no main thread that
owns the pool, decides its shape, and hands out placements. So a
central *pool placement policy* — a single argument given once at
creation, naming where everybody goes — has nowhere to be given from
and nobody to give it.

**So a worker announces its own home.** Each worker, as it joins,
pins itself and writes the mask it pinned to into its own record in
the shared structure. The pool's shape is therefore **learned from
its members** rather than configured: the set of processors this pool
can reach is the union of what its workers announced, and the class
eligibility lists are derived from those announcements the same way.

Three consequences, all of them simplifications:

- **A worker that joins late is not a special case.** It announces,
  its eligibility is computed, and it starts drawing. Nothing had to
  know in advance how many there would be or where.
- **A worker already placed by somebody else is honored as-is.**
  Launched under `taskset`, inside a cpuset, or in a container, a
  worker reads what it is permitted (`sched_getaffinity`) and
  announces that. It does not have to be told what it already is.
- **The refusal has a subject.** A station constraint that intersects
  no announced home is refused naming both — what the station asked
  for and what the pool actually reaches — rather than pointing at a
  policy nobody set.

How a worker decides what to pin to. The checklist is what a worker
may be *told*, by argument or environment, and the first is the
default:

- [ ] **unpinned** — today's behavior, and still the default. The
      worker announces the full permitted set, the kernel places and
      migrates it, and it belongs to no class but the unconstrained
      one. A program with no `on` line anywhere is exactly the program
      that runs today.
- [ ] **one processor each** — worker *i* pins to the *i*th permitted
      processor. The straightforward throughput arrangement, and the
      one that needs no coordination: each worker computes its own
      answer from its own index.
- [ ] **one physical core each**, siblings left unused. Fewer workers,
      each with a core to itself, for maps whose boxes compete for
      execution units rather than waiting on anything.
- [ ] **within a named set** — the worker pins inside an expression
      given to it, using the same grammar a station uses. Several
      workers given the same set is note 090's *one pool per
      processor* arrangement, arrived at by each worker doing the same
      thing rather than by anybody arranging it.
- [ ] **already placed** — pin to nothing, announce what
      `sched_getaffinity` reports. For a worker whose placement was
      decided outside the process entirely.

**A station constraint that no announced home can satisfy is refused**,
naming the station, the expression, and the union of the homes. Not
widened, not ignored, not best-effort. The alternative — quietly
honoring constraints when it can — is the exact shape of a fallback
this project treats as an error.

### The termination rule, which is the delicate part

[104](completed/104-termination-by-last-sleeper.md)'s rule is that the
worker whose registration brings the sleeper count to the worker total
re-scans the queue before anyone sleeps. With several queues:

**The last sleeper re-scans every class, not only its eligible ones.**
A task in a class this worker cannot serve is still work that exists,
and a re-scan that missed it would declare completion with tasks
queued — which is the silent-wrong-answer failure 104 exists to
prevent, reintroduced by the back door.

**And a class no worker can serve is a hang, so it is refused before
it can happen.** If a mask intersects no worker's pin, the last
sleeper correctly sees a non-empty queue, correctly declines to
declare completion, and nothing can ever run that task. Validation
therefore checks every class against the union of the workers' pins
when the program is declared finished, and the same check runs at the
moment a station is placed or repinned on a running program — the same
rule, both times, the way a wire is checked at startup and at the
moment of the edit.

---

## What it costs

The page a concurrency change answers to is
[058 — Guarantees](../docs/058-guarantees.md). Four entries move, and
one of them was already moving before this issue reached it —
[107](107-several-queues-a-station-may-name.md) replaces one queue
with several, which is what weakens ordering.

| # | today | after |
|---|---|---|
| **U1** | No worker sits idle while a task is ready to run. | **This is where it is sold.** 107 does not spend it: its default destination is served by every worker. A placement narrows a destination's server set on purpose, so a worker can sleep while a placed queue holds work — and unlike every other case in the engine there is no widening that would fix it, because widening is what a placement forbids. It is the author's to pay knowingly: a constraint is somebody saying *I would rather this ran there than soon.* |
| **P3** | First in, first out. | **Already weakened by 107** to first-in-first-out within each queue plus the order of each server's source array. A placement's destination is one more queue in that order, so this issue does not weaken it further — it populates a structure that had already given up global order. Whatever answer 107 takes for its starvation edge covers this one. |
| **P5** | When the pool declares completion, no task remains and nobody could enqueue one. | **Unchanged in statement, wider in scope**: 107 already widens the last sleeper's final look to every queue, and a placement's destination is one of them. What this issue adds is the refusal that keeps the widened rule from becoming a hang — a destination whose server set is empty is refused when it is named. |
| **P7** | The pool knows nothing about boxes, stations, or maps. It ferries four things it does not interpret. | **The pool gains one number it does interpret, and it is not a map fact.** Delivery resolves a station's placement to a destination index; the pool reads that index to choose a queue. It still does not know what a station is. The layering holds because *which queue* is scheduling, which is the pool's own business, and the translation from *station* to *destination* happens on the delivery path where every other map fact already lives. |

**And one new class of refusal**, which is a gain rather than a cost:
an empty mask, a constraint under an unpinned pool, and a destination
no worker can serve are each named and fatal before anything runs.

---

## Memory follows the thread, nearly for free

Pinning threads while leaving memory where it fell buys much less than
it looks like. The finding that makes the cheap version work:

**Under the kernel's default policy a page is allocated on the node of
the core that first writes it.** So a station's slot pages, allocated
by a worker that is already pinned, land on that worker's node without
a single call to `mbind`, `set_mempolicy`, or libnuma. The rule is one
sentence — *a station's storage is allocated from a thread already
placed where that station will run* — and the operating system does
the rest.

That is worth taking before any explicit NUMA allocation is
considered, because it costs one ordering constraint instead of a
dependency. Explicit placement is named here and left out of scope; if
it is ever wanted, `set_mempolicy` on the worker at startup is a
smaller change than per-allocation `mbind`.

---

## How this splits

Sub-issues in dependency order. Each is buildable and testable with
only its predecessors present.

| | what it is |
|---|---|
| **108a** | **Reading the machine.** The topology walk over `/sys`, intersected with `sched_getaffinity`, producing the set of logical processors and, for each, its core, package, node and L3 domain. Standalone, testable by printing what it found beside what `lscpu` says. |
| **108b** | **The expression.** Parser and evaluator over 108a's tables: terms, `and`, `or`, `not`, parentheses, kernel-style ranges, and the refusal of an empty result. Pure text in, mask out. |
| **108c** | **Workers announce their homes.** A worker pins itself with `pthread_setaffinity_np` as it joins and writes the mask it took into its own record; the pool learns its reach as the union of those. Includes the ways a worker may be told what to pin to, and the refusal of a constrained station no announced home can satisfy. |
| **108d** | **Class queues.** Interning masks, the ring per class, the sequence number, the oldest-of-heads pick, the eligibility list per worker, the last sleeper's widened re-scan, and the shelf-growing class table. |
| **108e** | **The map file line.** `on` in the parser, on the station record, in validation, and in the dump — including the round trip. |
| **108f** | **The relative terms.** `near` and `apart`, which resolve against other stations and therefore cannot be evaluated until the whole program exists; and the re-check when a station named by one is placed, removed, or repinned. |

**108f is the one with a genuine ordering problem.** `near mixer`
cannot be resolved while `mixer` may not exist yet, and both may be
placed on a running program. It is last for that reason and may want
its own answer rather than an obvious one.

---

## Suggested implementation steps

**One thing had to be settled before any of these, and it has been.**
*Shared memory* here means **one process whose threads share an
address space** — the pool, the station table and the queues are
ordinary heap that every worker can already see, mutexes are ordinary
mutexes, pointers are pointers, and a box is a function in this
binary. *No main thread* means no privileged owner among the workers,
not several processes mapping a region. So a worker's announcement is
a write to a struct, nothing becomes an offset, and
[090](../docs/implementation-notes/090-one-table-per-processor.md)'s
open question is closed in favour of its first reading.

1. **Read the machine and print it** (108a). No engine changes at all.
   A small program that dumps the topology it derived is the artifact,
   and comparing it against `lscpu` on two differently shaped machines
   is the test.
2. **The expression, offline** (108b). Text in, mask out, with the
   refusals. Testable entirely against synthetic topology tables, which
   is what lets a two-core build machine test the two-socket cases.
3. **Workers announce their homes, and nothing obeys them yet**
   (108c). Each worker pins itself as it joins and writes down the
   mask it took; the pool exposes the union. At the end of this step
   the engine's threads have places and no station can say anything
   about them — which is already note 090's coarse win, and it is
   reached without anybody having arranged it centrally.
4. **Class queues with exactly one class** (108d, first half). Restructure
   to a ring per class while every program still has only the
   unconstrained one. Every existing test must pass unchanged, which is
   the point: the restructuring is proven before it carries weight.
5. **More than one class** (108d, second half). Eligibility lists, the
   sequence number, the widened re-scan, the refusal of an unservable
   class, and a test that provokes a full sleep with work outstanding
   in another class.
6. **The map file line** (108e), including the dump round trip.
7. **Where it actually ran** — the reporting below — before the
   relative terms, because 108f cannot be believed without it.
8. **The relative terms** (108f).

---

## How it is tested

**A constraint is only real if you can watch it hold.** The direct
test: a box that records `sched_getcpu()` on every invocation, a
station constrained to a mask, a few thousand invocations, and an
assertion that the set of processors observed is a subset of the
declared mask. Nothing about that test is indirect and nothing about
it depends on timing.

**The masks in tests are derived, never written down.** A test that
names `cpu 6` fails on a four-core build machine, which teaches
nothing about the code. Tests take the first two permitted processors
from 108a and build their expressions out of those, and any test whose
shape the machine cannot supply — two sockets, say — says it is
skipping and why, rather than passing quietly.

**Every refusal gets a test**, because the refusals are the feature:
an empty mask, a constraint under an unpinned pool, a class no worker
can serve, and a cpu number outside what the process is permitted.

**And the termination race gets one** — a program whose only remaining
work sits in a class every awake worker is ineligible for, driven to
the point where the sleeper count is full, asserting that the re-scan
finds it. This is 104's test written again against the new shape, and
it is the one failure mode here that looks like success.

---

## Reporting, so a constraint can be believed

Phase 7's per-station statistics gain **which processors this station
actually ran on, and how many times each**. Without it a placement is
an assertion in a file; with it, it is an observation.

Two things follow that are worth having anyway: a station whose
observed set is smaller than its permitted set is a station that could
be told something more specific, and a station whose *unconstrained*
runs cluster on one node is one that was already local and needs no
line at all.

---

## Open questions

1. **Does a box ever carry a constraint, or only a station?** A box
   that touches a device attached to one socket wants the same
   placement wherever it is placed, and saying so once beats saying it
   at every station. Against: a box is a plain C function with no
   location, and the moment one carries a constraint the generator has
   to have a way to express it in C, which is a second place a
   placement can be written. Current position: **station only**.
2. **What is the term for an L3 domain called, and are other cache
   levels nameable?** `cache3 N` reads badly and `l3 N` reads like a
   typo. And if L3 is nameable, somebody will ask for L2.
3. **Is `worker N` a good idea at all?** It is the most direct thing
   an author can say and the least portable — worker counts vary by
   machine, so `worker 15` is a map that only runs on a big enough
   one. It may be that naming a worker should be refused in a file and
   allowed only from the construction surface.
4. ~~Should the pool's own placement be written in the map file?~~
   **Answered: the question was wrong.** It assumed a creator — some
   thread that makes the pool, chooses where it goes, and hands the
   placement to it. There is no such thread. The pool and every shared
   component live in shared memory and there is no main thread, so a
   central placement has nowhere to come from. **A worker announces
   its own home instead**, and the pool's reach is the union of what
   its members announced. Written into *The workers need homes first*
   above.
   **And the question underneath it is answered too**, which
   [090](../docs/implementation-notes/090-one-table-per-processor.md)
   had left open: *shared memory* means **one process whose threads
   share an address space**, not several processes mapping a region.
   *No main thread* means no privileged owner among the workers. So
   mutexes stay ordinary, pointers stay pointers, a box stays a
   function in this binary, and a worker's announcement is a write to
   a struct.
5. **What happens to a constrained station when the program is
   composed into another** (issue 217)? One station table, one
   processor, per note 090 — so a composed subgraph's constraints are
   interpreted against the parent's pool, and its `cpu 6` may mean
   something else there. Refuse? Rewrite? Ignore?
6. **Does `near` follow, or only place once?** If the station it names
   is later repinned, is `near` re-evaluated — which means a station's
   home can change while it runs, which means a class can change under
   queued tasks — or is it resolved once, at placement, and thereafter
   an ordinary mask?
7. ~~Is there a soft form?~~ **Answered: no. A placement is a hard
   promise.** `sched_setaffinity` is a hard restriction and there is
   no *prefer*, and the soft version would have had to be built out of
   eligibility instead — a worker draining its preferred classes first
   and taking from any class when starved. That keeps U1 whole and
   makes a placement a tendency, which is the opposite of what a
   person asking *run this box on that core* is asking for. A
   placement that quietly ran somewhere else would be a fallback used
   silently, which this project treats as an error. **The idle worker
   is therefore the accepted price**, written into the U1 row above,
   and a soft form is not a smaller version of this feature but a
   different one that can be argued for separately if the idling ever
   proves worse than the locality is worth.
8. **What does this mean on machines that are not Linux?** Every
   mechanism named here is Linux, `/sys` and all. macOS has no thread
   affinity worth the name. Is the answer that the feature is refused
   elsewhere, or that the whole engine is already Linux and this only
   makes it visible?
9. **How does a worker notice that the kernel broke its affinity?**
   The cheapest answer is `sched_getcpu()` at a task boundary, checked
   against the mask the worker announced — one instruction-cheap call
   per task, and it catches the hotplug case within one task rather
   than never. The question is whether per-task is the right cadence,
   whether it should instead be per-N-tasks or on a timer, and what
   the engine does when it finds out: stop the program, or refuse only
   the constrained classes and keep serving the unconstrained one.

---

## Related

- [107 — Several queues, and a station may name one](107-several-queues-a-station-may-name.md),
  **the prerequisite**. It replaces the single ring with an ordered
  array of queues per worker, and a placement is more entries in that
  array rather than an arrangement of its own. It also spends the two
  guarantees this issue would otherwise have had to spend alone.
- [090 — One station table per processor](../docs/implementation-notes/090-one-table-per-processor.md),
  which states the intent this builds, and whose own open question —
  *one process across sockets, or several processes sharing memory* —
  is upstream of question 4 here
- [006 — Scheduling](../docs/006-datapath-scheduling.md), the document
  this changes: one queue becomes one per class, and the last
  sleeper's look widens
- [058 — Guarantees](../docs/058-guarantees.md), where U1 becomes
  conditional and P7 gains the one number the pool interprets
- [104 — Termination by the last sleeper](completed/104-termination-by-last-sleeper.md),
  whose rule this must not break, and whose test is written again here
- [102 — Workers and the run loop](completed/102-workers-and-run-loop.md)
  and [101 — The task queue ring](completed/101-task-queue-ring.md),
  the two this restructures
- [211 — Growing the station table](completed/211-growing-the-station-table.md),
  whose shelf trick the class table borrows
- [212 — One way to build a program](completed/212-one-way-to-build-a-program.md),
  which makes placement legal at any moment and therefore makes the
  class table have to grow
- [607 — No reserved words](completed/607-no-reserved-words.md), which
  is what makes adding the `on` keyword safe, and whose first real test
  this is
- [703 — The map dump](completed/703-map-dump.md), which must write the
  line back for C5 to keep holding
- [702 — Station statistics](completed/702-station-statistics.md),
  where *which processors this station ran on* belongs
