#!/usr/bin/env bash
# Utility functions for driver-test map.

# {{{ stringify
# Receives a JSON-encoded number, returns "stringified: N" as a single JSON string.
# Bash driver contract: output must be a single JSON value.
stringify() {
    local value="${1}"
    local text="stringified: ${value}"
    printf '"%s"\n' "${text}"
}
# }}}
