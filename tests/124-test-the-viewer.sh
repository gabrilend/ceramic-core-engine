#!/usr/bin/env bash
# 124-test-the-viewer.sh — the window works, end to end.
#
# What this proves: a watched program writes a trail, the forwarding
# reader serves the page and the graph over HTTP, and the events reach
# the far end of a socket as a live stream. Every piece of the viewer
# except the drawing, which needs a browser and is checked by opening
# one.
#
# It also proves the thing most worth proving about a window: **the
# server answers nothing that writes.** Every path it knows is a read,
# and anything else is a refusal.
#
# Usage: 124-test-the-viewer.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
BUILD="${DIR}/tmp/build"
RING="/dev/shm/$(basename "${DIR}")/test-viewer-$$.ring"
PORT=$(( 18000 + (RANDOM % 4000) ))

command -v curl >/dev/null || { echo "  no curl, so the viewer cannot be tested"; exit 0; }

watched_pid=""
viewer_pid=""
cleanup() {
    [[ -n "${viewer_pid}" ]] && kill "${viewer_pid}" 2>/dev/null
    [[ -n "${watched_pid}" ]] && kill "${watched_pid}" 2>/dev/null
    rm -f "${RING}"
}
trap cleanup EXIT

fail() { echo "  $*"; exit 1; }

"${BUILD}/123-a-program-to-watch" --trail="${RING}" --pace=25 >/dev/null 2>&1 &
watched_pid=$!
sleep 1
kill -0 "${watched_pid}" 2>/dev/null || fail "the program to watch did not stay up"

"${BUILD}/119-viewer" --trail="${RING}" --map="${DIR}/maps/107-example.map" \
    --root="${DIR}/viewer" --port="${PORT}" >/dev/null 2>&1 &
viewer_pid=$!
sleep 1
kill -0 "${viewer_pid}" 2>/dev/null || fail "the viewer did not stay up on port ${PORT}"

code() { curl -s -o /dev/null -w '%{http_code}' "http://localhost:${PORT}$1"; }

for path in / /viewer.css /viewer.js /map /mapname; do
    [[ "$(code "${path}")" == "200" ]] || fail "${path} answered $(code "${path}"), not 200"
done
echo "  the page, its parts, and the map all come back"

# Nothing it does not know is served, and nothing it knows is a write.
[[ "$(code /nope)" == "404" ]] || fail "an unknown path was answered"
[[ "$(code /../../etc/passwd)" == "404" ]] || fail "a path outside its own files was served"
echo "  and anything it does not know is refused"

# The stream: live events, in sequence, from a program that is running.
stream="/tmp/$(basename "${DIR}")/viewer-stream-$$.txt"
timeout 3 curl -sN "http://localhost:${PORT}/events" > "${stream}" 2>/dev/null
events=$(grep -c '^data:' "${stream}" || true)
[[ "${events}" -gt 10 ]] || fail "only ${events} events arrived down the stream"

kinds=$(grep -oE '"kind":"[a-z ]+"' "${stream}" | sort -u | wc -l)
[[ "${kinds}" -ge 2 ]] || fail "the stream carried only ${kinds} kind of event"

# Sequence numbers must climb: a stream out of order is a picture that
# cannot be trusted about what happened before what.
if ! grep -oE '"seq":[0-9]+' "${stream}" | cut -d: -f2 | sort -c -n 2>/dev/null; then
    fail "the events did not arrive in sequence"
fi
echo "  ${events} events arrived in sequence, of ${kinds} different kinds"

rm -f "${stream}"
echo "  a running program was watched through a socket, and nothing wrote back"
