#!/usr/bin/env bash
# Runs a phase demo for SoraMech.
# Usage: ./demo.sh [phase-number]
# If no phase is given, prompts for one.

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
    luajit "${DIR}/soramech-runner.lua" "${DIR}/maps/classify-demo"
    echo ""
    echo "output:"
    cat "${DIR}/maps/classify-demo/data/output.json"
    echo ""
    echo "run log:"
    cat "${DIR}/maps/classify-demo/tmp/last-run.json"
elif [ "${PHASE}" = "3" ]; then
    # Phase 3 demo — runtime planner. A Lua box uses
    # soramech.create_box and soramech.connect inside its invoke
    # to spawn three downstream worker boxes mid-run, then fans
    # its own output value into each of them in parallel. The
    # JSONL transcript captures the runtime mutations
    # (box_create, wire_add, slot_alloc) and the four resulting
    # tasks. The demo's own run.sh extracts the story from the
    # transcript and prints it.
    "${DIR}/issues/completed/demos/phase-3-runtime-planner/run.sh" --dir "${DIR}"
else
    echo "demo.sh: unknown phase '${PHASE}' (valid: 1-${PHASES})" >&2
    exit 1
fi
