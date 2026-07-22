#!/usr/bin/env bash
# Starts the SoraMech HTTP server, serving the maps directory on localhost.
# Run: scripts/start-server.sh [port]
# Default port: 7700. Maps root is always ${DIR}/maps.

# {{{ --help — render this script's header doc block and exit
case "${1:-}" in
    -h|--help)
        sed -n '2,/^$/s/^# \?//p' "$0"
        exit 0
        ;;
esac
# }}}

DIR="/mnt/mtwo/programs/sora/soramech"

# Make sure the RAM-backed tmp/ tier exists before the server runs —
# logs and other ephemeral output belong there, never on disk.
"${DIR}/scripts/ensure-tmp.sh" "${DIR}" >/dev/null

PORT="${1:-7700}"

echo "SoraMech server starting on http://localhost:${PORT}"
echo "Maps root: ${DIR}/maps"
echo "Press Ctrl-C to stop."
echo ""

luajit "${DIR}/src/006-server-main.lua" "${DIR}/maps" "${PORT}"
