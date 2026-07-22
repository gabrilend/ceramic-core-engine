#!/bin/bash
# scripts/export-llm-transcripts.sh — capture this project's Claude
# conversation history at six verbosity levels.
#
# Thin wrapper. All work happens in the sister Lua script. Kept as
# a .sh so the project's other CLI entry points (run-tests.sh,
# soramech-compile.sh, etc.) stay shape-consistent.
#
# What it does, in CEO terms: every conversation Claude has had
# about this project is stored as a JSONL file under
# ~/.claude/projects/<encoded-path>/. This script renders each
# file into six markdown views — code-only, prompts-only,
# standard, verbose, complete, and raw — and parks them under
# llm-transcripts/v{N}-{name}/. Incremental: a conversation whose
# JSONL hasn't changed since the last export is skipped.
#
# Per the project convention: scripts run from any directory via
# a hard-coded ${DIR} path with an override argument; all paths
# are relative to ${DIR}.

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

exec luajit "$DIR/scripts/export-llm-transcripts.lua" "$DIR"
