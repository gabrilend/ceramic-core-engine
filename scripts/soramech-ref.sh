#!/bin/bash
# scripts/soramech-ref.sh — reference-counted compiled-artifact pins.
#
# What it does, in CEO terms: gives other programs a way to say
# "I'm using this compiled SoraMech artifact right now, don't
# clobber it." Holders acquire a reference at startup, release at
# shutdown, and the compile pipeline (scripts/soramech-compile.sh)
# checks the count before rebuilding — if anyone is holding,
# rebuild lands in a sibling directory instead of overwriting.
#
# The mechanism is a single dot-file (`.refs`) inside the compiled
# directory:
#
#   acquire <id> <iso8601-date> <pid> <pid-start-time-ticks> <marker>
#   release <id> <iso8601-date>
#
# Append-only writes are atomic on POSIX as long as the line fits
# in PIPE_BUF (4 KB on Linux); these lines are well under that.
# So no lock is needed for steady-state acquire / release — only
# `reap` (which rewrites the log to drop dead entries) takes a
# lock, via .refs.lock.
#
# References are *expected to be unreliable*: a crashing holder
# never gets to release. The back pointer (PID + start-time-ticks
# + optional marker path) lets `list` and `reap` independently
# verify whether a recorded acquire is still meaningful.
#
# Run as:
#   scripts/soramech-ref.sh acquire <compiled-dir> [--id <id>] [--marker <path>]
#   scripts/soramech-ref.sh release <compiled-dir> --id <id>
#   scripts/soramech-ref.sh list    <compiled-dir>          # JSON
#   scripts/soramech-ref.sh count   <compiled-dir>          # live count, integer
#   scripts/soramech-ref.sh reap    <compiled-dir>          # compact + drop dead
#
# Convention: scripts run from any directory via a hard-coded
# ${DIR} path with an override argument; all paths inside the
# script are absolute against ${DIR} or against the per-call
# compiled-dir argument.

set -u

# {{{ defaults
DIR="/mnt/mtwo/programs/sora/soramech"
ACTION=""
TARGET=""
ID=""
MARKER="-"
# }}}

# {{{ --help — render this script's header doc block and exit
# Placed ahead of the arg-count check below so a lone -h/--help
# prints help instead of tripping the "needs 2 args" usage error.
case "${1:-}" in
    -h|--help)
        sed -n '2,/^$/s/^# \?//p' "$0"
        exit 0
        ;;
esac
# }}}

# {{{ argv parse — action first, target second, then --flags
if [[ $# -lt 2 ]]; then
    echo "usage: soramech-ref.sh <acquire|release|list|count|reap> <compiled-dir> [--id <id>] [--marker <path>] [--dir <project-root>]" >&2
    exit 2
fi
ACTION="$1"
TARGET="$2"
shift 2
while [[ $# -gt 0 ]]; do
    case "$1" in
        --id)     ID="$2";     shift 2 ;;
        --marker) MARKER="$2"; shift 2 ;;
        --dir)    DIR="$2";    shift 2 ;;
        *)
            echo "soramech-ref: unexpected argument '$1'" >&2
            exit 2
            ;;
    esac
done
# }}}

# {{{ sanity-check the target directory
if [[ ! -d "$TARGET" ]]; then
    echo "soramech-ref: not a directory: $TARGET" >&2
    exit 1
fi
TARGET=$(cd "$TARGET" && pwd)
REFS_FILE="$TARGET/.refs"
LOCK_FILE="$TARGET/.refs.lock"
# }}}

# {{{ pid_starttime — read /proc/<pid>/stat field 22 (start time in jiffies)
# stat field 2 (comm) is parenthesized and may contain spaces, so we
# strip up to and including the closing paren before counting. After
# the paren, field 22 overall becomes the 20th field.
pid_starttime() {
    local pid="$1"
    if [[ ! -r "/proc/$pid/stat" ]]; then return 1; fi
    local raw
    raw=$(cat "/proc/$pid/stat" 2>/dev/null) || return 1
    local after_comm="${raw#*) }"
    echo "$after_comm" | awk '{print $20}'
}
# }}}

# {{{ gen_id — timestamp + random suffix, unique enough for this purpose
gen_id() {
    local ts
    ts=$(date +%s)
    local rand
    rand=$(head -c 4 /dev/urandom | od -An -tx1 | tr -d ' \n')
    echo "${ts}-${rand}"
}
# }}}

# {{{ is_alive — true iff the recorded back pointer still validates
# A reference is alive when:
#   - the PID exists, AND
#   - the PID's current start time matches the recorded one (so we
#     don't confuse a recycled PID for the original holder), AND
#   - if a marker path was supplied at acquire, the marker file
#     still exists.
# Any failure means stale.
is_alive() {
    local pid="$1"
    local recorded_start="$2"
    local marker="$3"
    if [[ ! -d "/proc/$pid" ]]; then return 1; fi
    local current_start
    current_start=$(pid_starttime "$pid") || return 1
    if [[ "$current_start" != "$recorded_start" ]]; then return 1; fi
    if [[ "$marker" != "-" ]] && [[ ! -e "$marker" ]]; then return 1; fi
    return 0
}
# }}}

# {{{ scan_refs — emit one line per live id: "<id> <date> <pid> <start> <marker>"
# Walks the log once, accumulates acquires, subtracts releases, prints
# the remaining acquire records. The id-set is in a temp file rather
# than a bash assoc array so the script stays portable across older
# bash versions; the log is small enough that an O(n) scan is fine.
scan_refs() {
    if [[ ! -f "$REFS_FILE" ]]; then return 0; fi
    local tmp_acq
    tmp_acq=$(mktemp)
    local tmp_rel
    tmp_rel=$(mktemp)
    while IFS= read -r line; do
        case "$line" in
            "acquire "*) echo "$line" >> "$tmp_acq" ;;
            "release "*)
                local rid
                rid=$(echo "$line" | awk '{print $2}')
                echo "$rid" >> "$tmp_rel"
                ;;
        esac
    done < "$REFS_FILE"
    # Print only acquires whose id is not present in the release set.
    while IFS= read -r aline; do
        local aid
        aid=$(echo "$aline" | awk '{print $2}')
        if ! grep -qFx "$aid" "$tmp_rel" 2>/dev/null; then
            local rest
            rest=$(echo "$aline" | cut -d' ' -f2-)
            echo "$rest"
        fi
    done < "$tmp_acq"
    rm -f "$tmp_acq" "$tmp_rel"
}
# }}}

# {{{ do_acquire — append one acquire record, print the assigned id
do_acquire() {
    if [[ -z "$ID" ]]; then
        ID=$(gen_id)
    fi
    local date
    date=$(date -Iseconds)
    local pid="$$"
    # The script's own PID isn't useful as a back pointer — the
    # *holder* process is whoever invoked us. Bash makes the parent
    # PID available via PPID, which is the right anchor.
    pid="$PPID"
    local start
    start=$(pid_starttime "$pid")
    if [[ -z "$start" ]]; then
        echo "soramech-ref: cannot read /proc/$pid/stat (caller's start time)" >&2
        exit 1
    fi
    # O_APPEND on POSIX makes the write atomic up to PIPE_BUF.
    printf 'acquire %s %s %s %s %s\n' "$ID" "$date" "$pid" "$start" "$MARKER" >> "$REFS_FILE"
    echo "$ID"
}
# }}}

# {{{ do_release — append one release record
do_release() {
    if [[ -z "$ID" ]]; then
        echo "soramech-ref: release requires --id <id>" >&2
        exit 2
    fi
    local date
    date=$(date -Iseconds)
    printf 'release %s %s\n' "$ID" "$date" >> "$REFS_FILE"
}
# }}}

# {{{ do_list — JSON array of {id, date, pid, start_ticks, marker, valid}
do_list() {
    echo "["
    local first=1
    while IFS=' ' read -r id date pid start marker; do
        if [[ -z "$id" ]]; then continue; fi
        local valid="true"
        if ! is_alive "$pid" "$start" "$marker"; then
            valid="false"
        fi
        local comma=","
        if [[ $first -eq 1 ]]; then
            comma=""
            first=0
        fi
        printf '%s\n  {"id":"%s","date":"%s","pid":%s,"start_ticks":%s,"marker":"%s","valid":%s}' \
            "$comma" "$id" "$date" "$pid" "$start" "$marker" "$valid"
    done < <(scan_refs)
    echo
    echo "]"
}
# }}}

# {{{ do_count — number of live (un-released, validated) references
# Used by the compile script to decide whether to fork. Prints a single
# integer so the caller can compare without parsing JSON.
do_count() {
    local n=0
    while IFS=' ' read -r id date pid start marker; do
        if [[ -z "$id" ]]; then continue; fi
        if is_alive "$pid" "$start" "$marker"; then
            n=$((n + 1))
        fi
    done < <(scan_refs)
    echo "$n"
}
# }}}

# {{{ do_reap — rewrite the log, dropping released and dead entries
# Takes .refs.lock for the duration so a concurrent acquire / release
# doesn't get spliced into a half-rewritten log. Uses mkdir as the
# portable atomic test-and-set (no flock dependency).
do_reap() {
    local i=0
    while ! mkdir "$LOCK_FILE" 2>/dev/null; do
        i=$((i + 1))
        if [[ $i -gt 100 ]]; then
            echo "soramech-ref: could not acquire $LOCK_FILE after 100 tries" >&2
            exit 1
        fi
        sleep 0.05
    done
    trap 'rmdir "$LOCK_FILE" 2>/dev/null' EXIT
    local tmp
    tmp=$(mktemp)
    while IFS=' ' read -r id date pid start marker; do
        if [[ -z "$id" ]]; then continue; fi
        if is_alive "$pid" "$start" "$marker"; then
            printf 'acquire %s %s %s %s %s\n' "$id" "$date" "$pid" "$start" "$marker" >> "$tmp"
        fi
    done < <(scan_refs)
    if [[ -s "$tmp" ]]; then
        mv "$tmp" "$REFS_FILE"
    else
        rm -f "$REFS_FILE" "$tmp"
    fi
}
# }}}

# {{{ dispatch
case "$ACTION" in
    acquire) do_acquire ;;
    release) do_release ;;
    list)    do_list    ;;
    count)   do_count   ;;
    reap)    do_reap    ;;
    *)
        echo "soramech-ref: unknown action '$ACTION'" >&2
        exit 2
        ;;
esac
# }}}
