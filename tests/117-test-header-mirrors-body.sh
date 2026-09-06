#!/usr/bin/env bash
# 117-test-header-mirrors-body.sh — cera.h says things in cera.c's order.
#
# What this proves: the two files carry the same components in the same
# order, and inside a component the header declares calls in the order
# the body defines them. Read either one and the other is a map of it.
#
# Why it needs checking rather than intending. The order is invisible to
# the compiler — a header whose declarations are shuffled compiles
# exactly as well — so nothing but this would notice it drifting, and it
# drifts one added function at a time.
#
# What a failure means: a call was declared somewhere other than where
# its definition sits. Move the declaration, not the definition; the
# body's order is the one with a reason behind it.
#
# Usage: 117-test-header-mirrors-body.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
WORK="/dev/shm/$(basename "${DIR}")/mirror"
mkdir -p "${WORK}"

# The order a name is declared in the header, one per line, with the
# component it sits in. The header folds only at the component level — a
# declaration is one line and a fold around it would hide nothing — so
# the declarations themselves are what is read. A struct's fields are
# indented and a declaration is not, which is the whole discriminator.
# A static inline is skipped on both sides: it is defined where it is
# declared, so there is no second place for it to disagree with.
awk '
  /^\/\* \{\{\{ [0-9]{3} — / { comp = $0; sub(/^\/\* \{\{\{ /, "", comp); sub(/ \*\/$/, "", comp); next }
  /^static/ { next }
  /^[A-Za-z_]/ {
      if (match($0, /cera_[a-z0-9_]+[ \t]*\(/)) {
          name = substr($0, RSTART, RLENGTH)
          sub(/[ \t]*\($/, "", name)
          if (comp != "" && !(name in seen)) { seen[name] = 1; print comp "\t" name }
      }
  }
' "${DIR}/src/cera.h" > "${WORK}/header.txt"

# The order a name is defined in the body, same shape. A definition is a
# line in column one that does not end in a semicolon, so forward
# declarations do not count, and is not static, so the engine's private
# calls are not expected in a header that exists to exclude them.
awk '
  /^\/\* \{\{\{ [0-9]{3} — / { comp = $0; sub(/^\/\* \{\{\{ /, "", comp); sub(/ \*\/$/, "", comp); next }
  /^static/ { next }
  /^[A-Za-z_].*\(/ && !/;[ \t]*$/ {
      if (match($0, /cera_[a-z0-9_]+[ \t]*\(/)) {
          name = substr($0, RSTART, RLENGTH)
          sub(/[ \t]*\($/, "", name)
          if (comp != "" && !(name in seen)) { seen[name] = 1; print comp "\t" name }
      }
  }
' "${DIR}/src/cera.c" > "${WORK}/body.txt"

if diff -u "${WORK}/body.txt" "${WORK}/header.txt" > "${WORK}/drift.txt"; then
    echo "  the header declares $(wc -l < "${WORK}/header.txt") calls in the order the body defines them"
    exit 0
fi

echo "  the header and the body disagree about where calls live:"
sed -n '4,24p' "${WORK}/drift.txt" | sed 's/^/    /'
echo
echo "  A line only in the body is declared somewhere else in the header;"
echo "  a line only in the header is defined somewhere else in the body."
echo "  Move the declaration — the body's order is the one with a reason."
exit 1
