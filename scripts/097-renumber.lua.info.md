# 097-renumber.lua — changing the reading order, from outside

Every file in this project carries an index, and the indices run
across the whole tree rather than per directory, so the project sorts
into one sequence somebody can read end to end. This changes that
sequence.

## Usage

    luajit scripts/097-renumber.lua <order-file> [--apply] [DIR]
    luajit scripts/097-renumber.lua --check [DIR]

| Argument | Meaning |
|---|---|
| `<order-file>` | One path per line, relative to the project root, in the order they should be read. Blank lines and `#` comments ignored. |
| `--apply` | Actually rename and rewrite. Without it nothing changes and the report says what would have happened. |
| `--check` | Report every indexed filename mentioned anywhere that is not a file that exists. Safe to run at any time. |
| `DIR` | Project root, if not the one hard-coded at the top. |

## What it does

**The order is an input, not a computation.** Nothing here sorts,
infers, or groups. Deciding what should be read after what is a
judgement about how the project is best explained; this obeys a list.
That separation is deliberate — the mechanical half should be boring
enough to trust, so the interesting half can be argued about on its
own.

**Every indexed file must appear exactly once.** Missing or duplicated
is refused rather than guessed at, because a file left out would keep
its old number and land wherever that put it, which is the opposite of
an order somebody decided.

**A companion follows its source.** `019-station.c.info.md` is not a
step in the reading order — it is the same step as `019-station.c`, so
it takes whatever number that file takes and is never listed
separately. Letter suffixes are preserved for the same reason: `037a`
is the second half of `037`, not the step after it.

**Renames are staged behind placeholders.** If 019 becomes 021 while
some existing 021 becomes 019, doing them one after another turns the
first into the second. Every old name becomes a private marker, and
only then does every marker become a new name — so nothing is renamed
twice and the order of the list cannot change the result. The files
move through a temporary name for the same reason.

**Every reference is rewritten** — source includes, companion
documents, issues, prose. A rename that misses one leaves a broken
build or a dead link, and both are found late and blamed on something
else.

The issues are deliberately not renumbered: their numbers are a phase
and a sequence within it, which is a different meaning from a reading
order and would be destroyed by sorting on one.

## The check

`--check` reads every text file, picks out anything shaped like an
indexed filename, and asks whether that file exists. Its first run
found two: a header saying the dump lives in `050-dump.c` and rewiring
in `051-rewire.c`, when they are `051` and `052`.

**It reports examples and history too**, and that is not a fault to
fix. An issue describing a rename that already happened mentions the
old name on purpose; a comment illustrating the convention names a
file that never existed. The check hands over a short list for a
person to read rather than a verdict, which is the right shape for
something whose false positives are all somebody being deliberate.
