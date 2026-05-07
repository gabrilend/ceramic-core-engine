#!/usr/bin/env bash
# Runs a phase demo for SoraMech.
# Usage: ./demo.sh [phase-number]
# If no phase is given, prompts for one.

DIR="/mnt/mtwo/programs/sora/soramech"

PHASES=1

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
else
    echo "demo.sh: unknown phase '${PHASE}' (valid: 1-${PHASES})" >&2
    exit 1
fi
