#!/usr/bin/env bash
# 146-test-the-compiler.sh — what serac does, checked one claim at a time.
#
# What this proves: that a description and some C functions are a whole
# program. The out-of-tree test proves serac can be built and used
# somewhere that cannot see this repository; this one stays home and
# asks what it actually produces — every shape of result, a program with
# no results at all, the three modes composing, and each refusal saying
# the thing it is supposed to say.
#
# Why the two are separate. That one is about packaging and would pass
# with a serac that built nonsense, as long as the nonsense compiled.
# This one is about the nonsense.
#
# Everything happens in the executable RAM tier under /tmp, because it
# compiles things and then runs them, and /dev/shm is mounted without
# execute permission. That is the whole reason the project keeps two
# tiers.
#
# Usage: 146-test-the-compiler.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
SERAC="${DIR}/tmp/build/serac"
WORK="/tmp/$(basename "${DIR}")/compiler-test"

fail() { echo "  FAIL: $*"; exit 1; }

[[ -x "${SERAC}" ]] || fail "there is no serac at ${SERAC}"

rm -rf "${WORK}"
mkdir -p "${WORK}"

# --- every shape a result can be -------------------------------------
#
# A result is printed through the return type of the box at the marked
# station, and there are four kinds that can reach a wire: signed,
# unsigned, floating, and a struct. A borrowed pointer is not among them
# and cannot be — the parser refuses a box that returns one, because the
# bytes live wherever the pointer points and a value on a wire has no
# owner to keep them alive.
cat > "${WORK}/shapes.c" <<'BOX'
typedef struct {
    int    x;
    double y;
} spot;

int twice(int v)
{
    return v * 2;
}

double halve(int v)
{
    return v / 2.0;
}

unsigned int size_of(int v)
{
    return v < 0 ? (unsigned int)(-v) : (unsigned int)v;
}

spot place(int v)
{
    spot s;
    s.x = v;
    s.y = v * 1.5;
    return s;
}
BOX

cat > "${WORK}/shapes.map" <<'MAP'
station doubler (shapes.c:twice)
  in 0 - 0$
  out 0 - 0$

station halver (shapes.c:halve)
  in 0 - 1$
  out 0 - 1$

station sizer (shapes.c:size_of)
  in 0 - 2$
  out 0 - 2$

station placer (shapes.c:place)
  in 0 - 3$
  out 0 - 3$
MAP

"${SERAC}" "${WORK}/shapes.map" "${WORK}/shapes.c" >/dev/null \
    || fail "serac refused a description it should accept"

# Beside the description and named after it, rather than wherever the
# shell happened to be standing.
[[ -x "${WORK}/shapes" ]] || fail "the program did not land beside its description"

got="$("${WORK}/shapes" 7 9 -3 4)"
want=$'14\n4.5\n3\n{ 4, 6 }'
[[ "${got}" == "${want}" ]] || fail "four results printed as '${got}', not '${want}'"
echo "  a signed, an unsigned, a float and a struct each printed in port order"

# --- and the struct that came out goes back in -----------------------
#
# Results print through the same writers that put a constant into a map
# file, which is the claim being checked here rather than merely stated:
# the brace form one program prints is the brace form the next reads.
cat > "${WORK}/nudge.c" <<'BOX'
typedef struct {
    int    x;
    double y;
} spot;

spot nudge(spot s)
{
    s.x += 1;
    s.y += 1.0;
    return s;
}
BOX

cat > "${WORK}/nudge.map" <<'MAP'
station mover (029-demo-boxes.c:nudge)
  in 0 - 0$
  out 0 - 0$
MAP

"${SERAC}" "${WORK}/nudge.map" "${WORK}/nudge.c" >/dev/null \
    || fail "serac refused a description whose argument is a struct"

got="$("${WORK}/nudge" "{ 4, 6 }")"
[[ "${got}" == "{ 5, 7 }" ]] || fail "a struct argument came back as '${got}'"
echo "  and a struct printed by one program is a struct argument to the next"

# --- a description with no results is a whole program ----------------
cat > "${WORK}/quiet.c" <<'BOX'
#include <stdio.h>

int shout(int v)
{
    printf("the box said %d\n", v);
    return v;
}
BOX

cat > "${WORK}/quiet.map" <<'MAP'
station voice (quiet.c:shout)
  in 0 - 0$
MAP

"${SERAC}" "${WORK}/quiet.map" "${WORK}/quiet.c" >/dev/null \
    || fail "serac refused a description that marks no results"

got="$("${WORK}/quiet" 5)"
status=$?
[[ ${status} -eq 0 ]] || fail "a program with no results exited ${status}"
[[ "${got}" == "the box said 5" ]] \
    || fail "a program with no results printed '${got}'"
echo "  a description marking no results prints nothing of its own and exits zero"

# --- the three modes compose -----------------------------------------
#
# --emit-c writes what would have been compiled minus the engine, and
# --unpack writes the engine. Compiling the one against the other by
# hand has to reproduce what a single serac does, because that is the
# only way a person can check the claim that it does anything ordinary.
# Named explicitly, because --emit-c over shapes.map would otherwise
# want to write shapes.c — which is the box source sitting beside it.
# That collision is refused rather than allowed to destroy the input,
# and the refusal is checked below.
rm -rf "${WORK}/engine"
"${SERAC}" --unpack "${WORK}/engine" >/dev/null || fail "serac could not unpack the engine"
for f in cera.c cera.h 098-engine-surface.syms; do
    cmp -s "${DIR}/src/${f}" "${WORK}/engine/${f}" \
        || fail "the ${f} serac carries differs from the one in src/"
done

"${SERAC}" --emit-c -o "${WORK}/built.c" "${WORK}/shapes.map" "${WORK}/shapes.c" \
    >/dev/null || fail "serac refused to emit the C to a named path"
[[ -s "${WORK}/built.c" ]] || fail "--emit-c wrote nothing"

"${CC:-cc}" -std=gnu11 -O2 -pthread -I"${WORK}/engine" \
    -ffunction-sections -fdata-sections \
    -o "${WORK}/byhand" "${WORK}/built.c" "${WORK}/engine/cera.c" \
    -Wl,--dynamic-list="${WORK}/engine/098-engine-surface.syms" \
    -Wl,--gc-sections \
    || fail "the emitted C did not compile against the unpacked engine"

got="$("${WORK}/byhand" 7 9 -3 4)"
[[ "${got}" == "${want}" ]] \
    || fail "built by hand it printed '${got}', not what one command printed"
echo "  --emit-c and --unpack compiled by hand reproduce what one command does"

# --- what it refuses -------------------------------------------------
if "${WORK}/shapes" 7 >/dev/null 2>"${WORK}/said.txt"; then
    fail "a program given one argument when it wants four exited zero"
fi
grep -q "takes 4 arguments and was given 1" "${WORK}/said.txt" \
    || fail "the refusal did not say how many were wanted: $(cat "${WORK}/said.txt")"

# A description marking a result on a box that returns nothing has no
# value to print, and the refusal names the box rather than leaving it
# to the C compiler to complain about a void.
cat > "${WORK}/void.c" <<'BOX'
void swallow(int v)
{
    (void)v;
}
BOX
cat > "${WORK}/void.map" <<'MAP'
station sink (void.c:swallow)
  in 0 - 0$
  out 0 - 0$
MAP
if "${SERAC}" "${WORK}/void.map" "${WORK}/void.c" >/dev/null 2>"${WORK}/void.txt"; then
    fail "a result marked on a box returning nothing was accepted"
fi

# Two descriptions is not a program, and the refusal names both.
if "${SERAC}" "${WORK}/shapes.map" "${WORK}/quiet.map" "${WORK}/shapes.c" \
        >/dev/null 2>"${WORK}/two.txt"; then
    fail "two descriptions on one command line were accepted"
fi
grep -q "from one" "${WORK}/two.txt" \
    || fail "the refusal for two descriptions did not explain: $(cat "${WORK}/two.txt")"

# And the collision the mode above sidesteps with -o: without it,
# --emit-c over shapes.map wants to write the box source it was asked to
# read. Destroying an input and then failing to compile it would be a
# message about a brace on a line nobody wrote.
if "${SERAC}" --emit-c "${WORK}/shapes.map" "${WORK}/shapes.c" \
        >/dev/null 2>"${WORK}/over.txt"; then
    fail "serac wrote its output over a source it was asked to read"
fi
grep -q "write over" "${WORK}/over.txt" \
    || fail "the refusal did not say what it would have overwritten: $(cat "${WORK}/over.txt")"
grep -q "spot place" "${WORK}/shapes.c" \
    || fail "the box source was damaged by a run that should have refused"

echo "  a wrong argument count, a void result, two descriptions and an"
echo "  output landing on an input are each refused"

# --- a description says where to look --------------------------------
#
# Every path in a description is relative to the description, and a block
# before the stations gives short names to places (issue 610). Both kinds
# of shortcut are exercised here, and so is the thing they are for: no C
# file is named on the command line at all, because the description
# already said which files it uses.
mkdir -p "${WORK}/away/libs/math" "${WORK}/away/libs"

cat > "${WORK}/away/libs/math/arithmetic.c" <<'BOX'
int add(int a, int b)
{
    return a + b;
}
BOX

cat > "${WORK}/away/libs/curves.c" <<'BOX'
int rotate(int v)
{
    return v * 90;
}
BOX

cat > "${WORK}/away/turn.map" <<'MAP'
math   = libs/math/
curves = libs/curves.c

station turn (curves:rotate)
  in 0 - 0$
  out 0 - sum.0

station sum (math/arithmetic.c:add)
  in 0 - turn.0
  in 1 = 7
  out 0 - 0$
MAP

"${SERAC}" "${WORK}/away/turn.map" >/dev/null \
    || fail "serac refused a description that says where to look"
got="$("${WORK}/away/turn" 2)"
[[ "${got}" == "187" ]] \
    || fail "a directory shortcut and a file shortcut gave '${got}', not 187"
echo "  a directory shortcut and a file shortcut both resolve, and no C"
echo "  file was named on the command line"

# The same relative path in two places is two different files, which is
# what "relative to the description" has to mean to be worth anything.
mkdir -p "${WORK}/one" "${WORK}/two"
for n in one two; do
    cat > "${WORK}/${n}/lib.c" <<BOX
int answer(int v)
{
    return v + $([[ ${n} == one ]] && echo 1 || echo 2);
}
BOX
    cat > "${WORK}/${n}/p.map" <<'MAP'
station only (lib.c:answer)
  in 0 - 0$
  out 0 - 0$
MAP
    "${SERAC}" "${WORK}/${n}/p.map" >/dev/null \
        || fail "serac refused ${n}/p.map"
done
a="$("${WORK}/one/p" 10)"
b="$("${WORK}/two/p" 10)"
[[ "${a}" == "11" && "${b}" == "12" ]] \
    || fail "one relative path in two directories gave '${a}' and '${b}'"
echo "  and the same relative path in two directories is two different files"

# What it refuses about shortcuts.
cat > "${WORK}/away/twice.map" <<'MAP'
math = libs/math/
math = libs/curves.c

station sum (math/arithmetic.c:add)
  in 0 - 0$
  out 0 - 0$
MAP
if "${SERAC}" "${WORK}/away/twice.map" >/dev/null 2>"${WORK}/twice.txt"; then
    fail "a shortcut declared twice was accepted"
fi
grep -q "already says where to look" "${WORK}/twice.txt" \
    || fail "the refusal did not say it was a duplicate: $(cat "${WORK}/twice.txt")"

cat > "${WORK}/away/late.map" <<'MAP'
station sum (math/arithmetic.c:add)
  in 0 - 0$
  out 0 - 0$

math = libs/math/
MAP
if "${SERAC}" "${WORK}/away/late.map" >/dev/null 2>"${WORK}/late.txt"; then
    fail "a shortcut declared after the stations was accepted"
fi
grep -q "before the stations" "${WORK}/late.txt" \
    || fail "the refusal did not say where the block goes: $(cat "${WORK}/late.txt")"

# And a path that resolves to nothing says what it resolved to, which is
# the sentence somebody needs to see what they got wrong.
cat > "${WORK}/away/missing.map" <<'MAP'
station sum (nowhere/arithmetic.c:add)
  in 0 - 0$
  out 0 - 0$
MAP
if "${SERAC}" "${WORK}/away/missing.map" >/dev/null 2>"${WORK}/missing.txt"; then
    fail "a description naming a file that is not there was accepted"
fi
grep -q "nowhere/arithmetic.c" "${WORK}/missing.txt" \
    || fail "the refusal did not name the path: $(cat "${WORK}/missing.txt")"
echo "  a shortcut declared twice, declared late, or resolving to nothing"
echo "  is refused, and the last one says what it resolved to"

# --- and it relocates ------------------------------------------------
#
# The defect this whole thing exists to remove: a program that could
# only bring in new code on the machine that built it. serac bakes in no
# path, so a program it built looks for serac beside itself — and moving
# the pair somewhere else has to change nothing.
mkdir -p "${WORK}/elsewhere"
cp "${WORK}/shapes" "${WORK}/elsewhere/"
got="$("${WORK}/elsewhere/shapes" 7 9 -3 4)"
[[ "${got}" == "${want}" ]] \
    || fail "moved to another directory the program printed '${got}'"
echo "  and the program still runs from a directory it was not built in"

rm -rf "${WORK}"
