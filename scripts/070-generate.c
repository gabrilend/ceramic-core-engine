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
             const char *out_path);

/* {{{ static void usage() */
static void usage(void)
{
    fprintf(stderr, "usage: generate <output.c> <box.c>...\n");
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

    const char **sources = (const char **)&argv[2];
    int n_sources = argc - 2;

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
        ge_emit(&d, sources, n_sources, out_path);

    gp_free(&d);
    return 0;
}
/* }}} */
