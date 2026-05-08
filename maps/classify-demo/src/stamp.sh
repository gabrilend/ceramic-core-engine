#!/usr/bin/env bash
# Bash utilities for classify-demo. stamp() appends an ISO timestamp
# to a greeting string and returns a single JSON string for the SoraMech runner.

# {{{ stamp
stamp() {
    local text_json="${1}"
    # strip surrounding JSON double-quotes from the string argument
    local text="${text_json%\"}"
    text="${text#\"}"
    local ts
    ts=$(date +%Y-%m-%dT%H:%M:%S)
    local result="${text} [${ts}]"
    # output a single JSON string; assumes no " or \ in result
    printf '"%s"\n' "${result}"
}
# }}}
