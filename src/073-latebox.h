/*
 * 073-latebox.h — boxes that arrive after the program started.
 *
 * What this is: the door through which C source becomes a box a
 * running program can place. Everything a build does to a box source,
 * done later: run the generator over it, compile the result, load it,
 * and add what it found to the table stations are placed from.
 *
 * How it does it, in general terms: the same generator and the same
 * compiler the build used, invoked as programs. **The same compiler
 * matters and is not incidental** — the build records which one it
 * was and this uses that one, so a program has exactly one answer to
 * `sizeof` by construction rather than by checking (issue 310).
 *
 * Why a signature is not enough, since it is the obvious idea: a
 * signature gives names — the box's name, its parameter type names,
 * its return type name. What it cannot give is **sizes and offsets**,
 * which are what the engine actually runs on, and the standing rule is
 * that the compiler computes every one of them and nothing guesses.
 * So the signature supplies the paperwork and a compiler supplies the
 * numbers. There is no version of this that skips the compiler.
 *
 * What it costs: a program that never brings in new code never invokes
 * a compiler and never needs one present. A program that does needs
 * the toolchain, exactly then, which is the only moment anyone would
 * expect otherwise.
 */
#ifndef SORA_LATEBOX_H
#define SORA_LATEBOX_H

#include "026-registry.h"

/* {{{ registry_compile_source() */
/*
 * Compile C source into the running program and add every box it
 * defines to the table.
 *
 * Returns the number of boxes added, or -1 on failure. A failure
 * leaves the table exactly as it was — nothing is half-added — and
 * puts **the compiler's own output** on stderr rather than a summary
 * of it, because the message that says what is wrong with a piece of C
 * is the one the compiler wrote.
 *
 * The source is saved to the RAM-backed scratch tier at the moment it
 * is compiled, treated exactly like a log: written as it happens,
 * gone at reboot. That is what lets a dump taken afterwards be
 * reloaded by a fresh process on the same machine — the artifact
 * exists before anybody needs it, which is the same reasoning that
 * opens the diagnostic report's destination during startup rather
 * than while dying.
 */
int registry_compile_source(const char *c_source);
/* }}} */

/* {{{ registry_late_count() / registry_late_box() */
/*
 * The rows added after the program started, in the order they
 * arrived. The generated rows are not included: those are
 * registry_boxes and have always been reachable directly.
 *
 * Lookup by name goes through registry_find, which walks both, so
 * almost nothing needs these. They exist for a report that wants to
 * say what a program has grown, and for the tests.
 */
int               registry_late_count(void);
const box_info_t *registry_late_box(int i);
/* }}} */

/* {{{ registry_late_source_dir() */
/* Where saved sources go, so a reloader can find one from a box's
 * name alone. */
const char *registry_late_source_dir(void);
/* }}} */

#endif
