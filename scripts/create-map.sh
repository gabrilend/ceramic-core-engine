#!/usr/bin/env bash
# Creates a new SoraMech map directory with all required files and a tmp/ symlink.
# The map directory is created under ${DIR}/maps/<name>.
# Run: scripts/create-map.sh <map-name> [maps-root]

DIR="/mnt/mtwo/programs/sora/soramech"

MAP_NAME="${1}"
MAPS_ROOT="${2:-${DIR}/maps}"

if [ -z "${MAP_NAME}" ]; then
    echo "usage: create-map.sh <map-name> [maps-root]" >&2
    exit 1
fi

MAP_DIR="${MAPS_ROOT}/${MAP_NAME}"

if [ -d "${MAP_DIR}" ]; then
    echo "error: map directory already exists: ${MAP_DIR}" >&2
    exit 1
fi

# create directory structure
mkdir -p "${MAP_DIR}/boxes"
mkdir -p "${MAP_DIR}/data"
mkdir -p "${MAP_DIR}/src"
mkdir -p "${MAP_DIR}/drivers"

# create /tmp target before symlinking — logs/ and cache/ live there at runtime
TMP_TARGET="/tmp/soramech-${MAP_NAME}"
mkdir -p "${TMP_TARGET}"

# tmp/ is a symlink to /tmp/soramech-<name>/ so ephemeral files stay in RAM
ln -s "${TMP_TARGET}" "${MAP_DIR}/tmp"

# write meta.json with placeholders
cat > "${MAP_DIR}/meta.json" << METAJSON
{
  "name": "${MAP_NAME}",
  "description": "",
  "entry_box_id": ""
}
METAJSON

# write drivers.json pointing at the built-in driver scripts in the soramech root
cat > "${MAP_DIR}/drivers.json" << DRIVERSJSON
{
  ".lua": "${DIR}/drivers/lua.sh",
  ".c":   "${DIR}/drivers/c.sh",
  ".sh":  "${DIR}/drivers/bash.sh"
}
DRIVERSJSON

echo "created: ${MAP_DIR}"
echo "next: edit meta.json to set entry_box_id, then add boxes in ${MAP_DIR}/boxes/"
