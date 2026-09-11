#!/usr/bin/env bash
# 116-test-outside-the-tree.sh — somebody else builds a program with this engine.
#
# What this proves: the engine can be taken away. A scratch directory
# that has no relationship to this repository gets a copy of cera.c,
# cera.h and the generator, plus a box source and a map written from
# scratch, and the three steps a consumer's build runs — generate,
# compile, link — produce a program that runs and prints the right
# answer.
#
# Why this is the capstone of phase nine. Every other claim in the phase
# is inference from reading the code: that the header is sufficient,
# that nothing reaches back into this tree, that the two linker settings
# are written down somewhere findable. Packaging that has not been
# compiled somewhere else is a guess.
#
# What each failure would mean:
#   - a missing declaration       -> the header is not sufficient
#   - a file not found            -> the engine reaches back into its tree
#   - a link error naming a shim  -> the export list is wrong or missing
#   - a wrong answer              -> the engine does not work when vendored
#
# Copies, never symlinks: a forgotten include path has to fail rather
# than silently resolve back here.
#
# The scratch tree goes in the executable RAM tier under /tmp rather
# than the artifact tier under /dev/shm, because this test compiles
# something and then runs it, and /dev/shm is mounted without execute
# permission. That is the whole reason the project keeps two tiers.
#
# Usage: 116-test-outside-the-tree.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
CC="${CC:-gcc}"
AWAY="/tmp/$(basename "${DIR}")/outside"

rm -rf "${AWAY}"
mkdir -p "${AWAY}/engine" "${AWAY}/generator" "${AWAY}/boxes" "${AWAY}/build"

fail() { echo "  $*"; exit 1; }

# --- what a consumer takes: two files, plus the generator ------------
cp "${DIR}/src/cera.c" "${DIR}/src/cera.h" "${AWAY}/engine/" || fail "no engine to copy"
cp "${DIR}/src/098-engine-surface.syms" "${AWAY}/engine/"    || fail "no export list"
cp "${DIR}"/scripts/*.c "${DIR}"/scripts/*.h "${AWAY}/generator/" 2>/dev/null
[[ -f "${AWAY}/generator/070-generate.c" ]] || fail "the generator did not come across"

# --- what a consumer writes: one box, one map ------------------------
cat > "${AWAY}/boxes/arithmetic.c" <<'BOX'
/* Two boxes. Ordinary C functions: arguments by value, one return each,
 * nothing remembered between calls.
 *
 * Both belong to this imaginary consumer. The map below names these and
 * nothing else, which matters: reaching for one of the engine
 * repository's own demo boxes would make this test pass for a reason
 * that has nothing to do with whether the engine can be taken away. */
int carry(int v)
{
    return v;
}

int add_up(int a, int b)
{
    return a + b;
}
BOX

cat > "${AWAY}/away.map" <<'MAP'
station feed (arithmetic.c:carry)
  in 0 - 0$
  out 0 - total.0

station total (arithmetic.c:add_up)
  out 0 - 0$
  in 0 - feed.0
  in 1 = 30
MAP

# --- and the program that runs it ------------------------------------
cat > "${AWAY}/away.c" <<'PROG'
#include "cera.h"

#include <stdio.h>

int main(void)
{
    const cera_map_build_t *program = cera_map_build_find("away.map");
    if (!program) {
        fprintf(stderr, "this binary was not built with away.map\n");
        return 1;
    }

    cera_map_t *m = cera_map_create_empty();
    program->build(m, NULL, 0);
    cera_map_start(m, 2);

    const char *refused = cera_map_bring_up(m);
    if (refused) {
        fprintf(stderr, "refused: %s\n", refused);
        return 1;
    }

    const int value = 12;
    const char *no = cera_map_deliver_argument(m, 0, 0, &value, (int)sizeof value);
    if (no) {
        fprintf(stderr, "could not feed it: %s\n", no);
        return 1;
    }

    int answers[4] = { 0 };
    if (cera_map_collect(m, 1, 0, answers, 4, (int)sizeof answers[0])) {
        fprintf(stderr, "nowhere to put the answer\n");
        return 1;
    }

    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    if (cera_map_collected(m, 1, 0) < 1) {
        fprintf(stderr, "nothing came back\n");
        return 1;
    }
    printf("%d\n", answers[0]);
    cera_map_destroy(m);
    return 0;
}
PROG

# --- step one: build the generator, which needs nothing from the engine
#     but the one header -------------------------------------------------
#
# Two programs live in scripts/ and each has a main of its own: the
# build-time generator, and the compiler that ships (issue 910). The
# generator is built from everything there except serac's front door,
# the way the project's own Makefile builds it, because two mains in one
# link is an error that names neither of them usefully.
GEN_SRC=()
for f in "${AWAY}"/generator/*.c; do
    [[ "$(basename "${f}")" == "144-serac.c" ]] && continue
    GEN_SRC+=("${f}")
done

"${CC}" -std=gnu11 -O2 -I"${AWAY}/generator" -I"${AWAY}/engine" \
    -o "${AWAY}/build/generate" "${GEN_SRC[@]}" \
    || fail "the generator did not compile away from home"

# --- step two: generate, over the consumer's own box and map ----------
"${AWAY}/build/generate" "${AWAY}/build/emitted.c" \
    --root="${AWAY}" --map="${AWAY}/away.map" "${AWAY}/boxes/arithmetic.c" \
    || fail "the generator refused a box and a map it should accept"

[[ -s "${AWAY}/build/emitted.c" ]] || fail "the generator wrote nothing"

# --- step three: compile and link ------------------------------------
#
# The two linker settings are not optional and are the easiest thing for
# a consumer to omit: without the export list a program builds and then
# fails at run time when a box arrives, which looks like success.
"${CC}" -std=gnu11 -O2 -pthread -I"${AWAY}/engine" \
    -ffunction-sections -fdata-sections \
    -DCERA_COMPILER='"'"${AWAY}/build/serac"'"' \
    -DCERA_ROOT='"'"${AWAY}"'"' \
    -DCERA_RAM_SHARED='"/dev/shm"' -DCERA_RAM_EXEC='"/tmp"' \
    -o "${AWAY}/build/away" \
    "${AWAY}/away.c" "${AWAY}/engine/cera.c" "${AWAY}/build/emitted.c" \
    -Wl,--dynamic-list="${AWAY}/engine/098-engine-surface.syms" \
    -Wl,--gc-sections \
    || fail "a program using the engine did not link away from home"

# --- and it has to be right, not merely built ------------------------
got="$("${AWAY}/build/away" 2>"${AWAY}/build/stderr.txt")"
status=$?
[[ ${status} -eq 0 ]] || fail "the program exited ${status}: $(cat "${AWAY}/build/stderr.txt")"
[[ "${got}" == "42" ]] || fail "the program printed '${got}', not 42"

echo "  a program was built with this engine in a directory that cannot see this one"
echo "  and 12 fed into a box adding 30 came back as ${got}"

# --- the engine must not have reached home ---------------------------
if grep -rq -- "${DIR}" "${AWAY}/build/emitted.c"; then
    fail "the emitted file names this repository; a consumer's build would not relocate"
fi
echo "  and nothing it generated names the repository it came from"

# --- the same work, in one command ------------------------------------
#
# Everything above is what a consumer used to assemble: four things
# travelling, three compiler invocations, two linker settings that are
# easy to omit, and a main nobody wanted to write. Below is issue 910's
# claim that all of it collapses into one program handed a description
# and some C.
#
# serac is built here rather than copied, for the same reason the
# generator is: a consumer needs a C compiler and nothing else, and a
# prebuilt binary is a platform, a libc version and a trust decision.
# The two stages are the ones the project's own Makefile runs — compile
# the generator, use it to write the engine out as C string literals,
# compile serac from the generator's sources plus that file.
"${AWAY}/build/generate" --embed "${AWAY}/build/145-embedded-engine.c" \
    "${AWAY}/engine/cera.h" "${AWAY}/engine/cera.c" \
    "${AWAY}/engine/098-engine-surface.syms" \
    || fail "the generator could not write the engine out as text"

SERAC_SRC=("${AWAY}/generator/144-serac.c" "${AWAY}/build/145-embedded-engine.c")
for f in "${AWAY}"/generator/*.c; do
    base="$(basename "${f}")"
    [[ "${base}" == "144-serac.c"   ]] && continue
    [[ "${base}" == "070-generate.c" ]] && continue
    SERAC_SRC+=("${f}")
done

"${CC}" -std=gnu11 -O2 -I"${AWAY}/generator" -I"${AWAY}/engine" \
    -DSERAC_CC="\"${CC}\"" \
    -o "${AWAY}/build/serac" "${SERAC_SRC[@]}" \
    || fail "serac did not compile away from home"

# What it carries has to be what went in, or everything built with it is
# built against something nobody wrote.
rm -rf "${AWAY}/unpacked"
"${AWAY}/build/serac" --unpack "${AWAY}/unpacked" >/dev/null \
    || fail "serac could not write the engine back out"
for f in cera.c cera.h 098-engine-surface.syms; do
    cmp -s "${AWAY}/engine/${f}" "${AWAY}/unpacked/${f}" \
        || fail "the ${f} serac carries is not the one it was built from"
done
echo "  the engine serac carries comes back out byte for byte"

# One command, and nothing in the directory but a description and a box.
mkdir -p "${AWAY}/onecommand"
cp "${AWAY}/away.map" "${AWAY}/onecommand/oneshot.map"
cp "${AWAY}/boxes/arithmetic.c" "${AWAY}/onecommand/arithmetic.c"

"${AWAY}/build/serac" "${AWAY}/onecommand/oneshot.map" \
    "${AWAY}/onecommand/arithmetic.c" >/dev/null \
    || fail "serac refused a description and a box it should accept"

# Beside the description, named after it — not wherever the shell was.
[[ -x "${AWAY}/onecommand/oneshot" ]] \
    || fail "serac did not put the program beside the description that made it"

got="$("${AWAY}/onecommand/oneshot" 12 2>"${AWAY}/build/stderr2.txt")"
status=$?
[[ ${status} -eq 0 ]] || fail "the one-command program exited ${status}: $(cat "${AWAY}/build/stderr2.txt")"
[[ "${got}" == "42" ]] || fail "the one-command program printed '${got}', not 42"
echo "  and one command over the same description and box printed ${got}"

# The count is the program's own, and getting it wrong says so.
if "${AWAY}/onecommand/oneshot" >/dev/null 2>"${AWAY}/build/stderr3.txt"; then
    fail "a program given no arguments when it wants one exited zero"
fi
grep -q "takes 1 argument" "${AWAY}/build/stderr3.txt" \
    || fail "the refusal did not say how many arguments were wanted: $(cat "${AWAY}/build/stderr3.txt")"
echo "  and asking for nothing is refused saying how many it wanted"

rm -rf "${AWAY}"
