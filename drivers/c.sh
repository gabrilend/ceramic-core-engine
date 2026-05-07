#!/usr/bin/env bash
# C language driver for SoraMech.
# Compiles the .c file if the source is newer than the cached binary,
# then invokes the binary with arguments. The binary must write a JSON
# array to stdout per the driver contract.
# Contract: <script> <file-path> <fn-name> <arg-count> [<arg> ...]
# The fn-name is passed as argv[1] so the binary can dispatch to the right function.
# Cache location: TMP_DIR/cache/<md5-of-absolute-path>

DIR="/mnt/mtwo/programs/sora/soramech"

FILE_PATH="${1}"
FN_NAME="${2}"
ARG_COUNT="${3}"
shift 3

if [ -z "${FILE_PATH}" ] || [ -z "${FN_NAME}" ]; then
    echo "c.sh: missing file-path or fn-name" >&2
    exit 1
fi

if [ ! -f "${FILE_PATH}" ]; then
    echo "c.sh: file not found: ${FILE_PATH}" >&2
    exit 1
fi

# resolve to absolute path for stable cache key
ABS_PATH=$(realpath "${FILE_PATH}")

# the map's tmp/cache dir lives relative to the file; walk up to find it
# convention: src/ is one level below the map root, cache is in tmp/cache/
MAP_ROOT=$(dirname "$(dirname "${ABS_PATH}")")
CACHE_DIR="${MAP_ROOT}/tmp/cache"
mkdir -p "${CACHE_DIR}"

# cache key: md5 of the absolute source path (not contents — path is stable per binary)
CACHE_KEY=$(echo -n "${ABS_PATH}" | md5sum | cut -d' ' -f1)
BINARY="${CACHE_DIR}/${CACHE_KEY}"

# compile if binary is missing or source is newer than the binary
NEEDS_COMPILE=0
if [ ! -f "${BINARY}" ]; then
    NEEDS_COMPILE=1
elif [ "${ABS_PATH}" -nt "${BINARY}" ]; then
    NEEDS_COMPILE=1
fi

if [ "${NEEDS_COMPILE}" -eq 1 ]; then
    CC="${CC:-gcc}"
    CFLAGS="${CFLAGS:--O2 -Wall}"
    "${CC}" ${CFLAGS} -o "${BINARY}" "${ABS_PATH}" 2>&1
    if [ $? -ne 0 ]; then
        echo "c.sh: compilation failed for ${FILE_PATH}" >&2
        exit 1
    fi
fi

# invoke binary: fn-name as first arg, then the caller's args
"${BINARY}" "${FN_NAME}" "$@"
