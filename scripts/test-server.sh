#!/usr/bin/env bash
# Smoke test for soramech-server.lua.
# Starts the server, runs curl tests against it, then kills it.
# Exits 0 if all assertions pass, non-zero otherwise.

# {{{ --help — render this script's header doc block and exit
case "${1:-}" in
    -h|--help)
        sed -n '2,/^$/s/^# \?//p' "$0"
        exit 0
        ;;
esac
# }}}

DIR="/mnt/mtwo/programs/sora/soramech"
MAPS_ROOT="${1:-${DIR}/maps}"
PORT="${2:-7701}"
BASE="http://localhost:${PORT}"

PASS=0
FAIL=0

# {{{ assert_contains
assert_contains() {
    local label="${1}"
    local expected="${2}"
    local actual="${3}"
    if echo "${actual}" | grep -qF "${expected}"; then
        echo "  PASS: ${label}"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: ${label}"
        echo "        expected to contain: ${expected}"
        echo "        got: ${actual}"
        FAIL=$((FAIL + 1))
    fi
}
# }}}

# {{{ start_server
luajit "${DIR}/soramech-server.lua" "${MAPS_ROOT}" "${PORT}" > /tmp/soramech-test-server.log 2>&1 &
SERVER_PID=$!
sleep 0.5
if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo "FAIL: server did not start (check /tmp/soramech-test-server.log)"
    exit 1
fi
echo "server started (pid ${SERVER_PID})"
# }}}

cleanup() {
    kill "${SERVER_PID}" 2>/dev/null
}
trap cleanup EXIT

# Test 1: list maps
RESULT=$(curl -s "${BASE}/maps")
assert_contains "GET /maps returns hello" "hello" "${RESULT}"

# Test 2: list boxes in hello map
RESULT=$(curl -s "${BASE}/maps/hello/boxes")
assert_contains "GET /maps/hello/boxes returns greet" "greet" "${RESULT}"

# Test 3: read a box
RESULT=$(curl -s "${BASE}/maps/hello/boxes/greet")
assert_contains "GET /maps/hello/boxes/greet has id field" '"id"' "${RESULT}"
assert_contains "GET /maps/hello/boxes/greet has greet id" '"greet"' "${RESULT}"

# Test 4: create a new box (PUT)
NEW_BOX='{"id":"test-box","label":"Test","kind":"call","ref":"src/test.lua","fn":"run","inputs":[],"outputs":[],"connections":[],"ui":{"x":0,"y":0}}'
RESULT=$(curl -s -X PUT -H "Content-Type: application/json" \
    -d "${NEW_BOX}" "${BASE}/maps/hello/boxes/test-box")
assert_contains "PUT new box returns ok" '"ok"' "${RESULT}"

# Test 5: read it back
RESULT=$(curl -s "${BASE}/maps/hello/boxes/test-box")
assert_contains "GET new box has label" '"Test"' "${RESULT}"

# Test 6: update it
UPDATED='{"id":"test-box","label":"Updated","kind":"call","ref":"src/test.lua","fn":"run","inputs":[],"outputs":[],"connections":[],"ui":{"x":10,"y":10}}'
RESULT=$(curl -s -X PUT -H "Content-Type: application/json" \
    -d "${UPDATED}" "${BASE}/maps/hello/boxes/test-box")
assert_contains "PUT updated box returns ok" '"ok"' "${RESULT}"

RESULT=$(curl -s "${BASE}/maps/hello/boxes/test-box")
assert_contains "GET updated box has new label" '"Updated"' "${RESULT}"

# Test 7: delete it
RESULT=$(curl -s -X DELETE "${BASE}/maps/hello/boxes/test-box")
assert_contains "DELETE box returns ok" '"ok"' "${RESULT}"

RESULT=$(curl -s "${BASE}/maps/hello/boxes/test-box")
assert_contains "GET deleted box returns 404" '"error"' "${RESULT}"

# Test 8: schema rejection
BAD_BOX='{"id":"bad","label":123}'
RESULT=$(curl -s -X PUT -H "Content-Type: application/json" \
    -d "${BAD_BOX}" "${BASE}/maps/hello/boxes/bad")
assert_contains "PUT bad schema returns error" '"error"' "${RESULT}"

# Test 9: read meta
RESULT=$(curl -s "${BASE}/maps/hello/meta")
assert_contains "GET /maps/hello/meta has name" '"hello"' "${RESULT}"

# Test 10: read drivers
RESULT=$(curl -s "${BASE}/maps/hello/drivers")
assert_contains "GET /maps/hello/drivers has .lua" '".lua"' "${RESULT}"

echo ""
echo "Results: ${PASS} passed, ${FAIL} failed"
test "${FAIL}" -eq 0
