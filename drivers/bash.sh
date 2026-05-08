#!/usr/bin/env bash
# Bash language driver for SoraMech.
# Sources the target .sh file, calls the named function with args as
# positional parameters. The function must echo a single JSON value to
# stdout. Arguments are passed as plain strings; JSON decode/encode is
# the function's responsibility.
# Contract: <script> <file-path> <fn-name> <arg-count> [<arg> ...]

DIR="/mnt/mtwo/programs/sora/soramech"

FILE_PATH="${1}"
FN_NAME="${2}"
ARG_COUNT="${3}"
shift 3

if [ -z "${FILE_PATH}" ] || [ -z "${FN_NAME}" ]; then
    echo "bash.sh: missing file-path or fn-name" >&2
    exit 1
fi

if [ ! -f "${FILE_PATH}" ]; then
    echo "bash.sh: file not found: ${FILE_PATH}" >&2
    exit 1
fi

# source the file to load the function into this shell
# shellcheck disable=SC1090
source "${FILE_PATH}"

if ! declare -f "${FN_NAME}" > /dev/null 2>&1; then
    echo "bash.sh: no function '${FN_NAME}' defined in ${FILE_PATH}" >&2
    exit 1
fi

# call the function with remaining args as positional parameters
"${FN_NAME}" "$@"
