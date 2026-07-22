# Pair-matrix fixture functions, Bash side (issue 325).

# {{{ produce
produce() {
    printf '%s-p' "$1"
}
# }}}

# {{{ reflect
reflect() {
    printf 'bash-saw:%s' "$1"
}
# }}}
