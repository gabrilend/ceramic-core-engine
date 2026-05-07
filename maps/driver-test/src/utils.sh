#!/usr/bin/env bash
# Utility functions for driver-test map.

# {{{ stringify
stringify() {
    local value="${1}"
    # wrap the value as a JSON string for the next box
    echo "\"${value}\""
}
# }}}
