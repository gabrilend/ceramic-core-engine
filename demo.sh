#!/usr/bin/env bash
# Runs a phase demo for SoraMech.
# Usage: ./demo.sh [phase-number]
# If no phase is given, prompts for one.

# {{{ --help — render this script's header doc block and exit
case "${1:-}" in
    -h|--help)
        sed -n '2,/^$/s/^# \?//p' "$0"
        exit 0
        ;;
esac
# }}}

DIR="/mnt/mtwo/programs/sora/soramech"

PHASES=3

if [ -n "${1}" ]; then
    PHASE="${1}"
else
    printf "Enter phase number (1-%d): " "${PHASES}"
    read -r PHASE
fi

if [ "${PHASE}" = "1" ]; then
    echo "--- Phase 1 demo: classify-demo ---"
    echo "input:"
    cat "${DIR}/maps/classify-demo/data/input.json"
    echo ""
    # The interpreter entry point moved into src/ during the
    # entry-point cleanup (issue 220); the old root-level
    # soramech-runner.lua no longer exists.
    luajit "${DIR}/src/007-runner-main.lua" "${DIR}/maps/classify-demo"
    echo ""
    echo "output:"
    cat "${DIR}/maps/classify-demo/data/output.json"
    echo ""
    echo "run log:"
    cat "${DIR}/maps/classify-demo/tmp/last-run.json"
elif [ "${PHASE}" = "3" ]; then
    # Phase 3 demo — thread-pool fan-out. A single seed value
    # fans through one Lua producer to three same-time tag
    # workers (Lua, C, and Bash), each writing its result to its
    # own output file. Showcases the phase-3 C thread pool,
    # multi-language dispatch, and the JSONL transcript layer.
    # The demo's own run.sh extracts the story from the
    # transcript and prints it.
    "${DIR}/issues/completed/demos/phase-3-thread-pool-fanout/run.sh" --dir "${DIR}"
else
    echo "demo.sh: unknown phase '${PHASE}' (valid: 1-${PHASES})" >&2
    exit 1
fi
