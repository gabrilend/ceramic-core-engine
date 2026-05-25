#!/bin/bash
# issues/completed/demos/phase-3-thread-pool-fanout/run.sh
#
# What it does, in CEO terms: runs the phase-3 demo map, then
# reads the JSONL transcript and prints a short story of what
# happened — the seed value, each language worker's output, the
# files they wrote, and a short concurrency / latency block from
# the per-task timing the transcript carries. The demo shows the
# phase-3 thread pool fanning a single seed value to three
# concurrent workers (one Lua, one C, one Bash), each tagging the
# value and writing to its own file.
#
# Convention: hard-coded ${DIR} with --dir override, every path
# inside resolved against ${DIR}. No interactive input.

set -u

# {{{ defaults
DIR="/mnt/mtwo/programs/sora/soramech"
if [[ $# -ge 1 ]] && [[ "$1" == "--dir" ]]; then
    DIR="$2"
fi
MAP="$DIR/issues/completed/demos/phase-3-thread-pool-fanout"
LOG="/tmp/soramech-last-run.jsonl"
OUT_LUA="/tmp/soramech-phase-3-lua.txt"
OUT_C="/tmp/soramech-phase-3-c.txt"
OUT_BASH="/tmp/soramech-phase-3-bash.txt"
# }}}

# {{{ sanity-check the runner binary exists
if [[ ! -x "$DIR/soramech-pool" ]]; then
    echo "phase-3 demo: pool runner not built at $DIR/soramech-pool" >&2
    echo "  run 'make' in $DIR first" >&2
    exit 1
fi
# }}}

# {{{ clean previous outputs so the demo's writes are fresh
rm -f "$OUT_LUA" "$OUT_C" "$OUT_BASH"
# }}}

# {{{ run the map with verbose task logging
echo "=== Phase 3 demo — thread-pool fan-out across three languages ==="
echo
printf "Seed value (from boxes/seed.json):  "
grep -o '"value":[[:space:]]*"[^"]*"' "$MAP/boxes/seed.json" \
    | head -1 \
    | sed 's/"value":[[:space:]]*"\([^"]*\)"/\1/'
echo
echo "--- running the map (LOG_VALUES on) ---"
SORAMECH_LOG_VALUES=1 "$DIR/soramech-pool" "$MAP" 2>&1 \
    | grep -E "soramech-pool: |→ " \
    | sed 's/^/  /'
echo
# }}}

# {{{ extract_field — pull one quoted-string field from a JSONL line
extract_field() {
    local line="$1"
    local field="$2"
    echo "$line" | grep -oE "\"$field\":\"[^\"]*\"" | head -1 \
        | sed "s/\"$field\":\"\\([^\"]*\\)\"/\\1/"
}
# }}}

# {{{ extract_num — pull one numeric field from a JSONL line
extract_num() {
    local line="$1"
    local field="$2"
    echo "$line" | grep -oE "\"$field\":[0-9]+" | head -1 \
        | sed "s/\"$field\"://"
}
# }}}

# {{{ story_from_jsonl — narrate what happened from the transcript
story_from_jsonl() {
    if [[ ! -f "$LOG" ]]; then
        echo "  (no JSONL transcript at $LOG — did the runner write it?)"
        return
    fi

    echo "--- worker outputs (from task_output events) ---"
    # Tasks 1+ are the language workers; task 0 is the fan box,
    # task indices for the write boxes follow.
    grep '"event":"task_output"' "$LOG" \
        | while IFS= read -r line; do
            local data
            data=$(echo "$line" | grep -oE '"data":"[^"]*"' \
                                  | head -1 \
                                  | sed 's/"data":"\(.*\)"/\1/')
            printf "  · %s\n" "$data"
          done
    echo

    echo "--- files written to disk ---"
    for f in "$OUT_LUA" "$OUT_C" "$OUT_BASH"; do
        if [[ -f "$f" ]]; then
            printf "  %s\n     %s\n" "$f" "$(cat "$f")"
        else
            printf "  %s   (missing)\n" "$f"
        fi
    done
    echo

    echo "--- concurrency and timing ---"
    local n_tasks duration_us workers
    n_tasks=$(grep -c '"event":"task_end"' "$LOG")
    duration_us=$(grep '"event":"run_end"' "$LOG" \
                  | grep -oE '"duration_us":[0-9]+' \
                  | head -1 | sed 's/"duration_us"://')
    workers=$(grep '"event":"task_end"' "$LOG" \
              | grep -oE '"worker_idx":[0-9]+' \
              | sort -u | wc -l)
    printf "  tasks fired:           %d\n" "$n_tasks"
    printf "  distinct workers used: %d (out of the pool's worker count)\n" "$workers"
    printf "  total run duration:    %s µs\n" "${duration_us:-?}"
    echo
    echo "  per-task durations (µs):"
    grep '"event":"task_end"' "$LOG" \
        | while IFS= read -r line; do
            local tid dur
            tid=$(extract_num "$line" "task_id")
            dur=$(extract_num "$line" "duration_us")
            printf "    task %s: %s µs\n" "$tid" "$dur"
          done
    echo
}
story_from_jsonl
# }}}

# {{{ goodbye
echo "Full JSONL transcript: $LOG"
echo "Re-run with:           $0"
echo
# }}}
