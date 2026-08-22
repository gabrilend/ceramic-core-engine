#!/usr/bin/env bash
#
# 109-dependencies.sh — everything this project needs that is not a C
# compiler, and how to get hold of it.
#
# What this is: the one place that knows what the build wants, whether
# this machine has it, and what to do about it when it does not. Run it
# with no arguments and it tells you the truth and fixes what is safe
# to fix. Run it with an argument and it acts.
#
# How it does it, in general terms: three tools are described in a
# table at the top — the compiler, the scripting language the
# documentation site is generated with, and the linker that turns C
# into a module a browser can run. For each one this script asks three
# questions. Is it on this machine at all? Is there a copy belonging to
# this project? Is either of those older than the version named in the
# table? What happens next follows from the answers, and the rules are
# deliberately not symmetric:
#
#   * A copy this project fetched is this project's to update, and it
#     is updated without asking. Agreeing to a local copy was the
#     decision; keeping it current is the consequence of it.
#
#   * A copy the system provides is never touched, at any version, for
#     any reason — not upgraded, not patched, not moved. It belongs to
#     a package manager, and two things writing to one file is how a
#     machine reaches a state nobody can explain afterwards.
#
#   * Something missing entirely is fetched, and *how* it is fetched
#     depends on what fetching it honestly costs. Where building from
#     source is a minute's work it is built from source. Where building
#     from source would mean building a compiler suite — hours, and
#     gigabytes — this script says so and downloads a prebuilt instead,
#     rather than pretending the two cases are alike.
#
# Running it twice does nothing the second time. That is the property
# worth protecting above the others, because it is what lets this sit
# in front of a build without slowing one down.
#
# Usage:
#   ./scripts/109-dependencies.sh              report, and fix what is safe
#   ./scripts/109-dependencies.sh --check      report only; change nothing
#   ./scripts/109-dependencies.sh --local NAME fetch a project-local copy
#   ./scripts/109-dependencies.sh --local all  ... of everything that can have one
#   ./scripts/109-dependencies.sh --yes        take the offers without asking
#   ./scripts/109-dependencies.sh /some/dir    work on a different project root

set -euo pipefail

# The project root. Hard-coded so the script works from any directory,
# overridable by an argument that names an existing directory — the
# same arrangement the demo launcher uses, for the same reason.
DIR="/mnt/mtwo/programming/ai-playground/minimal-soramech"

# {{{ the table
#
# What the project wants, and where a copy of it would come from. This
# is the only part that should need editing when a version moves on:
# change the wanted version, and every machine's next run notices.
#
# **Two different questions, so two different columns.** The first
# version of this table had one, and it was wrong in a way worth
# recording: it compared what a tool *reports* against what this script
# would *build*, which for LuaJIT meant comparing a version string
# against a commit hash. Those can never be equal, so a perfectly good
# system LuaJIT reported as out of date forever.
#
# PIN is what a local copy is built or downloaded from — an exact
# thing, so that two machines running this script end up with the same
# binary. For LuaJIT that is a commit on the rolling 2.1 branch rather
# than a release number, because that branch is how LuaJIT 2.1 is
# published: there has been no 2.1 tarball in years, and pretending
# otherwise would make "which version" unanswerable.
declare -A PIN=(
    [compiler]="none"
    [luajit]="1ee778a4e37122d8ca7d5733c590a47dafd6b15c"
    [wasm-linker]="33.0"
)

# Whether a copy that already exists is good enough is asked by
# `acceptable_version` further down, one question per tool, because the
# questions are genuinely different shapes: LuaJIT needs to be from the
# 2.1 line, and the WebAssembly linker needs to match the compiler it
# will be linking the output of — a number this script has to go and
# look up rather than one anybody can write in a table.

# What each one is for, in the words a person would use to decide
# whether they care.
declare -A PURPOSE=(
    [compiler]="everything — the engine, the generator, the tests"
    [luajit]="regenerating the documentation site"
    [wasm-linker]="regenerating the workbench's browser module"
)

# Whether the build stops without it. Only the compiler is required:
# the other two regenerate things that are committed in their finished
# form, so somebody who never edits a document or the map format never
# needs either.
declare -A REQUIRED=(
    [compiler]="yes"
    [luajit]="no"
    [wasm-linker]="no"
)

# What fetching one actually costs, which decides whether this script
# does it unprompted. "cheap" is fetched without asking when missing;
# "heavy" is only ever offered, because a couple of hundred megabytes
# arriving because somebody typed `make` is a rude surprise.
declare -A WEIGHT=(
    [compiler]="impossible"
    [luajit]="cheap"
    [wasm-linker]="heavy"
)

# The order they are reported in, since an associative array has none
# and a report whose lines move between runs is hard to read.
ORDER=(compiler luajit wasm-linker)
# }}}

# {{{ where a local copy lives
#
# Everything this script fetches lands under one directory, and that
# directory is ignored by git — it holds downloaded and derived things,
# which the project's standing rule keeps out of history because a
# derived file in a repository is a file that can be stale.
TOOLCHAIN="${DIR}/toolchain"
TOOLBIN="${TOOLCHAIN}/bin"
TOOLSRC="${TOOLCHAIN}/src"

# The manifest: one line per thing this project fetched, saying what
# version it is and how it got here. Without it, "is the local copy out
# of date" could only be answered by running the tool and parsing its
# output, which works for LuaJIT and does not work for a linker that
# reports the version of the suite it was cut from.
MANIFEST="${TOOLCHAIN}/installed"
# }}}

# {{{ say()
# Ordinary output. Everything this script prints goes through one of
# these three so that a caller redirecting output gets a coherent
# stream rather than some lines on one channel and some on another.
say()
{
    printf '%s\n' "$*"
}
# }}}

# {{{ warn()
# Something worth knowing that is not fatal. Warnings go to standard
# error so that a caller reading the report as data still sees them.
warn()
{
    printf 'dependencies: %s\n' "$*" >&2
}
# }}}

# {{{ die()
# Something that stops the run. There is no fallback path anywhere in
# this script: a step that cannot be completed says why and stops,
# because a half-installed toolchain that reports success is worse than
# no toolchain at all.
die()
{
    printf 'dependencies: %s\n' "$*" >&2
    exit 1
}
# }}}

# {{{ manifest_version()
# What the manifest says is installed locally, or nothing at all.
manifest_version()
{
    local name="$1"

    if [[ ! -f "${MANIFEST}" ]]; then
        return 0
    fi

    local line
    line="$(grep -m1 "^${name} " "${MANIFEST}" || true)"

    if [[ -z "${line}" ]]; then
        return 0
    fi

    printf '%s\n' "$(printf '%s' "${line}" | cut -d' ' -f2)"
}
# }}}

# {{{ manifest_record()
# Write down what was just installed, replacing any earlier line for
# the same tool. Recorded after the install succeeds and never before,
# so an interrupted download cannot leave the manifest claiming
# something that is not there.
manifest_record()
{
    local name="$1"
    local version="$2"
    local how="$3"

    mkdir -p "${TOOLCHAIN}"
    touch "${MANIFEST}"

    local kept
    kept="$(grep -v "^${name} " "${MANIFEST}" || true)"

    printf '%s\n' "${kept}" | grep -v '^$' > "${MANIFEST}.new" || true
    printf '%s %s %s\n' "${name}" "${version}" "${how}" >> "${MANIFEST}.new"
    mv "${MANIFEST}.new" "${MANIFEST}"
}
# }}}

# {{{ local_binary()
# The path to this project's own copy, if it has one that runs. Asking
# whether the file is executable rather than whether the manifest
# mentions it means a half-deleted toolchain reports as absent instead
# of as present-and-broken.
local_binary()
{
    local name="$1"

    declare -A BINARY=(
        [luajit]="luajit"
        [wasm-linker]="wasm-ld"
    )

    local file="${BINARY[${name}]:-}"

    if [[ -z "${file}" ]]; then
        return 0
    fi

    if [[ -x "${TOOLBIN}/${file}" ]]; then
        printf '%s\n' "${TOOLBIN}/${file}"
    fi
}
# }}}

# {{{ system_binary()
# The path to the machine's own copy, deliberately ignoring this
# project's directory so that "does the system have one" stays a
# separate question from "do we have one".
system_binary()
{
    local name="$1"

    declare -A CANDIDATES=(
        [compiler]="cc gcc clang"
        [luajit]="luajit"
        [wasm-linker]="wasm-ld wasm-ld18"
    )

    local found=""
    local candidate

    for candidate in ${CANDIDATES[${name}]}; do
        local path
        path="$(command -v "${candidate}" || true)"

        if [[ -n "${path}" && "${path}" != "${TOOLBIN}"/* ]]; then
            found="${path}"
            break
        fi
    done

    printf '%s\n' "${found}"
}
# }}}

# {{{ system_version()
# What version the machine's copy reports, in the crude terms this
# script needs: a number it can compare against the table. Each tool is
# asked in its own way because there is no common answer, which is
# exactly the sort of thing a dispatch table is for.
system_version()
{
    local name="$1"
    local path="$2"

    case "${name}" in
        compiler)
            printf 'present\n'
            ;;
        luajit)
            local text
            text="$("${path}" -v 2>&1 | head -1)"
            printf '%s\n' "$(printf '%s' "${text}" | cut -d' ' -f2)"
            ;;
        wasm-linker)
            # The linker reports the version of the compiler suite it
            # was cut from, and the major number is the part that has
            # to match the compiler building the objects it links —
            # object files carry a format version, and a linker from a
            # different major refuses them rather than guessing.
            local text
            text="$("${path}" --version 2>&1 | head -1)"
            local number
            number="$(printf '%s' "${text}" | grep -o '[0-9][0-9]*\.[0-9][0-9]*' | head -1)"
            printf '%s\n' "${number%%.*}"
            ;;
    esac
}
# }}}

# {{{ clang_major()
# The major version of the clang on this machine, or nothing if there
# is none.
#
# This exists because of one hard fact about the WebAssembly build:
# **gcc cannot target WebAssembly at all**, so that half of the project
# is a clang job regardless of what `cc` points at, and the linker has
# to be the same major version as the clang producing the objects it
# links. Object files carry a format version, and a linker from a
# different major refuses them rather than guessing — which is the
# right behaviour and an baffling error message if you do not know to
# expect it.
clang_major()
{
    local path
    path="$(command -v clang || true)"

    if [[ -z "${path}" ]]; then
        return 0
    fi

    local text
    text="$("${path}" --version 2>&1 | head -1)"
    local number
    number="$(printf '%s' "${text}" | grep -o '[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*' | head -1)"

    printf '%s\n' "${number%%.*}"
}
# }}}

# {{{ acceptable_version()
# Is a copy that already exists good enough? One answer per tool,
# because the question is a different shape for each — which is exactly
# the situation a dispatch belongs in rather than a chain of ifs each
# quietly assuming the last one's terms.
acceptable_version()
{
    local name="$1"
    local found="$2"

    case "${name}" in
        compiler)
            # Any C compiler builds the engine. The project's own
            # standing claim is that the build path needs a C compiler
            # and nothing else, and putting a floor here would quietly
            # make that untrue.
            return 0
            ;;

        luajit)
            # From the 2.1 line. The site generator uses nothing exotic,
            # so the exact build within that line does not matter, and
            # demanding an exact one would mean telling somebody their
            # working LuaJIT is wrong.
            [[ "${found}" == 2.1* ]]
            ;;

        wasm-linker)
            # Same major as clang, for the reason in clang_major above.
            local wanted
            wanted="$(clang_major)"

            if [[ -z "${wanted}" ]]; then
                # No clang means the module cannot be built whatever
                # linker is here, so no linker version is acceptable
                # and saying "yes" would be a lie of omission.
                return 1
            fi

            [[ "${found}" == "${wanted}" ]]
            ;;
    esac
}
# }}}

# {{{ install_luajit()
# Fetch the scripting language the documentation site is generated
# with, and build it. This is the case where building from source is
# the honest answer: it is one tarball, it depends on nothing but a C
# compiler and make, and it takes about as long as reading this
# comment.
install_luajit()
{
    local commit="${PIN[luajit]}"
    local url="https://github.com/LuaJIT/LuaJIT/archive/${commit}.tar.gz"
    local tarball="${TOOLSRC}/luajit-${commit}.tar.gz"
    local tree="${TOOLSRC}/LuaJIT-${commit}"

    mkdir -p "${TOOLSRC}"
    mkdir -p "${TOOLBIN}"

    say "  fetching LuaJIT ${commit:0:12} ..."
    curl -fsSL -o "${tarball}" "${url}" \
        || die "could not download LuaJIT from ${url}"

    # Unpacked fresh every time rather than reused. A source tree left
    # over from an interrupted build is the classic way to get an
    # object file from one version linked into a binary from another,
    # and the tarball is already on disk so re-unpacking costs nothing.
    rm -rf "${tree}"
    tar -xzf "${tarball}" -C "${TOOLSRC}" \
        || die "could not unpack ${tarball}"

    say "  building it ..."
    make -C "${tree}" --quiet PREFIX="${TOOLCHAIN}" \
        || die "LuaJIT did not build; its own output above says why"

    # Installed by copying the one binary rather than by running the
    # project's install target, which would scatter a share/ and an
    # include/ we have no use for and would want a prefix that survives
    # being moved. One file, one place.
    cp "${tree}/src/luajit" "${TOOLBIN}/luajit"

    manifest_record luajit "${commit}" compiled
    say "  LuaJIT is in ${TOOLBIN}/luajit"
}
# }}}

# {{{ install_wasm_linker()
# Fetch a linker that can produce a module a browser will run.
#
# **This one is downloaded rather than built, and the reason is worth
# stating rather than hiding.** This linker is a part of LLVM. Building
# it from source means building LLVM: several gigabytes of C++, an hour
# or more on a fast machine, and a build system with its own
# dependencies. Every other thing this script installs is built from
# source; this is the exception, and it is an exception because the
# alternative is dishonest about what it would cost somebody, not
# because downloading is preferred.
#
# The download is a prebuilt toolchain for compiling C to WebAssembly.
# It carries more than the linker — a compiler and a C library for the
# target as well — and only the linker is kept, because this project
# already has a compiler and supplies its own replacements for the
# library calls the parser makes.
install_wasm_linker()
{
    local release="wasi-sdk-33"
    local version="33.0"
    local tarball="${TOOLSRC}/wasi-sdk-${version}.tar.gz"
    local url="https://github.com/WebAssembly/wasi-sdk/releases/download/${release}/wasi-sdk-${version}-x86_64-linux.tar.gz"

    mkdir -p "${TOOLSRC}"
    mkdir -p "${TOOLBIN}"

    say "  fetching a prebuilt WebAssembly toolchain (about 190 MB) ..."
    curl -fL --progress-bar -o "${tarball}" "${url}" \
        || die "could not download the toolchain from ${url}"

    say "  unpacking it ..."

    # **Unpacked whole, and the first attempt at this cherry-picked one
    # binary out of it.** That looked thrifty and does not work: a
    # compiler finds its own headers by walking up from wherever its
    # binary sits, so a clang lifted out of its tree is a clang that
    # cannot find the definition of `size_t`. The same goes in a
    # quieter way for the linker, which is versioned together with the
    # compiler that produced what it links.
    #
    # **And the compiler is kept as well as the linker, on purpose.**
    # The two have to be the same version: an object file carries a
    # format version and a linker from a different major refuses it.
    # Taking the pair out of one archive makes them the same version by
    # construction, where taking the linker alone would leave it
    # matched against whatever clang this machine happens to have —
    # true today and a strange error message after somebody's next
    # system upgrade.
    rm -rf "${TOOLSRC}/wasi-sdk-${version}-x86_64-linux"
    tar -xzf "${tarball}" -C "${TOOLSRC}" \
        || die "could not unpack ${tarball}"

    local tree="${TOOLSRC}/wasi-sdk-${version}-x86_64-linux"

    if [[ ! -x "${tree}/bin/wasm-ld" ]]; then
        die "no wasm-ld inside ${tree} — the release layout has changed"
    fi

    # Pointed at rather than copied, for the reason above: these two
    # have to keep the tree around them.
    ln -sf "${tree}/bin/wasm-ld" "${TOOLBIN}/wasm-ld"
    ln -sf "${tree}/bin/clang" "${TOOLBIN}/wasm-clang"

    manifest_record wasm-linker "${version}" prebuilt
    say "  the linker is at ${TOOLBIN}/wasm-ld, with its matching compiler beside it"

    # Kept rather than deleted, so that asking for it again after a
    # botched extraction does not mean downloading it again. It sits
    # under the ignored directory, so it is never anybody's to trip
    # over in a status listing.
    say "  the archive stays in ${TOOLSRC} — delete it if you want the space back"
}
# }}}

# {{{ install_compiler()
# There is nothing to do here and there never can be. A C compiler is
# what every other install in this script is built with, including any
# compiler this one might try to fetch, so a machine without one cannot
# be bootstrapped from inside a script that needs one to run anything.
# It exists so that the dispatch below has an entry for every tool
# rather than a special case in the middle of it.
install_compiler()
{
    die "a C compiler cannot be installed by a script that needs one; install one from your package manager"
}
# }}}

# {{{ status_of()
# One word for where a tool stands. The five answers are the whole
# state space, and everything this script decides is decided from one
# of them:
#
#   absent       nowhere on this machine
#   system       the machine has one; this project does not
#   local        this project has one, at the version wanted
#   local-stale  this project has one, at some other version
#   both         both exist; this project's is the one that gets used
status_of()
{
    local name="$1"

    local mine
    mine="$(local_binary "${name}")"
    local theirs
    theirs="$(system_binary "${name}")"

    if [[ -n "${mine}" ]]; then
        local recorded
        recorded="$(manifest_version "${name}")"

        if [[ "${recorded}" != "${PIN[${name}]}" ]]; then
            printf 'local-stale\n'
            return 0
        fi

        if [[ -n "${theirs}" ]]; then
            printf 'both\n'
            return 0
        fi

        printf 'local\n'
        return 0
    fi

    if [[ -n "${theirs}" ]]; then
        printf 'system\n'
        return 0
    fi

    printf 'absent\n'
}
# }}}

# {{{ report()
# One line per tool, saying where it is and what this script thinks
# about that. Printed before anything is changed, so that a person who
# stops the run still knows what they were looking at.
report()
{
    local name="$1"
    local state="$2"

    local where=""
    local note=""

    case "${state}" in
        absent)
            where="not on this machine"
            if [[ "${REQUIRED[${name}]}" == "yes" ]]; then
                note="required"
            else
                note="only needed for ${PURPOSE[${name}]}"
            fi
            ;;
        system)
            local theirs
            theirs="$(system_binary "${name}")"
            local found
            found="$(system_version "${name}" "${theirs}")"
            where="${theirs}"

            if acceptable_version "${name}" "${found}"; then
                note="the system's, and the project is happy with it"
            else
                # Deliberately not an error and deliberately not fixed.
                # What version a machine's packages are at is a fact
                # about a machine somebody else administers, and the
                # most this script may do about it is mention it.
                note="the system's, reporting ${found}, which this project cannot use — left alone; ask for a local copy instead"
            fi
            ;;
        local)
            where="$(local_binary "${name}")"
            note="this project's own, and current"
            ;;
        local-stale)
            where="$(local_binary "${name}")"
            note="this project's own, at $(manifest_version "${name}") — will be updated"
            ;;
        both)
            where="$(local_binary "${name}")"
            note="this project's own, and current; the system also has one at $(system_binary "${name}")"
            ;;
    esac

    printf '  %-13s %s\n' "${name}" "${where}"
    printf '  %-13s %s\n' "" "${note}"
}
# }}}

# {{{ offer()
# Ask, once, whether to fetch something the machine already provides.
# A run with no terminal attached never asks — it says what it would
# have asked and carries on, because a build that blocks on a question
# nobody is there to answer is a build that hangs.
offer()
{
    local question="$1"

    if [[ "${ASSUME_YES}" == "yes" ]]; then
        return 0
    fi

    if [[ ! -t 0 ]]; then
        say "  (would ask: ${question} — nobody is at the terminal, so no)"
        return 1
    fi

    local answer=""
    read -r -p "  ${question} [y/N] " answer

    if [[ "${answer}" == "y" || "${answer}" == "Y" ]]; then
        return 0
    fi

    return 1
}
# }}}

# {{{ act()
# What to do about one tool, given where it stands. Every branch says
# what it would mean, because the asymmetry between them is the whole
# design and a reader who does not see it will eventually "fix" it.
act()
{
    local name="$1"
    local state="$2"

    case "${state}" in
        local|both)
            # Nothing to do, which is the case that makes running this
            # in front of a build free.
            ;;

        local-stale)
            # Ours, at the wrong version. Updated without asking: the
            # decision to keep a local copy was made when it was
            # installed, and a local copy nobody keeps current is worse
            # than none, because it is silently different from what the
            # project was tested against.
            say "  updating this project's copy of ${name} ..."
            "install_${name//-/_}"
            ;;

        absent)
            if [[ "${WEIGHT[${name}]}" == "impossible" ]]; then
                die "no C compiler on this machine, and nothing here can supply one"
            fi

            if [[ "${WEIGHT[${name}]}" == "cheap" ]]; then
                # Small enough that fetching it unasked is a kindness
                # rather than a liberty.
                say "  ${name} is missing and small; fetching it ..."
                "install_${name//-/_}"
                return 0
            fi

            # Heavy. Offered, never assumed. Somebody who does not
            # touch the map format never needs this at all, and the
            # download is large enough to notice on a slow line.
            if offer "fetch a project-local ${name}?"; then
                "install_${name//-/_}"
            else
                say "  left absent — ${PURPOSE[${name}]} will not work until it is here"
            fi
            ;;

        system)
            # Some things cannot have a local copy at all, and asking
            # would be an offer this script cannot keep — answering yes
            # used to reach the compiler's install function, whose
            # whole body is a refusal. Checked here rather than trusted
            # to the person answering.
            if [[ "${WEIGHT[${name}]}" == "impossible" ]]; then
                return 0
            fi

            # The machine has one. Offered rather than taken, because a
            # local copy is insurance against somebody else's upgrade
            # and not everybody wants to spend the disk on it.
            if offer "the system has ${name}; keep a copy inside the project too?"; then
                "install_${name//-/_}"
            fi
            ;;
    esac
}
# }}}

# {{{ main()
main()
{
    local mode="fix"
    local only=""
    ASSUME_YES="no"

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --check)
                mode="check"
                ;;
            --yes)
                ASSUME_YES="yes"
                ;;
            --local)
                mode="local"
                shift
                only="${1:-all}"
                ;;
            *)
                if [[ -d "$1" ]]; then
                    DIR="$1"
                    TOOLCHAIN="${DIR}/toolchain"
                    TOOLBIN="${TOOLCHAIN}/bin"
                    TOOLSRC="${TOOLCHAIN}/src"
                    MANIFEST="${TOOLCHAIN}/installed"
                else
                    die "not a directory and not an option: $1"
                fi
                ;;
        esac
        shift
    done

    say "what this project needs, and where it is:"
    say ""

    declare -A STATE=()
    local name

    for name in "${ORDER[@]}"; do
        STATE[${name}]="$(status_of "${name}")"
        report "${name}" "${STATE[${name}]}"
        say ""
    done

    if [[ "${mode}" == "check" ]]; then
        # Report only. Exits unhappily if something the build cannot do
        # without is missing, so this mode is usable as a gate.
        for name in "${ORDER[@]}"; do
            if [[ "${REQUIRED[${name}]}" == "yes" && "${STATE[${name}]}" == "absent" ]]; then
                die "${name} is required and absent"
            fi
        done
        return 0
    fi

    if [[ "${mode}" == "local" ]]; then
        for name in "${ORDER[@]}"; do
            if [[ "${only}" != "all" && "${only}" != "${name}" ]]; then
                continue
            fi

            if [[ "${WEIGHT[${name}]}" == "impossible" ]]; then
                if [[ "${only}" == "${name}" ]]; then
                    die "${name} cannot have a local copy — see the comment above install_compiler"
                fi
                continue
            fi

            # Both states in which a current local copy exists, not
            # just one. The first version of this line tested only for
            # `local` and so rebuilt every time on any machine that
            # also had a system copy — which is most of them, and which
            # is the exact case this mode is for. Idempotency that
            # holds on a bare machine and not on a normal one is not
            # idempotency.
            if [[ "${STATE[${name}]}" == "local" || "${STATE[${name}]}" == "both" ]]; then
                say "${name}: already local and current, nothing to do"
                continue
            fi

            say "${name}:"
            "install_${name//-/_}"
            say ""
        done
        return 0
    fi

    for name in "${ORDER[@]}"; do
        act "${name}" "${STATE[${name}]}"
    done

    say "done. Nothing above needs doing again."
}
# }}}

main "$@"
