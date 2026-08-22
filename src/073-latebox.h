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

#include "018-station.h"
#include "026-emitted.h"

/* {{{ late_compile_source() */
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
int late_compile_source(const char *c_source);
/* }}} */

/* {{{ late_box_count() / late_box_at() */
/*
 * The boxes added after the program started, in the order they
 * arrived. The compiled-in ones are not included: those are
 * box_places and have always been reachable directly.
 *
 * Lookup by name goes through box_place_find, which walks both, so
 * almost nothing needs these. They exist for a report that wants to
 * say what a program has grown, and for the tests.
 */
int                late_box_count(void);
const box_place_t *late_box_at(int i);
/* }}} */

/* {{{ late_unload_box() */
/*
 * Unload a box added while the program ran, freeing the library its
 * code came in.
 *
 * **Refused while any station in the given map places it.** Unloading
 * code a station names is exactly the crash this is built to avoid,
 * and the check is the cheap half of the problem.
 *
 * The expensive half is that a worker may be *inside* that code right
 * now, having picked up a task for it a moment ago. That is answered
 * by the same per-worker counter the scrapyard uses (issue 214) — the
 * counter deliberately spans a whole task rather than a delivery
 * walk, so it answers "might somebody be inside this box" and "might
 * somebody be inside this station" with one number. The library is
 * handed to the map's scrapyard and closed once nobody can be in it.
 *
 * **What it checks is the map you hand it.** A process running
 * several maps could have another one placing this box, and nothing
 * here can see that, because nothing in this engine is process-wide
 * and there is no list of running maps to consult. Unloading a box
 * another map places is the caller's to avoid; it is stated rather
 * than defended, which is the same footing as reaching into a program
 * by anything other than its input and output stations.
 *
 * Returns 0, or -1 with a reason on stderr.
 */
int late_unload_box(map_t *m, const char *name);
/* }}} */

/* {{{ late_source_dir() */
/* Where saved sources go, so a reloader can find one from a box's
 * name alone. */
const char *late_source_dir(void);
/* }}} */

/* {{{ late_source_text() — issue 311d */
/*
 * **The C one late-arriving source was compiled from**, by the path
 * it was compiled under, or NULL.
 *
 * The text is not copied here and nothing allocates: a loaded object
 * carries its own source as a C array
 * ([311c](../issues/completed/311c-source-rides-in-the-binary.md)),
 * exactly as the program's own generated file does, so this returns a
 * pointer into the loaded object and lives as long as it does.
 *
 * Callers should reach for `box_source_text` instead, which asks this
 * after asking what the build compiled in. Two lookups exist because
 * the two tables live in different objects; one question is asked, and
 * whether a box arrived early or late is not part of it.
 */
const char *late_source_text(const char *path);
/* }}} */

#endif
