#!/usr/bin/env bash
#
# 031-test-generator-cli.sh — proves the generator's conduct at the
# command line (issues 301, 306).
#
# What this is: the tests for how the generator behaves as a build
# citizen — errors that name file and line, braces inside strings
# that do not derail the parse, regeneration that follows an edit,
# and the guarantee that a failing run leaves no output at all.
#
# How it does it, in general terms: builds throwaway box sources in
# the RAM-backed scratch tier, runs the generator against them, and
# inspects exit codes, messages, and the presence or absence of the
# output file.

set -euo pipefail

DIR="/mnt/mtwo/programming/ai-playground/minimal-soramech"
if [[ $# -ge 1 && -d "$1" ]]; then
    DIR="$1"
fi

GENERATOR="${DIR}/scripts/028-generate.lua"
WORK="/tmp/$(basename "${DIR}")/generator-cli-test"
rm -rf "${WORK}"
mkdir -p "${WORK}"

fail() {
    echo "generator cli test failed: $1" >&2
    exit 1
}

# --- a healthy source, with a brace hiding inside a string ---------
cat > "${WORK}/good.c" <<'EOF'
typedef struct {
    int    a;
    double b;
} pair;

static const char *sneaky(void)
{
    return "{ not a real brace";
}

int widen(int x)
{
    const char *s = sneaky();
    return x + (s ? 1 : 0);
}

pair bundle(int a, double b)
{
    pair p;
    p.a = a;
    p.b = b;
    return p;
}
EOF

luajit "${GENERATOR}" "${WORK}/out.c" "${WORK}/good.c" \
    || fail "a healthy source was refused"
grep -q '"widen"' "${WORK}/out.c" || fail "widen missing from emission"
grep -q '"bundle"' "${WORK}/out.c" || fail "bundle missing from emission"
grep -q '"sneaky"' "${WORK}/out.c" && fail "a static helper leaked into the registry"
echo "  braces in strings, helpers, and multi-type boxes all handled"

# --- describe mode reports what was seen ---------------------------
DESCRIBED="$(luajit "${GENERATOR}" --describe "${WORK}/good.c")"
echo "${DESCRIBED}" | grep -q "box widen" || fail "describe mode lost a box"
echo "${DESCRIBED}" | grep -q "struct pair" || fail "describe mode lost a struct"
echo "  describe mode shows the parser's findings"

# --- a declaration spanning several lines --------------------------
cat > "${WORK}/multiline.c" <<'EOF'
long
stretch(
    int a,
    int b
)
{
    return (long)a + b;
}
EOF
luajit "${GENERATOR}" "${WORK}/out2.c" "${WORK}/multiline.c" \
    || fail "a multi-line declaration was refused"
grep -q '"stretch"' "${WORK}/out2.c" || fail "stretch missing"
echo "  declarations spanning lines parse"

# --- errors name the file and the line -----------------------------
cat > "${WORK}/bad.c" <<'EOF'
typedef struct {
    float x, y;
} sloppy;
EOF
set +e
MESSAGE="$(luajit "${GENERATOR}" "${WORK}/out3.c" "${WORK}/bad.c" 2>&1)"
STATUS=$?
set -e
[[ ${STATUS} -ne 0 ]] || fail "comma fields were accepted"
echo "${MESSAGE}" | grep -q "bad.c:" || fail "the error names no file"
echo "  a malformed source stops the build, naming the file"

# --- a failing run leaves nothing behind ---------------------------
[[ ! -f "${WORK}/out3.c" ]] || fail "a failing run left partial output in place"
echo "  failure emits nothing — no stale registry possible"

# --- regeneration follows an edit ----------------------------------
cat >> "${WORK}/good.c" <<'EOF'

int follow_up(int x)
{
    return x * 2;
}
EOF
luajit "${GENERATOR}" "${WORK}/out.c" "${WORK}/good.c" \
    || fail "regeneration after an edit failed"
grep -q '"follow_up"' "${WORK}/out.c" || fail "the edit did not reach the registry"
echo "  an edited source regenerates into an updated registry"

rm -rf "${WORK}"
