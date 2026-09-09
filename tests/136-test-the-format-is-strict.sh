#!/usr/bin/env bash
# 136-test-the-format-is-strict.sh — the map reader stops guessing (issue 601c).
#
# What this proves: three places where the reader used to accept
# something it should refuse, or refuse something it should accept, now
# behave. All three are the same kind of mistake — the reader deciding
# for itself what somebody probably meant — which is why they are one
# test.
#
# What each failure would mean:
#   - a bare word accepted        -> two spellings for one value, next to
#                                    a third that means something else
#   - a long line split silently  -> every line number after it is wrong,
#                                    and the value was cut in half
#   - a multi-line value refused  -> a struct with several fields cannot
#                                    be written down readably
#   - a '#' inside a string lost  -> the comment scanner does not know
#                                    what a string is
#   - a bare argument refused     -> somebody has to quote twice, once
#                                    for the shell and once for us
#
# The loader is compiled here rather than borrowed from another test,
# because what is being proven is what the *reader* does with a file,
# and the smallest program that reads a file is the one that says so.
#
# Scratch lives in the executable RAM tier under /tmp, because the maps
# written here are compiled at load time and /dev/shm is noexec.
#
# Usage: 136-test-the-format-is-strict.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
CC="${CC:-cc}"
WORK="/tmp/$(basename "${DIR}")/strict-format"

rm -rf "${WORK}"
mkdir -p "${WORK}"

fail() { echo "  FAIL: $*"; exit 1; }

# {{{ the loader
# Two modes, because the format has two readers: a file written down,
# and a command line handed over. Both end up in the same text-to-bytes
# routine and the whole point is that they answer differently.
cat > "${WORK}/reader.c" <<'EOF'
#include "cera.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    cera_map_t *m = cera_map_load_file(argv[1], 2);
    if (argc > 2) {
        cera_pool_submitter_register(m->pool);
        const char *no = cera_map_bring_up(m);
        if (no) { fprintf(stderr, "bring-up: %s\n", no); return 1; }
        cera_pool_release(m->pool);
        no = cera_map_deliver_command_line(m, argc - 1, argv + 1);
        cera_pool_submitter_unregister(m->pool);
        if (no) { fprintf(stderr, "arguments: %s\n", no); return 1; }
        printf("arguments accepted\n");
        return 0;
    }
    cera_map_dump(m, stdout);
    return 0;
}
EOF

"${CC}" -std=gnu11 -Wall -g -O2 -pthread \
    -I"${DIR}/src" \
    -DCERA_CC='"'"${CC}"'"' \
    -DCERA_GENERATOR='"'"${DIR}"'/tmp/build/generate"' \
    -DCERA_ROOT='"'"${DIR}"'"' \
    -DCERA_INCLUDE='"'"${DIR}"'/src"' \
    -DCERA_RAM_SHARED='"/dev/shm/'"$(basename "${DIR}")"'"' \
    -DCERA_RAM_EXEC='"/tmp/'"$(basename "${DIR}")"'"' \
    -o "${WORK}/reader" "${WORK}/reader.c" \
    "${DIR}/src/cera.c" "${DIR}/src/generated/emitted.c" \
    -Wl,--dynamic-list="${DIR}/src/098-engine-surface.syms" \
    || fail "the loader would not compile"
# }}}

# {{{ a struct value spread over four lines
printf 'station a triple_sum p\n  out 0 - 0$\n  in 0 = {\n      1.5,\n      2.5,\n      3.5\n  }\n' \
    > "${WORK}/multi.map"

GOT=$("${WORK}/reader" "${WORK}/multi.map" 2>&1)
echo "${GOT}" | grep -q 'in 0 = { 1.5, 2.5, 3.5 }' \
    || fail "a struct over four lines did not load as one value: ${GOT}"
echo "  a struct value spread over four lines is one value"
# }}}

# {{{ a bare word where a string belongs
printf 'station a read_int_file p\n  out 0 - 0$\n  in 0 = fire\n' > "${WORK}/bare.map"

GOT=$("${WORK}/reader" "${WORK}/bare.map" 2>&1)
[ $? -eq 0 ] && fail "a bare word was accepted where a string belongs"
echo "${GOT}" | grep -q 'quoted' \
    || fail "the refusal does not say the value should be quoted: ${GOT}"
echo "${GOT}" | grep -q -- '- fire' \
    || fail "the refusal does not name the wire spelling it collides with: ${GOT}"
echo "  a bare word is refused, naming the spelling it collides with"
# }}}

# {{{ the quoted form still works, and a '#' inside it survives
printf 'station a read_int_file p\n  out 0 - 0$\n  in 0 = "a#b.txt"   # a real comment\n' \
    > "${WORK}/hash.map"

GOT=$("${WORK}/reader" "${WORK}/hash.map" 2>&1)
echo "${GOT}" | grep -q 'in 0 = "a#b.txt"' \
    || fail "a '#' inside a string was taken for a comment: ${GOT}"
echo "  a '#' inside a string is a character, not a comment"
# }}}

# {{{ a physical line longer than the reader's buffer
{
    printf 'station a read_int_file p\n  out 0 - 0$\n  in 0 = "'
    awk 'BEGIN { for (i = 0; i < 1100; i++) printf "x" }'
    printf '"\n'
} > "${WORK}/toolong.map"

GOT=$("${WORK}/reader" "${WORK}/toolong.map" 2>&1)
[ $? -eq 0 ] && fail "an over-long line was accepted"
echo "${GOT}" | grep -q 'longer than' \
    || fail "the refusal does not say the line was too long: ${GOT}"
echo "${GOT}" | grep -q ':3:' \
    || fail "the refusal names the wrong line: ${GOT}"
echo "  a line longer than the buffer is refused, not split"
# }}}

# {{{ a brace that never closes
printf 'station a triple_sum p\n  out 0 - 0$\n  in 0 = {\n      1.5,\n      2.5,\n' \
    > "${WORK}/unclosed.map"

GOT=$("${WORK}/reader" "${WORK}/unclosed.map" 2>&1)
[ $? -eq 0 ] && fail "an unclosed brace was accepted"
echo "${GOT}" | grep -q "opened here" \
    || fail "the refusal does not say where the brace opened: ${GOT}"
echo "${GOT}" | grep -q ':3:' \
    || fail "the refusal names a line other than where the brace opened: ${GOT}"
echo "  an unclosed brace is refused, naming the line that opened it"
# }}}

# {{{ a command line still hands over bare text
# The shell already decided where this value started and stopped, so
# asking for quotes here would be asking somebody to quote twice.
printf 'station a read_int_file p\n  in 0 - 0$\nstation b keep p\n  out 0 - 0$\n' \
    > "${WORK}/argument.map"

GOT=$("${WORK}/reader" "${WORK}/argument.map" /etc/hostname 2>&1)
[ $? -eq 0 ] || fail "a bare command-line string was refused: ${GOT}"
echo "${GOT}" | grep -q 'arguments accepted' \
    || fail "the argument did not reach the port: ${GOT}"
echo "  a command-line string is taken as handed over, without quotes"
# }}}

rm -rf "${WORK}"
exit 0
