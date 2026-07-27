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
