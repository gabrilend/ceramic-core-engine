#!/usr/bin/env bash
# 138-test-both-ends.sh — every wire is written twice (issue 601a).
#
# What this proves: a map file declares each wire on both the producing
# and the receiving station, the loader refuses when the two disagree,
# and a dumped map still reloads.
#
# Why the disagreements are tested separately. "The two ends do not
# match" is one condition and four different mistakes, and somebody
# fixing a map wants to be told which they made:
#   - an arrow with no receiving end   -> a station that does not admit
#                                         to being fed
#   - a receiving end with no arrow    -> a wire nobody draws
#   - the two naming different ports   -> a typo in one of them
#   - a source naming nobody           -> the mirror of an arrow into
#                                         the void, which has no other
#                                         check to fall to
#
# The round trip is the scene that would be easiest to leave broken: a
# dump that wrote only the arrows would produce a file its own reader
# refuses, and nothing else here would notice.
#
# Usage: 138-test-both-ends.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
CC="${CC:-cc}"
WORK="/tmp/$(basename "${DIR}")/both-ends"

rm -rf "${WORK}"
mkdir -p "${WORK}"

fail() { echo "  FAIL: $*"; exit 1; }

# {{{ the loader
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

# {{{ both ends written, and fan-in among them
# Two stations feed one port. Fan-in has always worked; what is new is
# that the receiving station says so twice, once per wire.
cat > "${WORK}/whole.map" <<'MAP'
station left seven p
  out 0 - sink.0

station right seven p
  out 0 - sink.0

station sink keep p
  out 0 - 0$
  in 0 - left.0
  in 0 - right.0
MAP

GOT=$("${WORK}/reader" "${WORK}/whole.map" 2>&1)
[ $? -eq 0 ] || fail "a map with both ends written was refused: ${GOT}"
echo "${GOT}" | grep -q 'in 0 - left.0' \
    || fail "the dump lost the first of two wires into one port: ${GOT}"
echo "${GOT}" | grep -q 'in 0 - right.0' \
    || fail "the dump lost the second: ${GOT}"
echo "  two wires into one port, each written at both ends"
# }}}

# {{{ an arrow with no receiving end
cat > "${WORK}/no-in.map" <<'MAP'
station feed seven p
  out 0 - sink.0

station sink keep p
  out 0 - 0$
MAP

GOT=$("${WORK}/reader" "${WORK}/no-in.map" 2>&1)
[ $? -eq 0 ] && fail "an arrow with no receiving end was accepted"
echo "${GOT}" | grep -q "add 'in 0 - feed.0' to station 'sink'" \
    || fail "the refusal does not say what to add, or where: ${GOT}"
echo "  an arrow whose destination does not admit to being fed is refused"
# }}}

# {{{ a receiving end with no arrow
cat > "${WORK}/no-out.map" <<'MAP'
station feed seven p

station sink keep p
  out 0 - 0$
  in 0 - feed.0
MAP

GOT=$("${WORK}/reader" "${WORK}/no-out.map" 2>&1)
[ $? -eq 0 ] && fail "a receiving end with no arrow was accepted"
echo "${GOT}" | grep -q "add 'out 0 - sink.0' to station 'feed'" \
    || fail "the refusal does not name the arrow that is missing: ${GOT}"
echo "  a wire nobody draws is refused from the receiving side"
# }}}

# {{{ the two ends naming different ports
cat > "${WORK}/mismatch.map" <<'MAP'
station feed seven p
  out 0 - sink.0

station sink add p
  out 0 - 0$
  in 1 - feed.0
MAP

GOT=$("${WORK}/reader" "${WORK}/mismatch.map" 2>&1)
[ $? -eq 0 ] && fail "two ends naming different ports were accepted"
echo "${GOT}" | grep -q "both ends" \
    || fail "the refusal does not explain the rule: ${GOT}"
echo "  two ends naming different ports are refused"
# }}}

# {{{ a source that names nobody
cat > "${WORK}/nowhere.map" <<'MAP'
station sink keep p
  out 0 - 0$
  in 0 - ghost.0
MAP

GOT=$("${WORK}/reader" "${WORK}/nowhere.map" 2>&1)
[ $? -eq 0 ] && fail "a source naming a station that does not exist was accepted"
echo "${GOT}" | grep -q "does not declare" \
    || fail "the refusal does not say the station is not there: ${GOT}"
echo "  a source naming a station the map does not declare is refused"
# }}}

# {{{ a dumped map reloads
# The scene that would be easiest to leave broken: a dump writing only
# the arrows produces a file its own reader refuses.
"${WORK}/reader" "${DIR}/maps/127-the-ladder.map" > "${WORK}/dumped.map" \
    || fail "the ladder would not load"
GOT=$("${WORK}/reader" "${WORK}/dumped.map" 2>&1)
[ $? -eq 0 ] || fail "a dumped map did not reload: ${GOT}"

# And again, so that the second dump can be compared with the first:
# a round trip that changed the program would show up as changed text.
echo "${GOT}" > "${WORK}/twice.map"
diff -q "${WORK}/dumped.map" "${WORK}/twice.map" > /dev/null \
    || fail "dumping a dumped map produced different text"
echo "  a dumped map reloads, and dumps again to the same text"
# }}}

rm -rf "${WORK}"
exit 0
