#!/bin/bash
# langs/bash/bash-server.sh — persistent bash subprocess, one per
# worker thread, that talks to the C spec over a Unix-domain
# socketpair connected to stdin/stdout.
#
# Designed in issue 308. Replaces the fork+exec-per-call model
# with a long-lived process: process startup happens once at
# pool init, every box invocation reuses the same bash state
# (and the sourced module cache).
#
# Protocol — line-oriented, all values UTF-8:
#
#   Request:
#     <n_args>\n            (or "QUIT" to terminate)
#     <file_path>\n
#     <fn_name>\n
#     <arg1>\n              (n_args of these)
#     <arg2>\n
#     ...
#
#   Response:
#     <exit_status>\n        (0 on success)
#     <output_length>\n      (decimal byte count, no newline at end of payload)
#     <output_bytes>         (output_length bytes, NOT followed by \n)
#
# Limitations of this iteration:
#   - Arguments cannot contain literal newlines (line-oriented protocol).
#   - Output is read as bytes — newlines inside the payload are fine,
#     but trailing newlines from bash builtins like `echo` are part
#     of the output. Box functions use `printf '%s' "$x"` to avoid them.
#
# Caching: each box file is sourced on first use and remembered in
# the `sourced` associative array; subsequent invocations skip the
# `source` and reuse the loaded functions. Per-worker, no locking.

set -u

declare -A sourced

while IFS= read -r n_args; do
    [[ "$n_args" == "QUIT" ]] && break

    IFS= read -r file_path
    IFS= read -r fn_name

    args=()
    for ((i = 0; i < n_args; i++)); do
        IFS= read -r a
        args+=("$a")
    done

    if [[ ! -f "$file_path" ]]; then
        printf '127\n0\n'
        continue
    fi

    if [[ -z "${sourced[$file_path]:-}" ]]; then
        # shellcheck disable=SC1090
        if ! source "$file_path" 2>/dev/null; then
            printf '126\n0\n'
            continue
        fi
        sourced[$file_path]=1
    fi

    # Capture function output and exit status. The function name
    # itself failing (not a callable) surfaces as exit 127.
    if ! declare -F "$fn_name" >/dev/null 2>&1; then
        printf '127\n0\n'
        continue
    fi

    out=$("$fn_name" "${args[@]}" 2>/dev/null)
    status=$?
    n=${#out}
    printf '%d\n%d\n' "$status" "$n"
    printf '%s' "$out"
done
