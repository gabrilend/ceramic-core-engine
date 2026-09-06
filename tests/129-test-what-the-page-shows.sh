#!/usr/bin/env bash
# 129-test-what-the-page-shows.sh — the drawing's claims against the events.
#
# What this proves: that "this wire is carrying" means what it says.
#
# The page lights a wire for a fixed time after something crosses it,
# and if that time is longer than the program's own rhythm the light
# outlives the truth. The ladder alternates — ten values down one exit
# of a comparator, then ten down another — so a window longer than a
# burst leaves both exits lit at once, which is a picture of something
# that cannot happen: one value goes to one exit.
#
# So this replays a real event stream through the page's own rule, with
# the number read out of the page's own source rather than copied here,
# and measures how much of the time two exits of one comparator are
# shown live together. A little is honest, at the moment a burst hands
# over. A lot means the drawing is describing the past as the present.
#
# No browser is involved and none is needed: what is being checked is
# the arithmetic the browser would do.
#
# Usage: 129-test-what-the-page-shows.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
BUILD="${DIR}/tmp/build"
LUAJIT="$(ls "${DIR}"/toolchain/bin/luajit 2>/dev/null || echo luajit)"
RING="/dev/shm/$(basename "${DIR}")/page-check-$$.ring"
PORT=$(( 19000 + (RANDOM % 3000) ))
STREAM="/tmp/$(basename "${DIR}")/page-check-$$.txt"

command -v curl >/dev/null || { echo "  no curl, so the page cannot be checked"; exit 0; }
command -v "${LUAJIT}" >/dev/null 2>&1 || { echo "  no luajit, so the replay cannot run"; exit 0; }

program_pid=""
cleanup() {
    [[ -n "${program_pid}" ]] && kill "${program_pid}" 2>/dev/null
    rm -f "${RING}" "${RING}.map" "${STREAM}"
}
trap cleanup EXIT
fail() { echo "  $*"; exit 1; }

"${BUILD}/128-watch-a-map" --map=127-the-ladder.map --trail="${RING}" \
    --pace=60 --view="${PORT}" >/dev/null 2>&1 &
program_pid=$!
sleep 2
kill -0 "${program_pid}" 2>/dev/null || fail "the ladder did not stay up"

timeout 7 curl -sN "http://localhost:${PORT}/events" > "${STREAM}" 2>/dev/null
[[ -s "${STREAM}" ]] || fail "no events arrived"

"${LUAJIT}" "${DIR}/scripts/130-replay-the-page.lua" \
    "${STREAM}" "${DIR}/viewer/122-viewer.js" 3 0 2
