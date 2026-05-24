#!/bin/bash
# tests/315-refs-test.sh — unit-style tests for scripts/soramech-ref.sh.
#
# What it does, in CEO terms: exercises the acquire / release / list /
# count / reap commands of the reference helper through every
# meaningful state transition, asserting the right number of live
# references at each step. The test creates a private temp directory
# per scenario, runs the helper against it, and reports per-scenario
# pass / fail.
#
# Run as:
#   tests/315-refs-test.sh
#   tests/315-refs-test.sh --dir <project-root>
#
# Convention: hard-coded ${DIR} with --dir override; everything
# relative to ${DIR}.

set -u

# {{{ defaults
DIR="/mnt/mtwo/programs/sora/soramech"
if [[ $# -ge 1 ]] && [[ "$1" == "--dir" ]]; then
    DIR="$2"
fi
REF="$DIR/scripts/soramech-ref.sh"
# }}}

# {{{ counters
pass=0
fail=0
failures=()
# }}}

# {{{ assert_count — soramech-ref.sh count <dir> should equal $expected
assert_count() {
    local label="$1"
    local target="$2"
    local expected="$3"
    local got
    got=$("$REF" count "$target")
    if [[ "$got" == "$expected" ]]; then
        printf "  %-44s ok\n" "$label"
        pass=$((pass + 1))
    else
        printf "  %-44s FAIL — expected %s, got %s\n" "$label" "$expected" "$got"
        fail=$((fail + 1))
        failures+=("$label: expected $expected, got $got")
    fi
}
# }}}

# {{{ scenario_basic — acquire, list, release, count
scenario_basic() {
    local tmp
    tmp=$(mktemp -d)
    assert_count "basic: fresh dir has 0"            "$tmp" "0"
    local id
    id=$("$REF" acquire "$tmp")
    assert_count "basic: after acquire has 1"        "$tmp" "1"
    "$REF" release "$tmp" --id "$id"
    assert_count "basic: after release has 0"        "$tmp" "0"
    rm -rf "$tmp"
}
# }}}

# {{{ scenario_two_acquires — independent ids, partial release
scenario_two_acquires() {
    local tmp
    tmp=$(mktemp -d)
    local id1 id2
    id1=$("$REF" acquire "$tmp")
    id2=$("$REF" acquire "$tmp")
    assert_count "two-acquires: count 2"             "$tmp" "2"
    "$REF" release "$tmp" --id "$id1"
    assert_count "two-acquires: after one release 1" "$tmp" "1"
    "$REF" release "$tmp" --id "$id2"
    assert_count "two-acquires: after both 0"        "$tmp" "0"
    rm -rf "$tmp"
}
# }}}

# {{{ scenario_stale_pid — a fabricated dead pid validates as stale
# We append a fabricated acquire line with a PID that doesn't exist
# (the PID-MAX on Linux is usually 4194304; 9999999 is safely past
# it on every kernel we'd run on).
scenario_stale_pid() {
    local tmp
    tmp=$(mktemp -d)
    local id1
    id1=$("$REF" acquire "$tmp")
    printf 'acquire fake-dead %s 9999999 9999999 -\n' "2026-05-24T00:00:00-07:00" >> "$tmp/.refs"
    assert_count "stale: live one counts, fake doesn't" "$tmp" "1"
    "$REF" reap "$tmp"
    # After reap, only the live entry remains. The released-flag
    # entry from id1 (we haven't released yet) stays.
    assert_count "stale: after reap still 1 live"    "$tmp" "1"
    "$REF" release "$tmp" --id "$id1"
    assert_count "stale: after release of real 0"    "$tmp" "0"
    rm -rf "$tmp"
}
# }}}

# {{{ scenario_marker — marker file presence gates validity
# Acquire with a marker, count should be 1. Remove the marker, count
# drops to 0 (marker missing = ref is stale).
scenario_marker() {
    local tmp marker
    tmp=$(mktemp -d)
    marker="$tmp/holder.alive"
    touch "$marker"
    local id1
    id1=$("$REF" acquire "$tmp" --marker "$marker")
    assert_count "marker: present → 1 live"          "$tmp" "1"
    rm -f "$marker"
    assert_count "marker: missing → 0 live"          "$tmp" "0"
    rm -rf "$tmp"
}
# }}}

# {{{ scenario_list_json — list output should be JSON-shaped enough
# Not a strict JSON parse (no jq) — we just want to see the array
# brackets and the expected number of records inside.
scenario_list_json() {
    local tmp
    tmp=$(mktemp -d)
    local id1 id2
    id1=$("$REF" acquire "$tmp")
    id2=$("$REF" acquire "$tmp")
    local out
    out=$("$REF" list "$tmp")
    local first_char
    first_char="${out:0:1}"
    local last_char
    last_char="${out: -1}"
    local n_records
    n_records=$(echo "$out" | grep -c '"valid":')
    if [[ "$first_char" == "[" ]] && [[ "$last_char" == "]" ]] && [[ "$n_records" == "2" ]]; then
        printf "  %-44s ok\n" "list: JSON array with 2 records"
        pass=$((pass + 1))
    else
        printf "  %-44s FAIL — first=%s last=%s records=%s\n" \
            "list: JSON array with 2 records" "$first_char" "$last_char" "$n_records"
        fail=$((fail + 1))
        failures+=("list-json: first=$first_char last=$last_char records=$n_records")
    fi
    rm -rf "$tmp"
}
# }}}

# {{{ run
echo "315 — reference helper tests:"
scenario_basic
scenario_two_acquires
scenario_stale_pid
scenario_marker
scenario_list_json
echo
echo "  $pass passed, $fail failed"
if [[ "$fail" -gt 0 ]]; then
    echo "  failures:"
    for f in "${failures[@]}"; do echo "    - $f"; done
    exit 1
fi
exit 0
# }}}
