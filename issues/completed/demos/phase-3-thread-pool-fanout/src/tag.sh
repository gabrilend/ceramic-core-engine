# src/tag.sh — Bash worker that prefixes its input with a
# language tag and the input length, matching the Lua and C
# workers so the three outputs read as a triple.
#
# Sourced once per worker by the bash spec; subsequent
# invocations call tag_bash() directly without re-sourcing.

# {{{ tag_bash
tag_bash() {
    local v="$1"
    local n="${#v}"
    printf "bash:   %s  (%s bytes)" "$v" "$n"
}
# }}}
