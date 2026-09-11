/*
 * 143-cerac.h — what the compiler carries inside itself, declared.
 *
 * What this is: the one type and the one table that let `cerac` hold
 * the engine's own source as part of its executable. The engine is
 * three files in this repository — the header, the body, and the
 * linker's export list — and `cerac` is built from the generator's
 * sources plus a generated file defining the table below, whose
 * entries are those three files turned into C string literals.
 *
 * How it does it, in general terms: the build runs in two stages.
 * Stage one compiles `generate`, which is the ordinary build-time
 * generator and knows nothing about any of this. Stage two runs
 * `generate --embed` over the engine's files to write that generated
 * file, then compiles `cerac` from the generator's own sources plus
 * it. There is no bootstrap problem, because the thing doing the
 * embedding does not need to have been embedded.
 *
 * Why a table rather than three named arrays. `--unpack` writes the
 * files back out under the names they had, and a table carries the
 * name beside the bytes so that adding a fourth file is a change to
 * one command line rather than to three declarations and a writer.
 */
#ifndef CERAC_H
#define CERAC_H

/* {{{ type cerac_embedded_t */
/*
 * One file carried inside the executable.
 *
 * | field  | type          | what it holds |
 * |---|---|---|
 * | `name` | `const char *` | the file's basename as it stood in `src/`, which is the name `--unpack` writes it back out under |
 * | `text` | `const char *` | the file's bytes, exactly as they were read, NUL-terminated |
 *
 * The text is the source that was compiled, not the source that is on
 * somebody's disk now — which is the same property the box sources
 * riding in a built program already have, and the reason either is
 * worth carrying.
 */
typedef struct {
    const char *name;
    const char *text;
} cerac_embedded_t;
/* }}} */

/* The engine's files, in the order the command line named them, which
 * for the build is the order they have to be concatenated in: the
 * header before the body, because the body's declarations have to be
 * in scope before its definitions arrive. */
extern const cerac_embedded_t cerac_embedded[];
extern const int              cerac_n_embedded;

/* Finds one by name, or null. The caller decides what a missing file
 * means; nothing here substitutes a default for one, because a `cerac`
 * built without the engine in it is a broken build rather than a
 * degraded one. */
const char *cerac_embedded_text(const char *name);

#endif
