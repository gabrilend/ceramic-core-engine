#!/bin/bash
# scripts/run-unit-tests.sh — cheap, build-free unit-test runner.
#
# What it does, in CEO terms: runs every already-built C unit-test
# binary under build/tests/, captures pass/fail counts from each
# binary's trailing "N passed, M failed" summary line, and prints
# one row per binary plus a grand-total footer. No make recursion,
# no spec rebuilds, no integration tests — strictly an iteration
# loop for the C unit suite. `make test` remains the gold-standard
# "ship it" verification.
#
# Run as:
#   scripts/run-unit-tests.sh                 # run all built tests
#   scripts/run-unit-tests.sh 009             # only binaries starting with "009"
#   scripts/run-unit-tests.sh 31              # only 31* binaries
#   scripts/run-unit-tests.sh --quiet         # only totals
#   scripts/run-unit-tests.sh --verbose       # full output per binary
#   scripts/run-unit-tests.sh --dir <root>    # override project root
#
# Output (default):
#   009-slot-store-test                          23 /  23  ok
#   010-graph-loader-test                        13 /  13  ok
#   ...
#   N / M tests passed (K binaries)
#
# Failing binaries print their captured output below the row.
# Exit code is zero iff every binary passed, non-zero otherwise
# with the failing-binary count as the exit value (capped at 125
# to stay inside the standard exit-code range).
#
# Convention compliance per CLAUDE.md: hard-coded ${DIR} with
# --dir override; one shell command per line; tmp/ for ephemeral
# per-binary capture so the captures live in RAM rather than
# polluting the repo.

set -u

# {{{ defaults
DIR="/mnt/mtwo/programs/sora/soramech"
QUIET=0
VERBOSE=0
FILTER=""
# }}}

# {{{ argv parse
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dir)
            DIR="$2"
            shift 2
            ;;
        --quiet|-q)
            QUIET=1
            shift
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --help|-h)
            sed -n '2,/^$/s/^# \?//p' "$0"
            exit 0
            ;;
        --*)
            echo "run-unit-tests: unknown flag '$1'" >&2
            exit 2
            ;;
        *)
            if [[ -z "$FILTER" ]]; then
                FILTER="$1"
            else
                echo "run-unit-tests: only one filter prefix is supported (got '$1' after '$FILTER')" >&2
                exit 2
            fi
            shift
            ;;
    esac
done
# }}}

# {{{ sanity-check paths
TESTS_DIR="$DIR/build/tests"
if [[ ! -d "$TESTS_DIR" ]]; then
    echo "run-unit-tests: no build/tests/ at $TESTS_DIR — run 'make test' once to build the test binaries" >&2
    exit 1
fi

# Captures are ephemeral artifacts; they live in the RAM tier.
# ensure-tmp.sh builds the tmp/ symlink scheme if it's missing, so
# the old fall-back-to-/tmp branch is gone — if the tier can't be
# established, failing loudly here is correct (fallbacks are
# warnings, warnings are errors).
"$DIR/scripts/ensure-tmp.sh" "$DIR" >/dev/null
CAPTURE_DIR="$DIR/tmp/shared-memory/run-unit-tests"
mkdir -p "$CAPTURE_DIR"
# }}}

# {{{ collect_binaries — every built *-test executable, sorted
# The sort by filename also sorts by the project's leading
# numeric index, which gives a stable, project-meaningful order.
collect_binaries() {
    local b
    for b in "$TESTS_DIR"/*-test; do
        [[ -x "$b" ]] || continue
        local name
        name=$(basename "$b")
        if [[ -n "$FILTER" ]] && [[ "$name" != "$FILTER"* ]]; then continue; fi
        echo "$b"
    done | sort
}
# }}}

# {{{ run_one — execute one binary, parse its trailing summary
# The binary's last "N passed, M failed" line is the verdict; we
# look at the LAST such line so a test that prints intermediate
# summaries (none today, but the format is forgiving) still
# reports the final tally. On parse failure the binary's exit
# code stands in: zero → 1/1 ok; nonzero → 0/1 FAIL.
#
# Writes the per-binary capture to $CAPTURE_DIR/<name>.out so the
# verbose / failure paths can re-read it without re-running.
run_one() {
    local bin="$1"
    local name
    name=$(basename "$bin")
    local capture="$CAPTURE_DIR/$name.out"

    "$bin" > "$capture" 2>&1
    local rc=$?

    local last
    last=$(grep -E "^[[:space:]]*[0-9]+ passed, [0-9]+ failed" "$capture" | tail -n1)
    local pass=0
    local fail=0
    if [[ -n "$last" ]]; then
        pass=$(echo "$last" | grep -oE "^[[:space:]]*[0-9]+" | head -n1 | tr -d ' ')
        fail=$(echo "$last" | grep -oE "[0-9]+ failed" | grep -oE "^[0-9]+")
    else
        # No summary line — synthesise one from the exit code so
        # we still produce a row. Counts as one logical test.
        if [[ "$rc" -eq 0 ]]; then pass=1; fail=0; else pass=0; fail=1; fi
    fi

    local total=$((pass + fail))
    local status="ok"
    if [[ "$fail" -gt 0 ]] || [[ "$rc" -ne 0 ]]; then status="FAIL"; fi

    # Stash row for the printer + totals via global arrays.
    BINARY_NAMES+=("$name")
    BINARY_PASS+=("$pass")
    BINARY_TOTAL+=("$total")
    BINARY_STATUS+=("$status")
    BINARY_RC+=("$rc")
}
# }}}

# {{{ main loop
BINARY_NAMES=()
BINARY_PASS=()
BINARY_TOTAL=()
BINARY_STATUS=()
BINARY_RC=()

mapfile -t BINS < <(collect_binaries)
if [[ "${#BINS[@]}" -eq 0 ]]; then
    if [[ -n "$FILTER" ]]; then
        echo "run-unit-tests: no built binaries match filter '$FILTER'" >&2
    else
        echo "run-unit-tests: no built test binaries in $TESTS_DIR" >&2
    fi
    exit 1
fi

for b in "${BINS[@]}"; do
    run_one "$b"
done
# }}}

# {{{ print results
total_pass=0
total_all=0
total_fail_bins=0

if [[ "$QUIET" -eq 0 ]]; then
    for i in "${!BINARY_NAMES[@]}"; do
        printf "  %-44s %3d / %3d  %s\n" \
            "${BINARY_NAMES[$i]}" \
            "${BINARY_PASS[$i]}" \
            "${BINARY_TOTAL[$i]}" \
            "${BINARY_STATUS[$i]}"
        if [[ "$VERBOSE" -eq 1 ]] || [[ "${BINARY_STATUS[$i]}" == "FAIL" ]]; then
            sed 's/^/      /' "$CAPTURE_DIR/${BINARY_NAMES[$i]}.out"
            echo
        fi
    done
    echo
fi

for i in "${!BINARY_NAMES[@]}"; do
    total_pass=$((total_pass + BINARY_PASS[i]))
    total_all=$((total_all + BINARY_TOTAL[i]))
    if [[ "${BINARY_STATUS[$i]}" == "FAIL" ]]; then
        total_fail_bins=$((total_fail_bins + 1))
    fi
done

if [[ "$total_fail_bins" -eq 0 ]]; then
    printf "  %d / %d tests passed (%d binaries)\n" \
        "$total_pass" "$total_all" "${#BINARY_NAMES[@]}"
    exit 0
else
    printf "  %d / %d tests passed (%d binaries; %d FAILED)\n" \
        "$total_pass" "$total_all" "${#BINARY_NAMES[@]}" "$total_fail_bins"
    # Clamp exit code into the 1..125 range so it survives shell
    # propagation regardless of how many binaries failed.
    rc="$total_fail_bins"
    if [[ "$rc" -gt 125 ]]; then rc=125; fi
    exit "$rc"
fi
# }}}
