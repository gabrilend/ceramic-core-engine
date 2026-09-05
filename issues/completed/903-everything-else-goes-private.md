# 903 — Everything else goes private

The payoff. [901](901-the-engine-becomes-one-file.md) made one
translation unit and [902](902-the-header-says-what-is-public.md) said
what is public; this one makes the rest stop being linker symbols.

## Current behaviour

**Done, and enforced.** The engine exported 123 symbols and now exports
100. The twenty-three that went are private — not renamed, not hidden
behind a convention, but `static`, which means they are not linker
symbols at all and a host program linking this engine cannot collide
with them however it names its own functions.

**The list was derived, not written.** Compile the engine alone, ask
the object file what it publishes, subtract what the header declares,
and what is left is exactly the set that wants the word `static`. That
is why it caught two functions nobody knew were exported.

**And it is checked.** `tests/112-test-public-surface.sh` does that
subtraction on every test run and fails naming any symbol that is
exported without being declared — pointing out that the fix is either
the word `static` or a deliberate line in the header, and that those
mean opposite things so the choice is the author's. It was checked by
being made to fail: un-privatising one function makes it report that
function by name.

**Seven white-box tests moved inside.** They examine slots, pages,
destination sets and the constants a port holds, and a private function
cannot be called from another translation unit no matter what is
declared. They include `cera.c` and are compiled as one unit with it,
under a build rule that does not also put the engine on their link line.

### What that turned up

**Two joints exist only for tests.** The ordinal form of a slot move,
and the count of things filed in the scrapyard: nothing inside the
engine calls either. Compiling the engine on its own, the compiler
correctly says they are unused.

They are marked as expected-to-be-unused rather than deleted, because
deleting one is a decision about the engine's shape rather than about
moving it. The ordinal slot move is exactly the address form applied to
a located slot, and the engine uses the address form everywhere for a
documented reason — so whether the ordinal form should exist is a real
question, and it is somebody's to answer rather than a refactor's to
assume.

## Intended behaviour

**Every file-scope definition in `cera.c` not declared in `cera.h` is
`static`.** Those functions stop existing as symbols. The renaming job
that note 057 measured at forty names shrinks to only the genuinely
public ones, and — the part that matters more — **it cannot regress**: a
function added to the engine is private by default and becomes public
only by being declared in the header, which is one deliberate act in one
file.

### The white-box tests move inside

Eight tests call joints: reaching a slot in a ring, moving a value
between slots, reading a slot's state, adding a page to a port, reading
a port's constant back as text, and reading an output port's destination
set. A `static` function cannot be called from another translation unit
no matter what is declared, so these tests have to change.

**They include `cera.c` rather than link it.** A test that examines
internals belongs inside the unit it examines; that is what makes it a
white-box test rather than a badly-scoped API. The build rule for those
tests compiles the test alone, with `cera.c` textually inside it, and
does not add `cera.c` to the link line — doing both is every symbol
defined twice.

The alternative considered and refused: keeping those eight functions
public so the tests keep working. That makes the test suite the reason
a consumer's link fails, which is the tail wagging the dog, and it
leaves the "private by default" property depending on nobody ever
writing another white-box test.

### The demo boxes stop being part of anything shipped

`src/boxes/` exports `add`, `mix`, `keep`, `nudge`, `seven`, `swallow`,
`magnitude_squared`. These are example code that the build wildcards in,
and `add` is the single most collidable symbol in C. They are not part
of `cera.c` and never were — the generator compiles them per-consumer —
but the exclusion has to be **deliberate and stated**, because right now
they are only outside by accident of which directory they sit in.

## Suggested implementation steps

1. **Derive the keep-list from the header**, not by hand. Every function
   name declared in `cera.h` stays global; every other file-scope
   definition in `cera.c` gains `static`. Deriving it is what makes the
   property hold for functions written next year.
2. **Mark them.** Functions and file-scope data both — a file-scope
   variable is a symbol like anything else.
3. **Move the eight white-box tests to including `cera.c`**, with a
   build rule that compiles them as one unit and a comment on each
   saying which joint it is inside for.
4. **State the demo-box exclusion** where the packaging decides what
   ships, so it is a rule rather than a coincidence.
5. **Count what is left.** Read the symbol table of a linked test binary
   before and after. The number of exported engine symbols is the
   measurement this issue exists to change, and it belongs in the
   issue's record rather than in anybody's memory.

## What this does not fix

**The public names are still ordinary English words.** `cera_map_create` does
not collide any less for being deliberately public. That is
[905](905-the-prefix.md), and it is a smaller job after this one because
the list it has to rename is the header rather than the engine.

## Related

- [902](902-the-header-says-what-is-public.md), which decides the list
- [905](905-the-prefix.md), which renames what is left
- [098-engine-surface.syms](../src/098-engine-surface.syms.info.md), the
  linker's view of the same boundary
