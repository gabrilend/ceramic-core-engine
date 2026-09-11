/*
 * 149-same-program.h — comparing two dumps on what they say about a
 * program rather than on where its code came from.
 *
 * What this is: one rule, used by every test that builds the same
 * program two ways and asserts the two are the same. Comparing dumps is
 * the cheapest complete proof available — the dump walks the live
 * station table and writes what is actually there, so two identical
 * dumps are two identical tables, including everything a hand-written
 * comparison would forget.
 *
 * How it does it, in general terms: a station's box address says which
 * file the box was compiled from, shortened against whatever root did
 * the compiling. Two construction paths compile from two roots — a map
 * file is compiled where it sits (issue 611), so its boxes are named
 * relative to that map's directory, while the boxes a binary was built
 * with are named relative to the tree that built it. Same box, same
 * code, two provenances.
 *
 * Provenance is a fact about where code came from. These tests are
 * about the program. So the path is dropped and the function name kept,
 * and **everything else is still compared byte for byte** — every port,
 * every arrow, every value, every kind, every order — which is the
 * whole reason the comparison is a text comparison in the first place.
 *
 * Why it is a header rather than a copy in each test: it is a rule
 * about what two dumps being equal means, and a rule with two copies is
 * two rules that agree until one is edited. This project has relearned
 * that with the map format's two writers and with the escaping the
 * generator does; writing it down once is cheaper than learning it a
 * third time.
 */
#ifndef CERA_SAME_PROGRAM_H
#define CERA_SAME_PROGRAM_H

/* {{{ static void drop_box_paths() */
/*
 * Rewrites `(some/path/file.c:add)` into `(add)` in place.
 *
 * In place and shrinking, so nothing is allocated and the text stays
 * the same text: everything after an edit moves down, and a caller
 * reporting the first differing line still reports its own numbering.
 *
 * A bracket that does not close on its line is not a box address and is
 * left exactly as it is.
 */
static void drop_box_paths(char *text)
{
    char *w = text;
    for (char *r = text; *r; ) {
        if (*r != '(') {
            *w++ = *r++;
            continue;
        }
        char *close = r + 1;
        while (*close && *close != ')' && *close != '\n')
            close++;
        if (*close != ')') {
            *w++ = *r++;
            continue;
        }
        char *colon = NULL;
        for (char *p = r + 1; p < close; p++)
            if (*p == ':')
                colon = p;
        *w++ = '(';
        for (char *p = colon ? colon + 1 : r + 1; p < close; p++)
            *w++ = *p;
        *w++ = ')';
        r = close + 1;
    }
    *w = '\0';
}
/* }}} */

#endif
