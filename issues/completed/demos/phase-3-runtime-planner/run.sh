#!/bin/bash
# issues/completed/demos/phase-3-runtime-planner/run.sh
#
# What it does, in CEO terms: runs the phase-3 demo map, then
# reads the JSONL transcript and prints a short story of what
# happened — the input value, the runtime-spawned boxes, each
# worker's distinct output, and a small statistics block at the
# end. Demonstrates phase 3's headline capability (a Lua box
# building more of the graph mid-run) alongside the JSONL
# transcript layer that landed in issue 311.
#
# Convention: hard-coded ${DIR} with --dir override, every path
# inside resolved against ${DIR}. No interactive input.

set -u

# {{{ defaults
DIR="/mnt/mtwo/programs/sora/soramech"
if [[ $# -ge 1 ]] && [[ "$1" == "--dir" ]]; then
    DIR="$2"
fi
MAP="$DIR/issues/completed/demos/phase-3-runtime-planner"
LOG="/tmp/soramech-last-run.jsonl"
# }}}

# {{{ sanity-check the runner binary exists
if [[ ! -x "$DIR/soramech-pool" ]]; then
    echo "phase-3 demo: pool runner not built at $DIR/soramech-pool" >&2
    echo "  run 'make' in $DIR first" >&2
    exit 1
fi
# }}}

# {{{ run the map with full logging
echo "=== Phase 3 demo — runtime planner ==="
echo
printf "Seed value (from boxes/seed.json):  "
grep -o '"value":[[:space:]]*"[^"]*"' "$MAP/boxes/seed.json" | head -1 | sed 's/"value":[[:space:]]*"\([^"]*\)"/\1/'
echo
echo "--- running the map (LOG_VALUES + LOG_SLOTS on) ---"
SORAMECH_LOG_VALUES=1 SORAMECH_LOG_SLOTS=1 "$DIR/soramech-pool" "$MAP" 2>&1 \
    | grep -E "soramech-pool: |→ " \
    | sed 's/^/  /'
echo
# }}}

# {{{ extract_field — pull one field from a JSONL line
# Crude regex extraction because the project's no-jq policy holds
# here too. The JSONL lines are flat objects with primitive
# values; one regex per field is enough for the demo's needs.
extract_field() {
    local line="$1"
    local field="$2"
    echo "$line" | grep -oE "\"$field\":\"[^\"]*\"" | head -1 | sed "s/\"$field\":\"\\([^\"]*\\)\"/\\1/"
}
# }}}

# {{{ story_from_jsonl — narrate what happened from the transcript
story_from_jsonl() {
    if [[ ! -f "$LOG" ]]; then
        echo "  (no JSONL transcript at $LOG — did the runner write it?)"
        return
    fi

    echo "--- runtime graph mutations ---"
    local n_created=0
    while IFS= read -r line; do
        local id  fn  ref
        id=$(extract_field  "$line" "box_id")
        fn=$(extract_field  "$line" "fn")
        ref=$(extract_field "$line" "ref")
        printf "  + created box '%s' running %s() from %s\n" "$id" "$fn" "$ref"
        n_created=$((n_created + 1))
    done < <(grep '"event":"box_create"' "$LOG")
    if [[ $n_created -eq 0 ]]; then
        echo "  (no box_create events — no runtime mutations happened)"
    fi
    echo

    local n_wired=0
    while IFS= read -r line; do
        local from to port
        from=$(extract_field "$line" "from_box")
        to=$(extract_field   "$line" "to_box")
        port=$(extract_field "$line" "to_input")
        printf "  → wired '%s' → '%s'.%s\n" "$from" "$to" "$port"
        n_wired=$((n_wired + 1))
    done < <(grep '"event":"wire_add"' "$LOG")
    echo

    echo "--- worker outputs (from task_output events) ---"
    # task_id 0 is the planner; tasks 1+ are the runtime workers.
    grep '"event":"task_output"' "$LOG" \
        | grep -v '"task_id":0,' \
        | while IFS= read -r line; do
            local data
            data=$(echo "$line" | grep -oE '"data":"[^"]*"' | head -1 | sed 's/"data":"\(.*\)"/\1/')
            printf "  · %s\n" "$data"
        done
    echo

    echo "--- summary ---"
    local n_tasks
    n_tasks=$(grep '"event":"task_end"' "$LOG" | wc -l)
    local n_runtime_slots
    n_runtime_slots=$(grep '"event":"slot_alloc"' "$LOG" | grep -v '<counter>' | grep -v '"box":"seed"' | grep -v '"box":"planner"' | wc -l)
    local duration_us
    duration_us=$(grep '"event":"run_end"' "$LOG" | grep -oE '"duration_us":[0-9]+' | head -1 | sed 's/"duration_us"://')
    printf "  tasks fired:                %d\n" "$n_tasks"
    printf "  runtime-allocated slots:    %d\n" "$n_runtime_slots"
    printf "  runtime-created boxes:      %d\n" "$n_created"
    printf "  runtime-added wires:        %d\n" "$n_wired"
    printf "  total run duration:         %s µs\n" "${duration_us:-?}"
    echo
}
story_from_jsonl
# }}}

# {{{ goodbye
echo "Full JSONL transcript: $LOG"
echo "Re-run with:           $0"
echo
# }}}
