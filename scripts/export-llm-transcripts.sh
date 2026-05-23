#!/bin/bash
# scripts/export-llm-transcripts.sh — capture this project's Claude
# conversation history at every verbosity level.
#
# What it does, in CEO terms: every conversation Claude has had about
# this project is stored as a JSONL file under ~/.claude/projects/.
# An external tool can render those JSONL files into human-readable
# markdown at six different levels of detail (terse code-only to raw
# tool-result firehose). This script invokes that external tool once
# per verbosity level and parks the rendered markdown under
# llm-transcripts/ so the project repo carries its own development
# history alongside the code.
#
# Intended use: run as part of the commit ritual — refreshes the
# tracked transcripts so each commit captures the state of project
# history at that moment.
#
# Per the project convention: scripts run from any directory via a
# hard-coded ${DIR} path with an override argument; all paths are
# relative to ${DIR}.

set -u

DIR="/mnt/mtwo/programs/sora/soramech"
if [[ $# -ge 1 ]]; then
    DIR="$1"
fi

# {{{ External exporter location and verbosity table
EXPORTER="/home/ritz/programming/ai-stuff/scripts/claude-conversation-exporter.sh"

# Verbosity levels offered by the external tool. Each produces a
# different lens on the same conversations:
#   v0 minimal  — code + essentials, useful for "what was actually written"
#   v1 compact  — drops user sentiments
#   v2 standard — the default, everything readable
#   v3 verbose  — adds context-file expansions
#   v4 complete — adds LLM execution detail + vimfolds
#   v5 raw      — every intermediate step and tool result
declare -A LEVELS=(
    [0]="minimal"
    [1]="compact"
    [2]="standard"
    [3]="verbose"
    [4]="complete"
    [5]="raw"
)
# }}}

# {{{ Sanity checks
if [[ ! -x "$EXPORTER" ]]; then
    echo "error: external exporter not found at $EXPORTER" >&2
    exit 1
fi

if [[ ! -d "$DIR" ]]; then
    echo "error: project directory not found: $DIR" >&2
    exit 1
fi
# }}}

# {{{ Output directory
OUT_DIR="$DIR/llm-transcripts"
mkdir -p "$OUT_DIR"
# }}}

# {{{ Run the exporter once per verbosity level
echo "Exporting LLM transcripts for $DIR"
echo "  → $OUT_DIR/"

for level in 0 1 2 3 4 5; do
    name="${LEVELS[$level]}"
    out_file="$OUT_DIR/v${level}-${name}.md"
    printf "  v%d %-10s " "$level" "$name"

    if "$EXPORTER" "-v$level" "$DIR" all > "$out_file" 2>/dev/null; then
        size=$(wc -c < "$out_file" 2>/dev/null || echo "?")
        printf "ok (%s bytes)\n" "$size"
    else
        rc=$?
        printf "FAIL (exit=%d)\n" "$rc"
    fi
done
# }}}

echo "Done."
