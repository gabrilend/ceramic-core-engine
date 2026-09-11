#!/usr/bin/env bash
# 147-build-serac.sh — builds the compiler, from a release or from the tree.
#
# What this is: the script a person runs when all they have is a
# directory of source files and they want `serac`. The project's own
# Makefile knows how to do this, but a release bundle does not carry the
# Makefile — it carries fourteen files and this, which is the whole of
# what somebody needs.
#
# How it does it, in general terms: three stages, because the compiler
# contains a copy of the engine and something has to put it there.
#
#   one    compile the generator, which is ordinary C depending on
#          nothing
#   two    run it over the engine's three files, turning them into C
#          string literals
#   three  compile serac from the generator's own sources plus that
#
# There is no bootstrap problem. The program doing the embedding does
# not itself need to have been embedded, so stage one builds from
# nothing and stage three builds from stage two's output.
#
# It works in either shape of directory: a release, where every file
# sits together, or this repository, where the generator's sources are
# in scripts/ and the engine is in src/. Both are found rather than
# configured, because a person who just unpacked a tarball should not
# have to say which kind of directory they are standing in.
#
# Usage:
#   147-build-serac.sh [DIR] [-o OUTPUT]
#
#   DIR         where the sources are; defaults to this script's own
#               directory's parent, which is right in both shapes
#   -o OUTPUT   where serac lands; defaults to DIR/serac
#
# Environment:
#   CC          the C compiler, default cc. **This is the compiler serac
#               will invoke for everything it ever builds**, baked in,
#               because whatever compiles code added to a program has to
#               agree with it about sizeof.
set -u

# The project directory, hard-coded here and overridable as the first
# argument, so the script runs from anywhere.
DIR="/mnt/mtwo/programming/ai-playground/minimal-soramech"
OUT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o) OUT="${2:-}"; shift 2 ;;
        -h|--help) sed -n '2,40p' "$0"; exit 0 ;;
        *)  DIR="$1"; shift ;;
    esac
done

CC="${CC:-cc}"

fail() { echo "build-serac: $*" >&2; exit 1; }

# --- where the sources are --------------------------------------------
#
# Two shapes, told apart by looking rather than by being told. A release
# has everything in one directory. This repository has the generator's
# sources under scripts/ and the engine under src/. Nothing else is a
# shape this knows, and saying so beats compiling half of something.
if [[ -f "${DIR}/144-serac.c" ]]; then
    CODE="${DIR}"
    ENGINE="${DIR}"
elif [[ -f "${DIR}/scripts/144-serac.c" ]]; then
    CODE="${DIR}/scripts"
    ENGINE="${DIR}/src"
else
    fail "no serac source in ${DIR} — expected 144-serac.c there or in scripts/"
fi

for f in cera.c cera.h 098-engine-surface.syms; do
    [[ -f "${ENGINE}/${f}" ]] || fail "the engine is incomplete: no ${f} in ${ENGINE}"
done

[[ -n "${OUT}" ]] || OUT="${DIR}/serac"

# Somewhere to put the two things that are made on the way. The
# executable RAM tier when it exists, because these are compiled and one
# of them is run; a directory beside the output otherwise, since a
# release unpacked on a machine without this project's conventions
# should still build.
if [[ -d /tmp ]]; then
    WORK="$(mktemp -d /tmp/build-serac-XXXXXX)" || fail "nowhere to work"
    trap 'rm -rf "${WORK}"' EXIT
else
    fail "no /tmp to build in"
fi

# --- what each stage compiles -----------------------------------------
#
# The generator's own sources are every .c in the directory except the
# two front doors, each of which has a main: the generator's, and
# serac's. Listing them rather than globbing, because a release is a
# fixed set of files and a glob would quietly pick up whatever else
# somebody left there.
SHARED=(066-gentext.c 068-genparse.c 069-genemit.c 100-mapparse.c 105-mapwrite.c)

GEN_ARGS=()
for f in "${SHARED[@]}" 070-generate.c; do
    [[ -f "${CODE}/${f}" ]] || fail "missing source: ${CODE}/${f}"
    GEN_ARGS+=("${CODE}/${f}")
done

echo "one:   the generator"
"${CC}" -std=gnu11 -O2 -I"${CODE}" -I"${ENGINE}" \
    -o "${WORK}/generate" "${GEN_ARGS[@]}" \
    || fail "the generator did not compile"

echo "two:   the engine, as text"
"${WORK}/generate" --embed "${WORK}/145-embedded-engine.c" \
    "${ENGINE}/cera.h" "${ENGINE}/cera.c" "${ENGINE}/098-engine-surface.syms" \
    || fail "the engine could not be written out as text"

SERAC_ARGS=("${CODE}/144-serac.c")
for f in "${SHARED[@]}"; do
    SERAC_ARGS+=("${CODE}/${f}")
done
SERAC_ARGS+=("${WORK}/145-embedded-engine.c")

echo "three: serac"
"${CC}" -std=gnu11 -O2 -I"${CODE}" -I"${ENGINE}" -DSERAC_CC="\"${CC}\"" \
    -o "${OUT}" "${SERAC_ARGS[@]}" \
    || fail "serac did not compile"

# --- and it has to be the engine that went in -------------------------
#
# Checked here rather than trusted, because a serac carrying a different
# engine than the one beside it would build programs nobody could
# explain, and the check costs one unpack into a directory that is about
# to be thrown away.
"${OUT}" --unpack "${WORK}/check" >/dev/null || fail "serac cannot unpack what it carries"
for f in cera.c cera.h 098-engine-surface.syms; do
    cmp -s "${ENGINE}/${f}" "${WORK}/check/${f}" \
        || fail "the ${f} serac carries is not the one it was built from"
done

echo
echo "${OUT}"
echo "  carries the engine byte for byte, and invokes ${CC}"
echo
echo "Try it:"
echo "  ${OUT} yourmap.map yourboxes.c"
echo "  ${OUT} --unpack DIR     # the engine's own two files, if you"
echo "                          # would rather build against them"
