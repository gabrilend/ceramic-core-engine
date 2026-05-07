#!/usr/bin/env bash
# Utility functions for driver-test map.

# {{{ stringify
# Receives a JSON-encoded number, returns "stringified: N" as a JSON array.
# Bash driver contract: output must be a JSON array ["<value>"].
stringify() {
    local value="${1}"
    local text="stringified: ${value}"
    printf '["%s"]\n' "${text}"
}
# }}}
