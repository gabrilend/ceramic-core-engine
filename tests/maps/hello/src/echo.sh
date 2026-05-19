# Fixture bash functions for the 308 test.
#
# Each function reads its arguments from positional $1, $2, ... and
# writes its result to stdout (no trailing newline if possible).

# {{{ echo
echo_arg() {
    printf '%s' "$1"
}
# }}}

# {{{ concat
concat() {
    printf '%s%s' "$1" "$2"
}
# }}}

# {{{ shout
shout() {
    printf '%s!' "$1"
}
# }}}
