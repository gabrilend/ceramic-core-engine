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

set -u

DIR="/mnt/mtwo/programs/sora/soramech"
if [[ $# -ge 1 ]]; then
    DIR="$1"
fi

cd "$DIR"

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
    output=$("$DIR/soramech-pool" "$DIR/tests/maps/$map" 2>&1)
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

# {{{ pipeline_output_check() — additional: the pipeline writes to disk
pipeline_output_check() {
    local file="/tmp/soramech-pipeline-out.txt"
    rm -f "$file"
    "$DIR/soramech-pool" "$DIR/tests/maps/pipeline" > /dev/null 2>&1
    printf "  %-44s " "pipeline file_write output"
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
    local file="/tmp/soramech-pipeline-out.txt"

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
    "who → World" \
    "greet → Hello, World!"

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

check_map "pipeline" \
    "input → 5" \
    "double → 10" \
    "addone → 11" \
    "shout → 11!"

pipeline_output_check
compile_pipeline_check
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
