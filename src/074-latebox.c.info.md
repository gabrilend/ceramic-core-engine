# 074-latebox.c — a late box, from inside

Interface in `073-latebox.h.info.md`. Five steps: save the source,
run the generator, compile a shared object, load it, add its rows.
Nothing here re-implements any of that — a box added late goes through
the identical path a box added early did, so there is one way for a box
to come into existence rather than two that must agree.

**Two RAM tiers, and which goes where is not arbitrary.** Source text
goes to `/dev/shm`, the read tier; the compiled library goes to `/tmp`,
the execute tier. `/dev/shm` is commonly mounted so that nothing on it
may be executed, so a shared object written there compiles fine and
then fails to load with a message about mapping a segment that says
nothing about the real cause. Found by putting both in the wrong tier.

**The table grows by adding a block and never moves a row.** The
generated array is the first block and is const; later rows live in
blocks of their own, published by one pointer write so a reader either
sees a block complete or does not see it. Generated rows are searched
first, so bringing in new code can never shadow a box a map already
depends on.

**The source is filed twice**: once under a serial number, which is
what was handed over, and once per box under that box's own name, which
is how a **later process** finds it. That is what makes a dump taken
after somebody added code reloadable — `registry_recover_box` compiles
it back from the saved source when a name is not found, and says out
loud that it did, because a fallback nobody was told about is the shape
this project treats as an error.

**What is deliberately absent: unloading.** A shared object is never
closed, so a late box stays for the life of the process. Doing it
safely means waiting until no worker is inside the code being freed —
the same retire-sweep-free mechanism issues 214 and 216 need — and it
should be built once and shared rather than three times. Until then
this leaks a library per compile, bounded by how often somebody adds
code, and stated rather than hidden.
