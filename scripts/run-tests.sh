#!/bin/bash
# scripts/run-tests.sh — phase 3 integration tests.
#
# Runs every fixture map under tests/maps/ through `soramech-pool`
# and asserts that the captured outputs match a hardcoded
# expectation per map. The point is to catch regressions
# end-to-end — past the spec interface, past the dispatch action,
# past JSONL writing — rather than via the in-process unit tests.
#
# Per the project convention: scripts run from any directory via a
# hard-coded ${DIR} path with an override argument; all paths are
# relative to ${DIR}.

# {{{ --help — render this script's header doc block and exit
case "${1:-}" in
    -h|--help)
        sed -n '2,/^$/s/^# \?//p' "$0"
        exit 0
        ;;
esac
# }}}

set -u

DIR="/mnt/mtwo/programs/sora/soramech"
if [[ $# -ge 1 ]]; then
    DIR="$1"
fi

cd "$DIR"

# Establish the RAM-backed temp tiers before anything writes a log
# or scratch file (project convention: ephemeral output never lands
# on spinning disk). TEST_SCRATCH must stay literally in sync with
# the absolute paths baked into fixture write boxes under
# tests/maps/*/boxes/ — box JSON has no variable expansion.
"$DIR/scripts/ensure-tmp.sh" "$DIR" >/dev/null
TEST_SCRATCH="/dev/shm/soramech/tests"
mkdir -p "$TEST_SCRATCH"

pass=0
fail=0
failures=()

# {{{ check_map() — run one fixture, verify expected substrings
check_map() {
    local map="$1"
    shift
    local expected=("$@")

    printf "  %-44s " "$map"

    local output
    # Per-map env opt-in: a map may set SORAMECH_TEST_WORKERS to
    # pin worker count for tests that depend on per-worker state
    # (e.g. issue 318's $lang_opaque, which lives in the producer's
    # Lua registry and can only be reconstructed by a consumer on
    # the same worker).
    local workers_env=""
    if [[ -f "$DIR/tests/maps/$map/.test_workers" ]]; then
        workers_env=$(cat "$DIR/tests/maps/$map/.test_workers")
    fi
    if [[ -n "$workers_env" ]]; then
        output=$(SORAMECH_WORKERS="$workers_env" "$DIR/soramech-pool" "$DIR/tests/maps/$map" 2>&1)
    else
        output=$("$DIR/soramech-pool" "$DIR/tests/maps/$map" 2>&1)
    fi
    local rc=$?

    local missing=""
    if [[ $rc -ne 0 ]]; then
        missing="exit=$rc"
    fi
    for e in "${expected[@]}"; do
        if [[ "$output" != *"$e"* ]]; then
            missing+="${missing:+; }missing '$e'"
        fi
    done

    if [[ -z "$missing" ]]; then
        printf "ok\n"
        pass=$((pass + 1))
    else
        printf "FAIL — %s\n" "$missing"
        fail=$((fail + 1))
        failures+=("$map: $missing")
    fi
}
# }}}

# {{{ check_jsonl() — JSONL transcript diff against an expected fixture
# Stricter than check_map: runs the map, normalises the transcript
# (drops ts / durations / worker_idx / slot_id / task_id / n_workers,
# sorts events alphabetically), and diffs against tests/expected/<map>.jsonl.
# A divergence here means the map's runtime behaviour changed in a way
# that the substring-checks in check_map wouldn't catch — e.g. a new
# event type appearing, a box firing an extra time, an output size
# changing.
#
# Re-baselining: a fixture's expected file is updated by running the
# map manually:
#   ./soramech-pool tests/maps/<name> > /dev/null
#   scripts/normalise-jsonl.lua /tmp/soramech-last-run.jsonl \
#     > tests/expected/<name>.jsonl
# and committing the result.
check_jsonl() {
    local map="$1"
    local expected="$DIR/tests/expected/$map.jsonl"
    # actual_raw is the C pool runner's own no-map-tmp fallback path
    # (see src/008-pool-runner.c) — a runner contract, not a scratch
    # location this script chooses.
    local actual_raw="/tmp/soramech-last-run.jsonl"
    local actual_norm="$TEST_SCRATCH/norm-$map-$$.jsonl"

    printf "  %-44s " "$map (transcript diff)"

    if [[ ! -f "$expected" ]]; then
        printf "FAIL — expected fixture missing: %s\n" "$expected"
        fail=$((fail + 1))
        failures+=("$map transcript: expected missing")
        return
    fi

    rm -f "$actual_raw"
    local rc
    if [[ -f "$DIR/tests/maps/$map/.test_workers" ]]; then
        local workers_env
        workers_env=$(cat "$DIR/tests/maps/$map/.test_workers")
        SORAMECH_WORKERS="$workers_env" "$DIR/soramech-pool" "$DIR/tests/maps/$map" >/dev/null 2>&1
        rc=$?
    else
        "$DIR/soramech-pool" "$DIR/tests/maps/$map" >/dev/null 2>&1
        rc=$?
    fi
    if [[ $rc -ne 0 ]]; then
        printf "FAIL — pool exit=%d\n" "$rc"
        fail=$((fail + 1))
        failures+=("$map transcript: pool exit=$rc")
        return
    fi

    "$DIR/scripts/normalise-jsonl.lua" --dir "$DIR" "$actual_raw" > "$actual_norm"
    if ! diff -u "$expected" "$actual_norm" >"$TEST_SCRATCH/jsonl-diff-$$.txt" 2>&1; then
        printf "FAIL — transcript diverges\n"
        sed 's/^/      /' "$TEST_SCRATCH/jsonl-diff-$$.txt"
        fail=$((fail + 1))
        failures+=("$map transcript: diff failed")
    else
        printf "ok\n"
        pass=$((pass + 1))
    fi
    rm -f "$actual_norm" "$TEST_SCRATCH/jsonl-diff-$$.txt"
}
# }}}

# {{{ pipeline_output_check() — additional: the pipeline writes to disk
pipeline_output_check() {
    local file="$TEST_SCRATCH/pipeline-out.txt"
    rm -f "$file"
    "$DIR/soramech-pool" "$DIR/tests/maps/pipeline" > /dev/null 2>&1
    printf "  %-44s " "pipeline write output"
    if [[ -f "$file" ]]; then
        local content
        content=$(cat "$file")
        if [[ "$content" == "11!" ]]; then
            printf "ok\n"
            pass=$((pass + 1))
        else
            printf "FAIL — expected '11!', got '%s'\n" "$content"
            fail=$((fail + 1))
            failures+=("pipeline file: $content")
        fi
    else
        printf "FAIL — output file missing\n"
        fail=$((fail + 1))
        failures+=("pipeline: $file missing")
    fi
}
# }}}

# {{{ compile_pipeline_check() — soramech-compile produces a portable artifact
# Verifies issue 309's compile pipeline: bundles all spec.so + sources
# into compiled/, then runs the artifact from /tmp (i.e. with no
# dependency on the project's cwd) and verifies it produces the same
# output as the source map.
compile_pipeline_check() {
    local map="$DIR/tests/maps/pipeline"
    local compiled="$map/compiled"
    local file="$TEST_SCRATCH/pipeline-out.txt"

    printf "  %-44s " "compile pipeline (portable run)"

    rm -rf "$compiled"
    if ! "$DIR/scripts/soramech-compile.sh" "$map" --dir "$DIR" >/dev/null 2>&1; then
        printf "FAIL — soramech-compile failed\n"
        fail=$((fail + 1))
        failures+=("compile: script returned non-zero")
        return
    fi
    if [[ ! -x "$compiled/pool-runner" ]]; then
        printf "FAIL — compiled/pool-runner missing or not executable\n"
        fail=$((fail + 1))
        failures+=("compile: pool-runner missing")
        return
    fi

    rm -f "$file"
    local output
    output=$(cd /tmp && "$compiled/pool-runner" "$compiled" 2>&1)

    local content=""
    [[ -f "$file" ]] && content=$(cat "$file")

    if [[ "$output" == *"addone → 11"* ]] && [[ "$content" == "11!" ]]; then
        printf "ok\n"
        pass=$((pass + 1))
    else
        printf "FAIL — output='%s' file='%s'\n" "$output" "$content"
        fail=$((fail + 1))
        failures+=("compile: bad output")
    fi
}
# }}}

# {{{ run all fixtures
echo "integration tests (fixture maps):"

check_map "calc" \
    "add → 42"

check_map "hello" \
    "greet → Hello, World!"
# Opt-in transcript diff. The hello fixture is the simplest map
# in the suite — one box, one fire — so it's the cleanest place
# to demonstrate the normalised-JSONL check. Other fixtures
# adopt it as their transcripts stabilise.
check_jsonl "hello"
check_jsonl "pipeline"

check_map "comparator" \
    "classify → 8" \
    "high → HIGH:8" \
    "low → (no output)" \
    "mid → (no output)"

check_map "iter-route" \
    "iter → hi" \
    "a → A:hi" \
    "b → (no output)" \
    "c → (no output)"

# Issue 240 — randomizer routing. Hash(counter=0) mod 3 = 0, so
# the first invocation deterministically lands on branch 0;
# tagger_0 fires, tagger_1 and tagger_2 stay silent.
check_map "randomizer-route" \
    "tagger_0 → got_0:hi" \
    "tagger_1 → (no output)" \
    "tagger_2 → (no output)"

# Issue 241 — weighted routing. weights = [1, 0, 0] puts all
# probability mass on branch 0; the cumulative-band lookup
# always picks tagger_0, the other two never fire.
check_map "weighted-route" \
    "tagger_0 → got_0:hi" \
    "tagger_1 → (no output)" \
    "tagger_2 → (no output)"

# Issue 242 — distributor routing. All downstream slots empty
# means the argmin sees a tie; the tiebreaker counter at 0
# picks branch 0. Subsequent calls would rotate via the
# counter; this fixture exercises the first call only.
check_map "distributor-route" \
    "tagger_0 → got_0:hi" \
    "tagger_1 → (no output)" \
    "tagger_2 → (no output)"

# Issue 243 — multi-band comparator. Thresholds [3, 7] carve
# three bands. Seed value 5 lands strictly between 3 and 7, so
# only the between_3_7 branch fires (tagger_1). The picker also
# proves the band-name formatter matches between dispatch and
# the wire's from_branch string.
check_map "multi-band-comparator-route" \
    "tagger_0 → (no output)" \
    "tagger_1 → got_1:5" \
    "tagger_2 → (no output)"

# Issue 253 — nonlinearity refactor. The routing kind grew an
# auto-calibrating ring buffer for bounds, dropped the variant
# names in favour of a `range` toggle, and the output is now
# v × score (gated linear unit) rather than score alone. Single-
# fire fixtures cover the cold-start path: the buffer has only
# the one observed value, n_filled < 2 triggers the neutral
# score (0 for signed, 0.5 for unit), and the gated output is
# v × neutral.
#
#   signed : tanh,   input v=5  → score 0 (cold start) → v×0 = 0
#   unit   : sigmoid, input v=10 → score 0.5 (cold start) → v×0.5 = 5
check_map "nonlinearity-signed" \
    "tagger → 0"
check_map "nonlinearity-unit" \
    "tagger → 5"

check_map "pipeline" \
    "double → 10" \
    "addone → 11" \
    "shout → 11!"

check_map "read-literal" \
    "echo → hello, world!"

check_map "319a-many-inputs" \
    "combine → count=20 sum=210"

check_map "246-c-shim" \
    "echo → seen:HELLO-FROM-SEED"

check_map "246-lua-shim" \
    "echo → seen:REV-fedcba"

# Bug 323 — a table returned on the all-Lua fast path used to hit
# lua_tolstring's NULL return and leave as a silent zero-byte wire.
# The producer's table now rides the wire as JSON and the consumer
# receives a real table; the fixture's consumer names the actual
# received type on failure so a regression reads clearly.
check_map "323-table-fast-path" \
    'consumer → sum=42:x'

# Issue 318 — the producer's output proves the emit side works
# end-to-end (Lua function → $lang_opaque sentinel in JSON). The
# consumer's reconstruction needs the dual-ring slot to surface
# "I was written as JSON" to the same-language consumer, which is
# a deeper per-edge classification issue documented in 318's
# closeout; the emit fixture stands today.
check_map "318-lang-opaque" \
    '"$lang_opaque"' \
    '"lang":"lua"' \
    '"shape":"function"' \
    'consumer → doubled=42'

# Issue 248 input-side encapsulation. The parent's read box wires
# a value into a BOX_MAP whose sub-map has one externally-supplied
# data box; the graph loader splices the parent's wire through to
# the sub-map's write box at load time. Verification is split: the
# stdout check confirms the renamed write box fires; the disk file
# check confirms the parent's bytes actually arrived through the
# splice (the only path that produces the file's content).
check_map "248-encap-input-only" \
    "encap__writer → true"

# Issue 248 output-side encapsulation. The encap declares an output
# port; the sub-map's externally-consumed write box emits its
# `value` bytes downstream (rather than the usual "true" success
# string) and the encapsulation pass appends the parent's downstream
# wire onto that write box's connections. The renamed sub-writer
# emits the payload value, and the parent's writer then reports
# "true" after writing that value to disk.
check_map "248-encap-output-only" \
    "encap__external_writer → round-trip-through-encap" \
    "parent_writer → true"

# Issue 248 recursion. A two-level nesting: the parent encapsulates
# the outer sub-map, which itself encapsulates the inner sub-map.
# The inline-encapsulations pass has to iterate twice (once per
# nesting level), and both the input- and output-side splices have
# to compose through the prefixed-twice ids. All three writes report
# the same value, proving the splice carried it through both
# boundaries.
check_map "248-encap-recursive" \
    "encap_outer__encap_inner__inner_writer → twice-nested-greeting" \
    "encap_outer__outer_writer → twice-nested-greeting" \
    "parent_writer → true"

# {{{ encap_input_only_file_check() — input-side encap reaches disk
encap_input_only_file_check() {
    local file="$TEST_SCRATCH/248-encap-out.txt"
    rm -f "$file"
    "$DIR/soramech-pool" "$DIR/tests/maps/248-encap-input-only" >/dev/null 2>&1
    printf "  %-44s " "encap input-only write output"
    if [[ -f "$file" ]]; then
        local content
        content=$(cat "$file")
        if [[ "$content" == "encapsulated-greeting" ]]; then
            printf "ok\n"
            pass=$((pass + 1))
        else
            printf "FAIL — expected 'encapsulated-greeting', got '%s'\n" "$content"
            fail=$((fail + 1))
            failures+=("encap-input file: $content")
        fi
    else
        printf "FAIL — output file missing\n"
        fail=$((fail + 1))
        failures+=("encap-input: $file missing")
    fi
}
# }}}

# {{{ encap_recursive_file_check() — recursion exercises every disk
# artifact across both nesting levels. The inner sub-map's writer
# writes one file, the outer sub-map's writer writes another, and
# the parent's writer writes a third — all three should hold the
# same byte sequence, proving the value flowed from the parent
# through both encapsulation boundaries.
encap_recursive_file_check() {
    local inner_file="$TEST_SCRATCH/248-encap-recursive-inner.txt"
    local outer_file="$TEST_SCRATCH/248-encap-recursive-outer.txt"
    local parent_file="$TEST_SCRATCH/248-encap-recursive-parent.txt"
    rm -f "$inner_file" "$outer_file" "$parent_file"
    "$DIR/soramech-pool" "$DIR/tests/maps/248-encap-recursive" >/dev/null 2>&1
    printf "  %-44s " "encap recursive write outputs"
    local detail=""
    local want="twice-nested-greeting"
    [[ -f "$inner_file"  && "$(cat "$inner_file")"  == "$want" ]] || \
        detail+="inner='$(cat "$inner_file" 2>/dev/null)' "
    [[ -f "$outer_file"  && "$(cat "$outer_file")"  == "$want" ]] || \
        detail+="outer='$(cat "$outer_file" 2>/dev/null)' "
    [[ -f "$parent_file" && "$(cat "$parent_file")" == "$want" ]] || \
        detail+="parent='$(cat "$parent_file" 2>/dev/null)' "
    if [[ -z "$detail" ]]; then
        printf "ok\n"
        pass=$((pass + 1))
    else
        printf "FAIL — %s\n" "$detail"
        fail=$((fail + 1))
        failures+=("encap-recursive: $detail")
    fi
}
# }}}

# {{{ refs_unit_tests() — issue 315: ref helper acquire/release/list/reap
# Runs the dedicated 315-refs-test.sh and folds its overall pass/fail
# into the suite's counters. The detailed per-scenario output goes
# only on failure to keep the suite quiet on green.
refs_unit_tests() {
    printf "  %-44s " "315-refs unit tests"
    local out
    out=$("$DIR/tests/315-refs-test.sh" --dir "$DIR" 2>&1)
    local rc=$?
    if [[ $rc -eq 0 ]]; then
        printf "ok\n"
        pass=$((pass + 1))
    else
        printf "FAIL\n"
        echo "$out" | sed 's/^/      /'
        fail=$((fail + 1))
        failures+=("315-refs unit: rc=$rc")
    fi
}
# }}}

# {{{ refs_fork_on_live_check() — issue 315: compile forks on live refs
# End-to-end integration. Compile the pipeline fixture, acquire a
# reference on the result, compile again — the second run should
# fork to a sibling generation (`compiled.1/`) and leave the original
# untouched. The pinned generation must remain runnable after the
# fork (its pool-runner / spec.so / sources are still where they
# were). After the assertion the test releases the reference and
# clears every generation directory so it doesn't interfere with the
# compile_pipeline_check that runs alongside.
refs_fork_on_live_check() {
    local map="$DIR/tests/maps/pipeline"
    local base="$map/compiled"
    printf "  %-44s " "315 compile fork on live refs"

    rm -rf "$base" "$map"/compiled.*
    "$DIR/scripts/soramech-compile.sh" "$map" --dir "$DIR" >/dev/null 2>&1

    local id
    id=$("$DIR/scripts/soramech-ref.sh" acquire "$base")
    "$DIR/scripts/soramech-compile.sh" "$map" --dir "$DIR" >/dev/null 2>&1

    local detail=""
    [[ -d "$base"        ]] || detail+="compiled/ missing; "
    [[ -d "$map/compiled.1" ]] || detail+="compiled.1/ missing; "
    grep -q '"forked_from"' "$map/compiled.1/manifest.json" 2>/dev/null \
        || detail+="forked_from missing from compiled.1; "
    # Pinned generation must still run. Capture stdout/err so a
    # crash on the pinned dir surfaces clearly.
    if ! (cd /tmp && "$base/pool-runner" "$base" >/dev/null 2>&1); then
        detail+="pinned generation no longer runs; "
    fi

    "$DIR/scripts/soramech-ref.sh" release "$base" --id "$id" >/dev/null
    rm -rf "$base" "$map"/compiled.*

    if [[ -z "$detail" ]]; then
        printf "ok\n"
        pass=$((pass + 1))
    else
        printf "FAIL — %s\n" "$detail"
        fail=$((fail + 1))
        failures+=("315-refs fork: $detail")
    fi
}
# }}}

# {{{ encap_output_only_file_check() — output-side encap reaches both
# the sub-map's disk artifact AND the parent's disk artifact via the
# spliced wire. The two files should be byte-identical because the
# write box does its disk write THEN pushes the same value bytes
# downstream to the parent's writer.
encap_output_only_file_check() {
    local sub_file="$TEST_SCRATCH/248-encap-output-sub.txt"
    local out_file="$TEST_SCRATCH/248-encap-output-out.txt"
    rm -f "$sub_file" "$out_file"
    "$DIR/soramech-pool" "$DIR/tests/maps/248-encap-output-only" >/dev/null 2>&1
    printf "  %-44s " "encap output-only write outputs"
    local sub_ok=0 out_ok=0 detail=""
    if [[ -f "$sub_file" && "$(cat "$sub_file")" == "round-trip-through-encap" ]]; then
        sub_ok=1
    else
        detail+="sub-file=${sub_file}:'$(cat "$sub_file" 2>/dev/null)' "
    fi
    if [[ -f "$out_file" && "$(cat "$out_file")" == "round-trip-through-encap" ]]; then
        out_ok=1
    else
        detail+="out-file=${out_file}:'$(cat "$out_file" 2>/dev/null)' "
    fi
    if [[ $sub_ok -eq 1 && $out_ok -eq 1 ]]; then
        printf "ok\n"
        pass=$((pass + 1))
    else
        printf "FAIL — %s\n" "$detail"
        fail=$((fail + 1))
        failures+=("encap-output: $detail")
    fi
}
# }}}

# {{{ legacy_data_tests() — phase-1 data library unit tests
# 003-data-test.lua predates the C runner but covers the still-live
# libs/soramech-data.lua semantics (constants, refused writes,
# missing-field errors). Folded in here so it runs on every suite
# pass instead of silently rotting outside any test target. Its
# sibling 004-driver-test-runner.lua stays out: it needs the
# old-schema maps/driver-test map (see issue 111).
legacy_data_tests() {
    printf "  %-44s " "003-data unit tests (lua)"
    local out
    out=$(luajit "$DIR/tests/003-data-test.lua" 2>&1)
    local rc=$?
    if [[ $rc -eq 0 && "$out" == *"All data tests passed."* ]]; then
        printf "ok\n"
        pass=$((pass + 1))
    else
        printf "FAIL\n"
        echo "$out" | sed 's/^/      /'
        fail=$((fail + 1))
        failures+=("003-data: rc=$rc")
    fi
}
# }}}

pipeline_output_check
compile_pipeline_check
encap_input_only_file_check
encap_output_only_file_check
encap_recursive_file_check
refs_unit_tests
refs_fork_on_live_check
legacy_data_tests
# }}}

# {{{ parser_tests() — issue 232: unit tests for langs/<lang>/parser.js
# Run Node's built-in test runner over each per-language parser test
# file and fold the result into the surrounding pass/fail counters.
# Each .mjs file is one logical "test" from this script's perspective;
# Node prints its own subtest breakdown on failure for the diagnostic.
parser_tests() {
    if ! command -v node >/dev/null 2>&1; then
        printf "  %-44s SKIP — node not installed\n" "parser tests (lua, bash)"
        return
    fi
    for tf in "$DIR/tests/232-lua-parser-test.mjs" \
              "$DIR/tests/233-bash-parser-test.mjs" \
              "$DIR/tests/234-language-spec-js-test.mjs"; do
        local label
        label=$(basename "$tf")
        printf "  %-44s " "$label"
        local out
        out=$(SORAMECH_DIR="$DIR" node --test "$tf" 2>&1)
        local rc=$?
        if [[ $rc -eq 0 ]]; then
            printf "ok\n"
            pass=$((pass + 1))
        else
            printf "FAIL\n"
            echo "$out" | sed 's/^/      /'
            fail=$((fail + 1))
            failures+=("$label: node --test exit=$rc")
        fi
    done
}
parser_tests
# }}}

echo
echo "  $pass passed, $fail failed"

if [[ $fail -gt 0 ]]; then
    echo
    echo "  failures:"
    for f in "${failures[@]}"; do
        echo "    - $f"
    done
fi

exit $fail
