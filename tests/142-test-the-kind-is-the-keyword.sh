#!/usr/bin/env bash
# 142-test-the-kind-is-the-keyword.sh — a station line reads at a glance
# (issue 608).
#
# What this proves: the kind of a station is the first word of its line
# and the box function is in brackets. Five spellings parse, the dump
# writes one of them, and the shape a map used to have — a trailing
# letter and a bare box name — is refused by name rather than
# reinterpreted.
#
# What each failure would mean:
#   - a short spelling refused   -> a person typing a map by hand pays
#                                   the long word on every line
#   - a dump writing a short one -> two spellings on disk for one kind
#   - a trailing letter accepted -> two grammars, and a file that means
#                                   something different from how it reads
#   - a bare box name accepted   -> the brackets are decoration, and the
#                                   one field that is code still looks
#                                   like every other word on the line
#   - an unclosed bracket taken  -> a station line could run past its
#                                   own end
#
# The loader is compiled here rather than borrowed, for the same reason
# the strict-format test compiles its own: what is under test is what
# the *reader* does with a file.
#
# Scratch lives in the executable RAM tier under /tmp, because the maps
# written here are compiled at load time and /dev/shm is noexec.
#
# Usage: 142-test-the-kind-is-the-keyword.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
CC="${CC:-cc}"
WORK="/tmp/$(basename "${DIR}")/kind-keyword"

rm -rf "${WORK}"
mkdir -p "${WORK}"

fail() { echo "  FAIL: $*"; exit 1; }

# {{{ the loader — reads a map and writes it back out
cat > "${WORK}/reader.c" <<'EOF'
#include "cera.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;
    cera_map_t *m = cera_map_load_file(argv[1], 2);
    cera_map_dump(m, stdout);
    return 0;
}
EOF

"${CC}" -std=gnu11 -Wall -g -O2 -pthread \
    -I"${DIR}/src" \
    -DCERA_COMPILER='"'"${DIR}"'/tmp/build/serac"' \
    -DCERA_ROOT='"'"${DIR}"'"' \
    -DCERA_RAM_SHARED='"/dev/shm/'"$(basename "${DIR}")"'"' \
    -DCERA_RAM_EXEC='"/tmp/'"$(basename "${DIR}")"'"' \
    -o "${WORK}/reader" "${WORK}/reader.c" \
    "${DIR}/src/cera.c" "${DIR}/src/generated/emitted.c" \
    -Wl,--dynamic-list="${DIR}/src/098-engine-surface.syms" \
    || fail "the loader would not compile"
# }}}

# {{{ all five spellings parse, and the dump writes the long ones
# `comp` and `iter` cost a person nothing to type and cost a reader
# nothing to recognise. The dump writing only the long forms is what
# keeps one spelling per kind on disk.
{
    printf 'station plainly (double_it)\n  in 0 - 0$\n  out 0 - short_comp.0\n'
    printf 'comp short_comp (keep)\n  in 0 - plainly.0\n  in 1 = 5\n'
    printf '  out 0 - long_comp.0\n'
    printf 'comparator long_comp (keep)\n  in 0 - short_comp.0\n  in 1 = 5\n'
    printf '  out 0 - short_iter.0\n'
    printf 'iter short_iter (keep)\n  in 0 - long_comp.0\n  out 0 - long_iter.0\n'
    printf 'iterator long_iter (keep)\n  in 0 - short_iter.0\n  out 0 - 0$\n'
} > "${WORK}/spellings.map"

GOT=$("${WORK}/reader" "${WORK}/spellings.map" 2>&1) \
    || fail "a map using every kind spelling did not load: ${GOT}"

echo "${GOT}" | grep -q '^station plainly (double_it)' \
    || fail "a plain station did not come back as one: ${GOT}"
echo "${GOT}" | grep -q '^comparator short_comp (keep)' \
    || fail "'comp' did not place a comparator, or the dump wrote the short form: ${GOT}"
echo "${GOT}" | grep -q '^comparator long_comp (keep)' \
    || fail "'comparator' did not place a comparator: ${GOT}"
echo "${GOT}" | grep -q '^iterator short_iter (keep)' \
    || fail "'iter' did not place an iterator, or the dump wrote the short form: ${GOT}"
echo "${GOT}" | grep -q '^iterator long_iter (keep)' \
    || fail "'iterator' did not place an iterator: ${GOT}"
echo "${GOT}" | grep -q ' comp \| iter ' \
    && fail "the dump wrote a short spelling: ${GOT}"
echo "  five spellings parse; the dump writes station, comparator, iterator"
# }}}

# {{{ and it round-trips
# A dump that reads back into the same text is the whole claim, and the
# station line is the line this issue changed.
"${WORK}/reader" "${WORK}/spellings.map" > "${WORK}/once.map" 2>/dev/null
"${WORK}/reader" "${WORK}/once.map" > "${WORK}/twice.map" 2>/dev/null
cmp -s "${WORK}/once.map" "${WORK}/twice.map" \
    || fail "dumping a dump did not yield the dump"
echo "  and dumping a dump yields the dump"
# }}}

# {{{ an iterator's position still rides on the line
printf 'iterator spread (keep)\n  in 0 - 0$\n  out 0 - 0$\n  out 1 - 1$\n' \
    > "${WORK}/cursor.map"
GOT=$("${WORK}/reader" "${WORK}/cursor.map" 2>&1) \
    || fail "an iterator with two exits did not load: ${GOT}"

printf 'station notaniter (keep) @2\n  in 0 - 0$\n  out 0 - 0$\n' \
    > "${WORK}/badcursor.map"
GOT=$("${WORK}/reader" "${WORK}/badcursor.map" 2>&1)
[ $? -eq 0 ] && fail "'@' was accepted on a station that takes no exits in turn"
echo "${GOT}" | grep -q "iterator" \
    || fail "the refusal does not name the keyword that would allow it: ${GOT}"
echo "  '@' on a plain station is refused, naming the keyword that takes one"
# }}}

# {{{ the old form is refused, and the refusal says what replaced it
# Every map written before this change has a trailing letter on every
# station line. "Unexpected word" would send its author hunting for a
# typo they did not make.
printf 'station a keep p\n  in 0 - 0$\n  out 0 - 0$\n' > "${WORK}/letter.map"

GOT=$("${WORK}/reader" "${WORK}/letter.map" 2>&1)
[ $? -eq 0 ] && fail "the old trailing-letter form was accepted"
echo "${GOT}" | grep -q "first word" \
    || fail "the refusal does not say the kind moved to the front: ${GOT}"
echo "${GOT}" | grep -q "comparator" \
    || fail "the refusal does not name what to write instead: ${GOT}"
echo "  a trailing p, c or i is refused, naming what replaced it"
# }}}

# {{{ a bare box name is refused
printf 'station a keep\n  in 0 - 0$\n  out 0 - 0$\n' > "${WORK}/bare.map"

GOT=$("${WORK}/reader" "${WORK}/bare.map" 2>&1)
[ $? -eq 0 ] && fail "a box function without brackets was accepted"
echo "${GOT}" | grep -q "brackets" \
    || fail "the refusal does not say the box wants brackets: ${GOT}"
echo "  a box function without brackets is refused"
# }}}

# {{{ a bracket that never closes
printf 'station a (keep\n  in 0 - 0$\n  out 0 - 0$\n' > "${WORK}/unclosed.map"

GOT=$("${WORK}/reader" "${WORK}/unclosed.map" 2>&1)
[ $? -eq 0 ] && fail "an unclosed bracket was accepted"
echo "${GOT}" | grep -q "does not close" \
    || fail "the refusal does not say the bracket never closed: ${GOT}"
echo "${GOT}" | grep -q ':1:' \
    || fail "the refusal names a line other than the one that opened it: ${GOT}"
echo "  an unclosed bracket is refused, naming its line"
# }}}

# {{{ and nothing between the brackets
printf 'station a ()\n  in 0 - 0$\n  out 0 - 0$\n' > "${WORK}/empty.map"

GOT=$("${WORK}/reader" "${WORK}/empty.map" 2>&1)
[ $? -eq 0 ] && fail "empty brackets were accepted where a box belongs"
echo "${GOT}" | grep -q "nothing between" \
    || fail "the refusal does not say the brackets are empty: ${GOT}"
echo "  empty brackets are refused"
# }}}

rm -rf "${WORK}"
