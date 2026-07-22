#!/usr/bin/env bash
# Establishes SoraMech's two-tier RAM-backed temp layout, idempotently.
# In CEO terms: the project keeps all scratch files in memory, not on disk.
# This script builds the doorways: tmp/ in the project root leads to the
# machine's executable scratch area, and tmp/shared-memory/ inside it leads
# to guaranteed-RAM storage for logs, builds, and other artifacts.
# Run: scripts/ensure-tmp.sh [project-dir]
#
# Layout established:
#   <project>/tmp            -> /tmp/soramech          (exec-capable scratch)
#   <project>/tmp/shared-memory -> /dev/shm/soramech   (logs + artifacts, RAM)
#
# If tmp/ exists as a plain directory (the pre-symlink state), its contents
# are moved into the RAM artifact tier — loudly, file by file — before the
# directory is replaced with the symlink. Nothing is ever silently dropped.

# {{{ --help — render this script's header doc block and exit
case "${1:-}" in
    -h|--help)
        sed -n '2,/^$/s/^# \?//p' "$0"
        exit 0
        ;;
esac
# }}}

DIR="${1:-/mnt/mtwo/programs/sora/soramech}"

EXEC_TIER="/tmp/soramech"
RAM_TIER="/dev/shm/soramech"

mkdir -p "${EXEC_TIER}"
mkdir -p "${RAM_TIER}"

# {{{ guard — tmp/ already a symlink somewhere unexpected
# A symlink to a different target means someone wired a different layout
# on purpose; overriding it silently would destroy that intent. Error and
# let the human decide.
if [ -L "${DIR}/tmp" ]; then
    CURRENT_TARGET=$(readlink "${DIR}/tmp")
    if [ "${CURRENT_TARGET}" != "${EXEC_TIER}" ]; then
        echo "ensure-tmp: ${DIR}/tmp already links to ${CURRENT_TARGET}, expected ${EXEC_TIER} — refusing to change it" >&2
        exit 1
    fi
fi
# }}}

# {{{ migrate — a plain tmp/ directory predates the symlink scheme
# Its files are logs and artifacts written to spinning disk by accident;
# they belong in the RAM tier. Move each one, announcing it, then replace
# the emptied directory with the symlink. rmdir (not rm -rf) so anything
# unexpected left behind stops the script instead of being destroyed.
if [ -d "${DIR}/tmp" ] && [ ! -L "${DIR}/tmp" ]; then
    for f in "${DIR}/tmp/"* "${DIR}/tmp/".*; do
        base=$(basename "${f}")
        [ "${base}" = "." ] && continue
        [ "${base}" = ".." ] && continue
        [ -e "${f}" ] || continue
        echo "ensure-tmp: migrating trapped file to RAM tier: ${base} -> ${RAM_TIER}/"
        mv "${f}" "${RAM_TIER}/${base}"
    done
    rmdir "${DIR}/tmp" || {
        echo "ensure-tmp: ${DIR}/tmp still not empty after migration — refusing to replace it" >&2
        exit 1
    }
    echo "ensure-tmp: replaced plain tmp/ directory with symlink"
fi
# }}}

if [ ! -e "${DIR}/tmp" ]; then
    ln -s "${EXEC_TIER}" "${DIR}/tmp"
    echo "ensure-tmp: linked ${DIR}/tmp -> ${EXEC_TIER}"
fi

# shared-memory lives inside the exec tier so every path can be spelled
# relative to the project root: tmp/shared-memory/<artifact>
if [ ! -e "${EXEC_TIER}/shared-memory" ]; then
    ln -s "${RAM_TIER}" "${EXEC_TIER}/shared-memory"
    echo "ensure-tmp: linked tmp/shared-memory -> ${RAM_TIER}"
fi
