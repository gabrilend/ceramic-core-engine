/*
 * 070-generate.c — the build-time generator: box sources in, registry out.
 *
 * What this is: the program that makes "adding a box" mean "writing a
 * function". It reads the C files designated as box sources, finds
 * every function, struct, and compare function in them, and writes one
 * C file containing a shim per box, a registry of names to shims and
 * full type information, a field table per struct, and a three-way
 * compare per comparable type.
 *
 * How it does it, in general terms: it does not understand C — it
 * recognizes exactly three top-level shapes in files whose whole
 * purpose is to hold them, and stops loudly on anything else. Every
 * size and offset in its output is a sizeof or offsetof expression, so
 * the compiler computes the numbers and this program never guesses
 * about padding or alignment. Output is written to a temporary file
 * and moved into place only on success, so a failing run can never
 * leave a half-registry for the build to compile against.
 *
 * Why it is C (issue 308): a program built with this engine should
 * need a C compiler and nothing else. This was a LuaJIT script, which
 * meant every consumer of the engine inherited a build dependency on
 * an interpreter that nothing at run time used. The generator does not
 * depend on the engine it feeds, so it compiles before anything else
 * in the build exists, and there is no bootstrap problem to solve.
 *
 * Usage:
 *   generate <output.c> <box-source.c> [more...]
 *   generate --describe <box-source.c> [more...]
 *     (prints what the parser saw, for diagnosing build problems)
 *
 * Box source ground rules, enforced by the parser, documented once:
 *   - value types are `typedef struct { ... } name;` — plain struct
 *     declarations and inline nested struct bodies are refused; define
 *     nested structs separately and refer to them by name.
 *   - every non-static function is a box; static functions are private
 *     helpers; a function named `type__compare` is that type's
 *     three-way comparison and never becomes a box.
 *   - parameters and returns may be primitives, typedef'd structs, or
 *     `const char *` (a borrowed string — the bytes live wherever the
 *     pointer points, which for a static is the port's own storage).
 */
#include "067-genparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ge_emit(const description_t *d, const char **sources, int n_sources,
             const char **maps, int n_maps,
             const char *out_path, const char *root);

/* {{{ static void usage() */
static void usage(void)
{
    fprintf(stderr, "usage: generate <output.c> [--root=DIR] "
                    "[--map=FILE]... <box.c>...\n");
    fprintf(stderr, "       generate --describe <box.c>...\n");
    exit(65);
}
/* }}} */

/* {{{ main */
int main(int argc, char **argv)
{
    if (argc < 2)
        usage();

    int describe_only = strcmp(argv[1], "--describe") == 0;
    const char *out_path = describe_only ? NULL : argv[1];

    /*
     * The project root, so that a box's path can be shortened before
     * it becomes part of a generated symbol (issue 311a). Without it
     * the symbol carries the absolute path of whatever machine ran
     * the build, which is both enormous and different on every
     * machine — a generated file that differs by where it was built
     * is one nobody can compare against another.
     *
     * Optional: a box compiled while a program runs has no project
     * root to be relative to, and its path is already unique.
     */
    const char *root = NULL;
    const char *sources[argc > 2 ? argc - 2 : 1];
    int n_sources = 0;
    /*
     * **Maps the build was told about** (issue 311d). Each becomes a
     * function that builds it — construction calls, with every box
     * name resolved to a placement function while somebody can still
     * read an error message about it.
     *
     * Named with a flag rather than positionally, because a map file
     * and a box source are both `.c`-adjacent text as far as a shell
     * glob is concerned and telling them apart by extension would be
     * a rule the build has to remember rather than one the caller
     * states.
     */
    const char *maps[argc > 2 ? argc - 2 : 1];
    int n_maps = 0;
    for (int i = 2; i < argc; i++) {
        if (strncmp(argv[i], "--root=", 7) == 0)
            root = argv[i] + 7;
        else if (strncmp(argv[i], "--map=", 6) == 0)
            maps[n_maps++] = argv[i] + 6;
        else
            sources[n_sources++] = argv[i];
    }

    if (n_sources <= 0)
        usage();

    description_t d;
    gp_init(&d);
    for (int i = 0; i < n_sources; i++)
        gp_parse_file(&d, sources[i]);
    gp_validate(&d);

    if (describe_only)
        gp_describe(&d);
    else
        ge_emit(&d, sources, n_sources, maps, n_maps, out_path, root);

    gp_free(&d);
    return 0;
}
/* }}} */
