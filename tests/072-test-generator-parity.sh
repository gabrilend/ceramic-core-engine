#!/usr/bin/env bash
#
# 072-test-generator-parity.sh — proves the C generator emits what the
# Lua one did (issue 308, step 5).
#
# What this is: the bridge test for a translation. The generator was a
# LuaJIT script and is now a C program, and the only claim worth
# making about a translation is that it produces the same answer. This
# runs both over the same sources and compares, byte for byte.
#
# How it does it, in general terms: builds a spread of box sources in
# the RAM scratch tier — every shape the parser accepts, and every
# shape it refuses — runs both generators over each, and diffs both
# the emitted C and the refusal messages. The first line of the
# emitted file names the generator that wrote it and is expected to
# differ; everything after it is expected not to.
#
# THIS TEST IS DELIBERATELY TEMPORARY. It cannot outlive the Lua
# generator it compares against, and it goes when that does. If you
# are reading this after the Lua generator is gone, delete it.

set -euo pipefail

DIR="/mnt/mtwo/programming/ai-playground/minimal-soramech"
if [[ $# -ge 1 && -d "$1" ]]; then
    DIR="$1"
fi

CGEN="${DIR}/tmp/build/generate"
LGEN="${DIR}/scripts/028-generate.lua-done"

if [[ ! -x "${CGEN}" ]]; then
    echo "parity test: ${CGEN} is not built; run make first" >&2
    exit 1
fi
if [[ ! -f "${LGEN}" ]]; then
    echo "  the Lua generator is gone; parity has nothing to compare against"
    echo "  (this test should be deleted with it — issue 308, step 7)"
    exit 0
fi
if ! command -v luajit >/dev/null 2>&1; then
    echo "  luajit absent; parity cannot be checked here"
    echo "  (which is the point of the change this test guards — issue 308)"
    exit 0
fi

WORK="/tmp/$(basename "${DIR}")/generator-parity-test"
rm -rf "${WORK}"
mkdir -p "${WORK}"

fail() {
    echo "generator parity failed: $1" >&2
    exit 1
}

# --- sources the parser accepts ------------------------------------
cat > "${WORK}/accepted.c" <<'EOF'
typedef struct { float x; float y; float z; } vec3;
typedef struct { int a; double b; } pair;
typedef struct { char name[16]; int n; vec3 where; } record;

static const char *helper(void)
{
    return "{ a brace in a string }";
}

int vec3__compare(vec3 a, vec3 b)
{
    if (a.x != b.x) return a.x < b.x ? -1 : 1;
    return 0;
}

int widen(int x)
{
    const char *s = helper();
    return x + (s ? 1 : 0);
}

double scale(double v, double by) { return v * by; }

pair bundle(int a, double b) { pair p; p.a = a; p.b = b; return p; }

vec3 origin(void) { vec3 v; v.x = v.y = v.z = 0; return v; }

record label(const char *text, int n)
{
    record r; r.n = n; r.name[0] = text ? text[0] : 0;
    r.where.x = r.where.y = r.where.z = 0;
    return r;
}

void sink(pair p) { (void)p; }

long
stretch(
    int a,
    int b
)
{
    return (long)a + b;
}
EOF

"${CGEN}" "${WORK}/c-out.c" "${WORK}/accepted.c" \
    || fail "the C generator refused a healthy source"
luajit "${LGEN}" "${WORK}/lua-out.c" "${WORK}/accepted.c" \
    || fail "the Lua generator refused a healthy source"

# The first line names which generator wrote the file, and is meant
# to differ. Everything after it is the actual claim.
if ! diff -q <(tail -n +2 "${WORK}/lua-out.c") \
             <(tail -n +2 "${WORK}/c-out.c") >/dev/null; then
    diff <(tail -n +2 "${WORK}/lua-out.c") <(tail -n +2 "${WORK}/c-out.c") | head -40
    fail "emitted registries differ"
fi
LINES=$(wc -l < "${WORK}/c-out.c")
echo "  ${LINES} lines of registry, identical from both generators"

# --- describe mode --------------------------------------------------
if ! diff -q <("${CGEN}" --describe "${WORK}/accepted.c") \
             <(luajit "${LGEN}" --describe "${WORK}/accepted.c") >/dev/null; then
    fail "describe output differs"
fi
echo "  describe mode reports the same findings"

# --- sources the parser refuses -------------------------------------
# Each of these is a shape the generator must stop on, and the message
# is what a person actually reads, so the messages are compared too.
mk() { printf '%s\n' "$2" > "${WORK}/$1.c"; }

mk comma      'typedef struct { float x, y; } sloppy;'
mk plain      'struct plain { int a; };'
mk nested     'typedef struct { struct { int a; } inner; } outer;'
mk unknown    'typedef struct { int a; } thing;
int f(thing t, unknown_t u) { return t.a; }'
mk unnamed    'int f(int) { return 0; }'
mk dupstruct  'typedef struct { int a; } thing;
typedef struct { int b; } thing;'
mk retstring  'const char *f(int x) { (void)x; return "hi"; }'
mk ptrfield   'typedef struct { int *p; } bad;'
mk dupbox     'int dup(int x) { return x; }
int dup(int y) { return y; }'
mk badcompare 'typedef struct { int a; } v;
int v__compare(int a, int b) { return a - b; }'
mk unbalanced 'int f(int x) { return x;'
mk noname     'typedef struct { int a; }'
mk arraytype  'typedef struct { int nums[4]; } arr;'

for t in comma plain nested unknown unnamed dupstruct retstring ptrfield \
         dupbox badcompare unbalanced noname arraytype; do
    set +e
    CMSG="$("${CGEN}" "${WORK}/c-r.c" "${WORK}/${t}.c" 2>&1)"
    CRC=$?
    LMSG="$(luajit "${LGEN}" "${WORK}/lua-r.c" "${WORK}/${t}.c" 2>&1)"
    LRC=$?
    set -e
    [[ ${CRC} -ne 0 ]] || fail "${t}: the C generator accepted what it must refuse"
    [[ ${LRC} -ne 0 ]] || fail "${t}: the Lua generator accepted it, so this case is wrong"
    [[ "${CMSG}" == "${LMSG}" ]] \
        || fail "${t}: messages differ
  lua: ${LMSG}
  c:   ${CMSG}"
    [[ ! -f "${WORK}/c-r.c" ]] || fail "${t}: a failing run left output behind"
done
echo "  13 refusals, each with the same message from both"

rm -rf "${WORK}"
