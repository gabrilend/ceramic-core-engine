#!/bin/bash
# scripts/soramech-compile.sh — produce a self-contained compiled map.
#
# What it does, in CEO terms: takes a SoraMech map directory, walks
# every box file to find the languages and source files it uses,
# copies everything into a `compiled/` subdirectory alongside the
# language spec.so plugins and the pool runner binary, and stamps a
# manifest. The resulting directory is portable — copy or symlink it
# anywhere on the same machine and it runs the map standalone.
#
# Layout produced under <map>/compiled/:
#   meta.json, boxes/, src/, data/  — verbatim copies from the map
#   langs/<lang>/spec.so            — copies of the language plugins
#                                     that the map actually uses
#   langs/bash/bash-server.sh       — only if bash boxes are present
#   bin/<box>.so                    — pre-compiled C boxes (informational;
#                                     the runner still lazy-compiles
#                                     from src/ but having them here
#                                     means you can verify the compile
#                                     step worked without running)
#   pool-runner                     — symlink to soramech-pool
#   manifest.json                   — summary stats
#
# Run as:
#   scripts/soramech-compile.sh <map-dir>
#   scripts/soramech-compile.sh <map-dir> --dir <project-root>
#
# Convention: scripts run from any directory via a hard-coded ${DIR}
# path with an override argument; all paths relative to ${DIR}.

set -u

# {{{ defaults
DIR="/mnt/mtwo/programs/sora/soramech"
MAP=""
# }}}

# {{{ argv parse — map first, --dir <path> optional
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dir)
            DIR="$2"
            shift 2
            ;;
        --help|-h)
            grep '^#' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            if [[ -z "$MAP" ]]; then
                MAP="$1"
            else
                echo "soramech-compile: unexpected argument '$1'" >&2
                exit 2
            fi
            shift
            ;;
    esac
done

if [[ -z "$MAP" ]]; then
    echo "usage: soramech-compile <map-dir> [--dir <project-root>]" >&2
    exit 2
fi
# }}}

# {{{ resolve paths & sanity-check
if [[ ! -d "$MAP" ]]; then
    echo "soramech-compile: not a directory: $MAP" >&2
    exit 1
fi
MAP=$(cd "$MAP" && pwd)
if [[ ! -f "$MAP/meta.json" ]]; then
    echo "soramech-compile: missing meta.json in $MAP" >&2
    exit 1
fi
if [[ ! -d "$MAP/boxes" ]]; then
    echo "soramech-compile: missing boxes/ in $MAP" >&2
    exit 1
fi
if [[ ! -x "$DIR/soramech-pool" ]]; then
    echo "soramech-compile: pool runner not built at $DIR/soramech-pool" >&2
    echo "  run 'make' in $DIR first" >&2
    exit 1
fi

COMPILED="$MAP/compiled"
# }}}

# {{{ box_field — pull a top-level string field from a box JSON file
# Tolerant of whitespace; returns empty string if the field is missing.
# We don't have jq in-tree, so a regex over the small box files is
# fine. Box JSON is hand-written and shallow.
box_field() {
    local file="$1"
    local field="$2"
    grep -o "\"$field\"[[:space:]]*:[[:space:]]*\"[^\"]*\"" "$file" \
        | head -n1 \
        | sed 's/.*"\([^"]*\)"$/\1/'
}
# }}}

# {{{ wipe & lay out the compiled directory
rm -rf "$COMPILED"
mkdir -p "$COMPILED" "$COMPILED/boxes" "$COMPILED/src" "$COMPILED/bin" "$COMPILED/langs"
# }}}

# {{{ copy verbatim assets (meta, boxes, src, data, *.txt)
cp "$MAP/meta.json" "$COMPILED/meta.json"
cp -r "$MAP/boxes/." "$COMPILED/boxes/"

if [[ -d "$MAP/src" ]]; then
    cp -r "$MAP/src/." "$COMPILED/src/"
fi

if [[ -d "$MAP/data" ]]; then
    mkdir -p "$COMPILED/data"
    cp -r "$MAP/data/." "$COMPILED/data/"
fi

# Top-level data files (input.txt, names.txt, etc) — the runner
# resolves file_write/data refs relative to the map root.
shopt -s nullglob
for f in "$MAP"/*.txt; do
    cp "$f" "$COMPILED/"
done
shopt -u nullglob
# }}}

# {{{ enumerate the set of languages the map actually uses
# A box file is in compiled/boxes/. We read `lang` from each.
LANGS=()
for box in "$COMPILED"/boxes/*.json; do
    lang=$(box_field "$box" lang)
    if [[ -z "$lang" ]]; then continue; fi
    seen=0
    for known in "${LANGS[@]+"${LANGS[@]}"}"; do
        if [[ "$known" == "$lang" ]]; then seen=1; break; fi
    done
    if [[ $seen -eq 0 ]]; then
        LANGS+=("$lang")
    fi
done
# }}}

# {{{ copy spec.so for each used language
for lang in "${LANGS[@]+"${LANGS[@]}"}"; do
    src_spec="$DIR/langs/$lang/spec.so"
    if [[ ! -f "$src_spec" ]]; then
        echo "soramech-compile: missing spec.so for language '$lang'" >&2
        echo "  expected at $src_spec — run 'make' in $DIR" >&2
        exit 1
    fi
    mkdir -p "$COMPILED/langs/$lang"
    cp "$src_spec" "$COMPILED/langs/$lang/spec.so"
    # Bash spec needs its companion server script for runtime.
    if [[ "$lang" == "bash" ]] && [[ -f "$DIR/langs/bash/bash-server.sh" ]]; then
        cp "$DIR/langs/bash/bash-server.sh" "$COMPILED/langs/bash/"
    fi
done
# }}}

# {{{ pre-compile C boxes into compiled/bin/
# The runner already lazy-compiles .c → .so on first invoke, so this
# step is purely informational: it surfaces compile errors at compile
# time rather than at run time, and produces artifacts the user can
# inspect or distribute alongside the source.
n_c_compiled=0
n_c_failed=0
for box in "$COMPILED"/boxes/*.json; do
    box_lang=$(box_field "$box" lang)
    if [[ "$box_lang" != "c" ]]; then continue; fi
    ref=$(box_field "$box" ref)
    if [[ -z "$ref" ]]; then continue; fi
    src_path="$COMPILED/$ref"
    if [[ ! -f "$src_path" ]]; then
        echo "soramech-compile: C box source missing: $ref" >&2
        n_c_failed=$((n_c_failed + 1))
        continue
    fi
    base=$(basename "$ref" .c)
    out="$COMPILED/bin/$base.so"
    if ! gcc -shared -fPIC -O2 -Wall -o "$out" "$src_path" 2>/tmp/soramech-compile-cc.log; then
        echo "soramech-compile: gcc failed for $ref:" >&2
        cat /tmp/soramech-compile-cc.log >&2
        n_c_failed=$((n_c_failed + 1))
        continue
    fi
    n_c_compiled=$((n_c_compiled + 1))
done

if [[ $n_c_failed -gt 0 ]]; then
    echo "soramech-compile: $n_c_failed C box(es) failed to compile" >&2
    exit 1
fi
# }}}

# {{{ copy the pool runner into the compiled directory
# A real copy (not a symlink) is intentional: the runner discovers
# its sibling langs/ via /proc/self/exe, which follows symlinks to
# their canonical target. A symlinked pool-runner would resolve back
# to the project tree and pick up the project's langs/ instead of
# the bundled compiled/langs/. Copying keeps the compiled artifact
# self-describing and portable.
cp "$DIR/soramech-pool" "$COMPILED/pool-runner"
chmod +x "$COMPILED/pool-runner"
# }}}

# {{{ write manifest.json
# Hand-rolled JSON (no jq) keeps the script self-contained. The
# fields are stable enough to grep against in tests.
n_boxes=$(ls -1 "$COMPILED"/boxes/*.json 2>/dev/null | wc -l)
n_langs=${#LANGS[@]}
langs_json=""
first=1
for lang in "${LANGS[@]+"${LANGS[@]}"}"; do
    if [[ $first -eq 1 ]]; then
        langs_json="\"$lang\""
        first=0
    else
        langs_json+=",\"$lang\""
    fi
done
compiled_at=$(date -Iseconds 2>/dev/null || date)
map_name=$(basename "$MAP")

cat > "$COMPILED/manifest.json" <<EOF
{
    "name": "$map_name",
    "compiled_at": "$compiled_at",
    "n_boxes": $n_boxes,
    "n_languages": $n_langs,
    "languages": [$langs_json],
    "c_boxes_precompiled": $n_c_compiled,
    "source_map_dir": "$MAP"
}
EOF
# }}}

# {{{ summary line
echo "compiled $map_name → $COMPILED"
echo "  boxes:       $n_boxes"
echo "  languages:   $n_langs (${LANGS[*]+${LANGS[*]}})"
echo "  c precompiled: $n_c_compiled"
echo
echo "run with:"
echo "  $COMPILED/pool-runner $COMPILED"
# }}}
