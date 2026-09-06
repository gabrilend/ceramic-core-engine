#!/usr/bin/env bash
# capture-tests.sh — run every test binary and record exactly what it said.
#
# What this is: the measuring instrument for issue 901. Moving the
# engine into one file is supposed to change nothing, and "the tests
# pass" does not prove that — identical bytes do. This runs each test
# binary, keeps its standard output and standard error together in one
# file per test, and normalises what legitimately differs between any
# two runs of the same binary.
#
# What has to be normalised, and why. Three kinds of thing vary without
# meaning anything:
#
#   1. Process ids, which appear in scratch directory paths.
#   2. Timings, which are wall-clock measurements.
#   3. Counts produced by concurrent scheduling. Four tests deliberately
#      race threads against each other and then report what happened —
#      how many pages a port grew under load, how deep the scrapyard
#      got, how many removals landed, what share of a delivery the copy
#      took. These are the tests doing their job; the numbers are
#      different every run on the same binary. They are listed below by
#      name and every number in them is flattened, so what is compared
#      is the shape of what they said rather than the values.
#
# What this instrument cannot do, which is worth knowing before trusting
# it. Flattening the numbers in a racing test does not flatten whether a
# line appears at all: the readiness test reports a buffer growing only
# when a producer actually outran its sibling, and under heavy load it
# sometimes does not. So a diff on those four can show a line present in
# one capture and absent in the other with nothing having changed.
#
# For a change that should not affect behaviour at all — moving comments,
# renaming, refolding — the stronger check is to compile the engine
# before and after into object files, from identical filenames in
# separate directories, and compare those byte for byte. Identical object
# code settles the question that identical output only suggests.
#
# Anything else that differs between two builds is a finding.
#
# Usage: capture-tests.sh <project-dir> <output-dir>
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
OUT="${2:-/dev/shm/minimal-soramech/capture-before}"
BUILD="${DIR}/tmp/build"

# Tests whose output is a report of a race. Compared by shape, not value.
RACY="023-test-readiness 063-test-fan-in-cost 076-test-destinations 077-test-removal"

mkdir -p "${OUT}"
rm -f "${OUT}"/*.txt

normalise() {
    sed -E 's/capture-[0-9]+/capture-PID/g' \
        | sed -E 's/(box|emitted)-[0-9]+-/\1-PID-/g' \
        | sed -E 's/[0-9]+(\.[0-9]+)?(ns|us|ms|s)\b/N\2/g' \
        | sed -E 's/\b[0-9]{4,}\b/NUM/g'
}

flatten() {
    sed -E 's/capture-[0-9]+/capture-PID/g' | sed -E 's/(box|emitted)-[0-9]+-/\1-PID-/g' | sed -E 's/[0-9]+(\.[0-9]+)?/N/g'
}

for t in "${BUILD}"/*; do
    [[ -x "$t" && -f "$t" ]] || continue
    name="$(basename "$t")"
    [[ "$name" == "generate" ]] && continue
    raw="$("$t" 2>&1)"
    if [[ " ${RACY} " == *" ${name} "* ]]; then
        echo "$raw" | flatten > "${OUT}/${name}.txt"
    else
        echo "$raw" | normalise > "${OUT}/${name}.txt"
    fi
done

for s in "${DIR}"/tests/*.sh; do
    [[ -f "$s" ]] || continue
    name="$(basename "$s")"
    bash "$s" "${DIR}" 2>&1 | normalise > "${OUT}/${name}.txt"
done

echo "captured $(ls -1 "${OUT}" | wc -l) test outputs into ${OUT}"
