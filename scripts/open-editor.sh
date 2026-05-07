#!/usr/bin/env bash
# Opens the SoraMech visual editor in Firefox.
# Run: scripts/open-editor.sh [server-url]
# Default server URL: http://localhost:7700
# The editor is served by the HTTP server, so start-server.sh must be running first.

DIR="/mnt/mtwo/programs/sora/soramech"

SERVER_URL="${1:-http://localhost:7700}"

if ! command -v firefox > /dev/null 2>&1; then
    echo "open-editor.sh: firefox not found on PATH" >&2
    exit 1
fi

echo "Opening SoraMech editor at ${SERVER_URL}"
firefox "${SERVER_URL}" &
