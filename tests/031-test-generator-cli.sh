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

GENERATOR="${DIR}/tmp/build/generate"
if [[ ! -x "${GENERATOR}" ]]; then
    echo "generator cli test: ${GENERATOR} is not built; run make first" >&2
    exit 1
fi
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

"${GENERATOR}" "${WORK}/out.c" "${WORK}/good.c" \
    || fail "a healthy source was refused"
grep -q '"widen"' "${WORK}/out.c" || fail "widen missing from emission"
grep -q '"bundle"' "${WORK}/out.c" || fail "bundle missing from emission"
grep -q '"sneaky"' "${WORK}/out.c" && fail "a static helper leaked into the emitted file"
echo "  braces in strings, helpers, and multi-type boxes all handled"

# --- describe mode reports what was seen ---------------------------
DESCRIBED="$("${GENERATOR}" --describe "${WORK}/good.c")"
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
"${GENERATOR}" "${WORK}/out2.c" "${WORK}/multiline.c" \
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
MESSAGE="$("${GENERATOR}" "${WORK}/out3.c" "${WORK}/bad.c" 2>&1)"
STATUS=$?
set -e
[[ ${STATUS} -ne 0 ]] || fail "comma fields were accepted"
echo "${MESSAGE}" | grep -q "bad.c:" || fail "the error names no file"
echo "  a malformed source stops the build, naming the file"

# --- a failing run leaves nothing behind ---------------------------
[[ ! -f "${WORK}/out3.c" ]] || fail "a failing run left partial output in place"
echo "  failure emits nothing — no stale emission possible"

# --- regeneration follows an edit ----------------------------------
cat >> "${WORK}/good.c" <<'EOF'

int follow_up(int x)
{
    return x * 2;
}
EOF
"${GENERATOR}" "${WORK}/out.c" "${WORK}/good.c" \
    || fail "regeneration after an edit failed"
grep -q '"follow_up"' "${WORK}/out.c" || fail "the edit did not reach the emitted file"
echo "  an edited source regenerates into an updated emission"


# --- an ambiguous box name refuses, naming both paths --------------
#
# Two sources in different directories, each defining a function of
# the same name, and a map asking for it by that bare name.
# Resolution refuses rather than picking one, and names both
# candidates — because the author's fix is to write one of them out in
# full and they cannot do that without being told which two files are
# in question (issue 311a).
mkdir -p "${WORK}/left" "${WORK}/right"
cat > "${WORK}/left/twins.c" <<'EOF'
int twin(int x)
{
    return x + 1;
}
EOF
cat > "${WORK}/right/twins.c" <<'EOF'
int twin(int x)
{
    return x + 2;
}
EOF
cat > "${WORK}/ambiguous.map" <<'EOF'
station only twin p result
EOF

set +e
MESSAGE="$("${GENERATOR}" "${WORK}/out4.c" "--map=${WORK}/ambiguous.map" \
           "${WORK}/left/twins.c" "${WORK}/right/twins.c" 2>&1)"
STATUS=$?
set -e
[[ ${STATUS} -ne 0 ]] || fail "an ambiguous box name was accepted"
echo "${MESSAGE}" | grep -q "more than" || fail "the refusal does not say it is ambiguous"
echo "${MESSAGE}" | grep -q "left/twins.c:twin" || fail "the refusal omits the first path"
echo "${MESSAGE}" | grep -q "right/twins.c:twin" || fail "the refusal omits the second path"
[[ ! -f "${WORK}/out4.c" ]] || fail "a refused map left output in place"
echo "  an ambiguous box name refuses and names both paths"

# --- and writing one out in full settles it ------------------------
#
# The path is not a fallback; it is a more specific way of saying the
# same thing. The same two sources and the same shape of map, differing
# only in how the line addresses the box, builds.
cat > "${WORK}/settled.map" <<'EOF'
station only right/twins.c:twin p result
EOF
"${GENERATOR}" "${WORK}/out5.c" "--map=${WORK}/settled.map" \
    "${WORK}/left/twins.c" "${WORK}/right/twins.c" \
    || fail "a path did not settle the tie"
grep -q "right_sl_twins_dot_c__twin__place" "${WORK}/out5.c" \
    || fail "the settled map did not call the right placement function"
echo "  and writing one path out in full settles it"

rm -rf "${WORK}"
