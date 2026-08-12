/*
 * 029-demo-boxes.c — the boxes the tests and demos place in maps.
 *
 * What this is: ordinary C functions, and nothing else. Everything
 * the engine needs to call these by name — shims, the registry,
 * field tables, compare functions — is derived from this file at
 * build time. Writing a function here is the entire act of adding a
 * box; there is no registration and no list to update.
 *
 * How the file is read, in general terms: the generator recognizes
 * typedef structs (one field per declaration, nested structs defined
 * separately), non-static functions (each becomes a box), static
 * functions (private helpers, invisible to maps), and functions
 * named type__compare (that type's three-way ordering).
 */
#include <stdio.h>
#include <stdlib.h>

/* A value type: three floats, no surprises. */
typedef struct {
    float x;
    float y;
    float z;
} vec3;

/* A struct whose padding is non-obvious on purpose: the char pushes
 * the double to an aligned offset, leaving a seven-byte hole the
 * field table must step over correctly (issue 304's hard case). */
typedef struct {
    char   tag;
    double heavy;
    int    id;
} padded;

/* A struct exercising every field kind at once: primitive, nested
 * struct, fixed string, wide unsigned. */
typedef struct {
    int           a;
    vec3          pos;
    char          note[16];
    unsigned long stamp;
} record;

/* {{{ add() */
/* Two of the same type in, one out — the simplest possible box. */
int add(int a, int b)
{
    return a + b;
}
/* }}} */

/* {{{ mix() */
/* Two different types in: the box that catches size confusion. */
double mix(int count, double factor)
{
    return count * factor;
}
/* }}} */

/* {{{ make_vec3() */
/* Returns a struct by value. */
vec3 make_vec3(float x, float y, float z)
{
    vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}
/* }}} */

/* {{{ nudge() */
/* Takes a struct by value and returns one: both directions at once. */
vec3 nudge(vec3 v, float amount)
{
    v.x += amount;
    v.y += amount;
    v.z += amount;
    return v;
}
/* }}} */

/* {{{ magnitude_squared() */
/* Struct in, primitive out. Squared to stay exact in floats. */
float magnitude_squared(vec3 v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}
/* }}} */

/* {{{ stamp_record() */
/* Builds the every-kind struct, for byte-fidelity checks. */
record stamp_record(int a, vec3 pos, unsigned long stamp)
{
    record r;
    r.a = a;
    r.pos = pos;
    for (int i = 0; i < 16; i++)
        r.note[i] = 0;
    r.note[0] = 'o';
    r.note[1] = 'k';
    r.stamp = stamp;
    return r;
}
/* }}} */

/* {{{ swallow() */
/* Returns nothing: a sink. Delivery skips its tasks entirely. */
void swallow(int x)
{
    (void)x;
}
/* }}} */

/* {{{ seven() */
/* No inputs at all: every slot vacuously satisfied. A station placing
 * this can only ever run by being seeded — nothing can be written
 * into it to discover it. It could once also be reached by being
 * gathered from, which is gone (issue 210), so seeding is now the
 * only door. */
int seven(void)
{
    return 7;
}
/* }}} */

/* {{{ double_it() */
int double_it(int x)
{
    return x * 2;
}
/* }}} */

/* {{{ keep() */
/* Identity: the box a comparator places when the point is routing a
 * value unchanged rather than transforming it. */
int keep(int x)
{
    return x;
}
/* }}} */

/* {{{ slow_double() */
/* Doubling, expensively: a deliberate bottleneck with an input, so
 * the phase 7 demo has something to find with the statistics and
 * relieve by rewiring. Burns rather than sleeps, as always. */
int slow_double(int x)
{
    unsigned long v = 88172645463325252UL;
    for (int i = 0; i < 60000; i++) {
        v ^= v << 13;
        v ^= v >> 7;
        v ^= v << 17;
    }
    if (v == 0)
        abort();
    return x * 2;
}
/* }}} */

/* {{{ slow_seven() */
/*
 * Seven, expensively: an input-less box whose cost is measurable.
 * It existed to show what gathering charged the delivery path once
 * per task assembled; with the pull path gone (issue 210) it is a
 * costly source that runs once when seeded, which is what phase 4's
 * rewritten demo needs to show a chain recalculating. Burns
 * arithmetic rather than sleeping, because nothing in this engine is
 * allowed to block.
 */
int slow_seven(void)
{
    unsigned long x = 88172645463325252UL;
    for (int i = 0; i < 30000; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
    }
    /* The result must be genuinely consumed or the optimizer deletes
     * the entire loop and the cost this box exists to have. */
    if (x == 0)
        abort();
    return 7;
}
/* }}} */

/* {{{ read_int_file() */
/*
 * A read box, written as an ordinary function to prove no engine
 * support is required — the dedicated read box type from the original
 * vision dissolves into "an ordinary function that happens to read".
 * Opens, reads, closes: safe for two workers to be inside at the same
 * instant, which a kept-open seeking handle would not be.
 *
 * A box cannot decline (the task struct has a place waiting for bytes
 * and no way to say absence), so a missing file stops the program and
 * says so. That was true of a gatherer and stayed true without it:
 * absence is a thing the value has no room to express, which is what
 * optional parameters are for when issue 210 gets to them.
 */
int read_int_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "read_int_file: '%s' does not exist — a box's "
                        "value cannot be absent\n", path);
        abort();
    }
    int value = 0;
    if (fscanf(f, "%d", &value) != 1) {
        fprintf(stderr, "read_int_file: '%s' holds no number\n", path);
        fclose(f);
        abort();
    }
    fclose(f);
    return value;
}
/* }}} */

/* {{{ write_int_file() */
/*
 * A write box: an ordinary void function, no engine support — the
 * vision's dedicated write type stays dissolved, this time from the
 * writing side. Overwrites; a map wanting history wires a different
 * box.
 */
void write_int_file(const char *path, int value)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "write_int_file: cannot open '%s'\n", path);
        abort();
    }
    fprintf(f, "%d\n", value);
    fclose(f);
}
/* }}} */

/* {{{ vec3__compare() */
/*
 * The author-supplied three-way ordering for vec3, found by suffix.
 * Orders on squared magnitude — deliberately not on the first field,
 * so a test can tell this ran rather than a byte comparison.
 */
int vec3__compare(vec3 a, vec3 b)
{
    float ma = a.x * a.x + a.y * a.y + a.z * a.z;
    float mb = b.x * b.x + b.y * b.y + b.z * b.z;
    return (ma > mb) - (ma < mb);
}
/* }}} */
