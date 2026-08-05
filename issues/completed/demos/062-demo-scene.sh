#!/usr/bin/env bash
#
# 062-demo-scene.sh — how a shell-driven phase demo talks to a person.
#
# What this is: the shell counterpart of 060-demo-scene.h. Phases 3 and
# 6 are demonstrated from the shell, because what they prove is about
# builds and files rather than about running code — a generator being
# rerun, a text file being edited between runs. They still have to read
# exactly like the other five.
#
# How it does it, in general terms: the same block shapes, the same
# column widths, the same wrap width, the same palette, and the same
# pager as the compiled presenter. Text is wrapped here; blank lines
# between blocks are inserted here; screen rows are counted here so a
# finished scene can erase precisely itself. Nothing that calls these
# functions decides layout.
#
# Sourced, not run. See issue 707 for the contract a scene has to
# satisfy — in particular that every correspondence carries its own
# justification, which is why the mapping function takes three
# arguments rather than two.

# The page, matching 061-demo-scene.c exactly. If one moves, both move
# — two demos side by side in one terminal with different margins look
# like two projects.
SORA_SCENE_WIDTH=72
SORA_SCENE_LABEL_WIDTH=24
SORA_SCENE_STORY_WIDTH=22
SORA_SCENE_TERM_WIDTH=28

SORA_SCENE_REPORT=""
SORA_SCENE_LAST_BLOCK=""
SORA_SCENE_ROWS=0
SORA_SCENE_COLUMNS=80
SORA_SCENE_INTERACTIVE=0

# The palette, meaning for meaning identical to the compiled one.
SORA_C_RESET=""
SORA_C_HEADING=""
SORA_C_ENGINE=""
SORA_C_STORY=""
SORA_C_FIGURE=""
SORA_C_QUIET=""
SORA_C_KEY=""

# The correspondences named so far in this scene, held until something
# else needs printing: they are rendered twice, once as a table and
# once as justified sentences, so they cannot be printed as they
# arrive.
SORA_MAP_ENGINE=()
SORA_MAP_STORY=()
SORA_MAP_BECAUSE=()

# {{{ sora_scene__setup()
# Decides whether anybody is watching. A redirected run gets no colour,
# no paging and no waiting — there is nobody there to press the key,
# and escape sequences written into a file are noise in a document
# somebody may grep in six months.
sora_scene__setup()
{
    if [[ -t 1 && -t 0 ]]; then
        SORA_SCENE_INTERACTIVE=1
        SORA_C_RESET=$'\033[0m'
        SORA_C_HEADING=$'\033[1;36m'
        SORA_C_ENGINE=$'\033[33m'
        SORA_C_STORY=$'\033[32m'
        SORA_C_FIGURE=$'\033[1;37m'
        SORA_C_QUIET=$'\033[2m'
        SORA_C_KEY=$'\033[1;35m'
        SORA_SCENE_COLUMNS=$(tput cols 2>/dev/null || echo 80)
    else
        SORA_SCENE_INTERACTIVE=0
    fi
}
# }}}

# {{{ sora_scene__emit()
# Everything printed passes through here: to the screen with colour, to
# the report without it, so the mirrored copy is plain text and cannot
# become a different document from the run.
#
# The row count is taken from the uncoloured text and divided by the
# window's width, because a line longer than the window occupies two
# rows and erasing the page has to know that.
sora_scene__emit()
{
    local colour="$1"
    local text="$2"
    local length=${#text}
    local rows=1

    if (( length > SORA_SCENE_COLUMNS )); then
        rows=$(( (length + SORA_SCENE_COLUMNS - 1) / SORA_SCENE_COLUMNS ))
    fi
    SORA_SCENE_ROWS=$(( SORA_SCENE_ROWS + rows ))

    if [[ -n "${colour}" && ${SORA_SCENE_INTERACTIVE} -eq 1 ]]; then
        printf '%s%s%s\n' "${colour}" "${text}" "${SORA_C_RESET}"
    else
        printf '%s\n' "${text}"
    fi
    printf '%s\n' "${text}" >> "${SORA_SCENE_REPORT}"
}
# }}}

# {{{ sora_scene__newline()
sora_scene__newline()
{
    SORA_SCENE_ROWS=$(( SORA_SCENE_ROWS + 1 ))
    printf '\n'
    printf '\n' >> "${SORA_SCENE_REPORT}"
}
# }}}

# {{{ sora_scene__block()
# Emits the separating blank line when the kind of block being printed
# has changed, and nothing when it has not. Flushes any pending
# correspondence table first.
sora_scene__block()
{
    local kind="$1"

    if [[ ${#SORA_MAP_ENGINE[@]} -gt 0 && "${kind}" != "table" ]]; then
        sora_scene__flush_mapping
    fi
    if [[ -n "${SORA_SCENE_LAST_BLOCK}" && "${SORA_SCENE_LAST_BLOCK}" != "${kind}" ]]; then
        sora_scene__newline
    fi
    SORA_SCENE_LAST_BLOCK="${kind}"
}
# }}}

# {{{ sora_scene__wrapped()
# Prints a paragraph broken at spaces to fit the page, each line
# carrying a two-space indent.
#
# The paragraph is flattened to one line before it is folded. Stories
# are written across several source lines for the sake of whoever is
# reading the script, and fold treats every input line as its own
# paragraph — so without flattening, the story appears on screen broken
# wherever it happened to be broken in the source, which looked like a
# wrapping bug and was one.
sora_scene__wrapped()
{
    local colour="$1"
    local text="$2"
    local room=$(( SORA_SCENE_WIDTH - 2 ))
    local flattened
    local squeezed
    local folded
    local line

    flattened="$(printf '%s' "${text}" | tr '\n' ' ')"
    squeezed="$(printf '%s' "${flattened}" | tr -s ' ')"
    folded="$(printf '%s\n' "${squeezed}" | fold -s -w "${room}")"

    while IFS= read -r line; do
        line="${line%"${line##*[![:space:]]}"}"
        sora_scene__emit "${colour}" "  ${line}"
    done <<< "${folded}"
}
# }}}

# {{{ sora_scene__pad()
# A column and the gap after it. printf's %-*s gives no gap at all when
# the text is as wide as the column, which welds two columns together;
# two spaces are guaranteed instead, so an over-long entry pushes its
# own row out of alignment rather than becoming unreadable.
sora_scene__pad()
{
    local text="$1"
    local width="$2"
    local used=${#text}
    local gap=2

    if (( used < width )); then
        gap=$(( width - used ))
    fi
    printf '%s%*s' "${text}" "${gap}" ""
}
# }}}

# {{{ sora_scene__flush_mapping()
# Renders the correspondences: first the table, then the reasons.
#
# Both, and in that order, on purpose. The table exists to be scanned,
# so the shape of the mapping can be taken in at a glance and referred
# back to. The sentences exist to be believed: an analogy nobody
# justified is a claim the reader has to take on trust.
sora_scene__flush_mapping()
{
    local count=${#SORA_MAP_ENGINE[@]}
    local i
    local row
    local sentence
    local first
    local rest

    # Emptied before printing, so the block calls below cannot recurse
    # back into here.
    local engines=("${SORA_MAP_ENGINE[@]}")
    local stories=("${SORA_MAP_STORY[@]}")
    local becauses=("${SORA_MAP_BECAUSE[@]}")
    SORA_MAP_ENGINE=()
    SORA_MAP_STORY=()
    SORA_MAP_BECAUSE=()

    if [[ -n "${SORA_SCENE_LAST_BLOCK}" ]]; then
        sora_scene__newline
    fi

    row="    $(sora_scene__pad "in the engine" "${SORA_SCENE_TERM_WIDTH}")in the story"
    sora_scene__emit "${SORA_C_QUIET}" "${row}"
    row="    $(sora_scene__pad "--------------------------" "${SORA_SCENE_TERM_WIDTH}")--------------------------"
    sora_scene__emit "${SORA_C_QUIET}" "${row}"

    for (( i = 0; i < count; i++ )); do
        row="    $(sora_scene__pad "${engines[i]}" "${SORA_SCENE_TERM_WIDTH}")${stories[i]}"
        sora_scene__emit "${SORA_C_ENGINE}" "${row}"
    done

    for (( i = 0; i < count; i++ )); do
        sora_scene__newline
        sentence="${engines[i]} is like ${stories[i]} because ${becauses[i]}."
        # The demos write their terms in lower case, because that is how
        # the terms read inside the table's columns. A sentence still
        # starts with a capital.
        first="${sentence:0:1}"
        rest="${sentence:1}"
        sentence="${first^^}${rest}"
        sora_scene__wrapped "" "${sentence}"
    done

    SORA_SCENE_LAST_BLOCK="table"
}
# }}}

# {{{ sora_scene__erase()
# Walks the cursor up, clearing each line, and leaves it where the page
# began. Never clears the whole screen: whatever the reader had above
# the demo is theirs.
sora_scene__erase()
{
    local rows="$1"
    local i

    if [[ ${SORA_SCENE_INTERACTIVE} -eq 0 ]] || (( rows <= 0 )); then
        return
    fi
    for (( i = 0; i < rows; i++ )); do
        printf '\033[1A\033[2K'
    done
    printf '\r'
}
# }}}

# {{{ sora_scene__page_end()
# Offers the finished page to the reader, waits, then takes it back.
# Everything said is in the report, so nothing is lost by clearing it
# from the screen.
sora_scene__page_end()
{
    local key

    if [[ ${SORA_SCENE_INTERACTIVE} -eq 0 ]] || (( SORA_SCENE_ROWS == 0 )); then
        SORA_SCENE_ROWS=0
        return
    fi

    printf '\n'
    printf '  %s[space]%s next scene   %s[q]%s quit   %severything here is mirrored to the report%s\n' \
        "${SORA_C_KEY}" "${SORA_C_RESET}" "${SORA_C_KEY}" "${SORA_C_RESET}" \
        "${SORA_C_QUIET}" "${SORA_C_RESET}"
    SORA_SCENE_ROWS=$(( SORA_SCENE_ROWS + 2 ))

    while true; do
        IFS= read -r -s -n 1 key || key=" "
        if [[ "${key}" == "q" || "${key}" == "Q" ]]; then
            sora_scene__erase "${SORA_SCENE_ROWS}"
            SORA_SCENE_ROWS=0
            printf '  stopped early; the report holds every scene, including the ones not shown\n'
            exit 0
        fi
        # Any other key is ignored rather than advancing the page, so a
        # key hit by accident does not skip a scene.
        if [[ -z "${key}" || "${key}" == " " ]]; then
            break
        fi
    done

    sora_scene__erase "${SORA_SCENE_ROWS}"
    SORA_SCENE_ROWS=0
    SORA_SCENE_LAST_BLOCK=""
}
# }}}

# {{{ sora_demo_open()
# sora_demo_open <report-path> <title>
#
# Starts the report fresh and prints the banner. As in the compiled
# demos, the report is not optional: a demo whose closing line promises
# a mirrored report has to be able to keep that promise.
sora_demo_open()
{
    SORA_SCENE_REPORT="$1"
    SORA_SCENE_LAST_BLOCK=""
    SORA_SCENE_ROWS=0

    if ! : > "${SORA_SCENE_REPORT}"; then
        echo "demo: cannot write its report to ${SORA_SCENE_REPORT}" >&2
        echo "demo: the shared-memory tier is missing or unwritable;" >&2
        echo "demo: the launcher creates it" >&2
        exit 1
    fi

    sora_scene__setup

    # ctrl-c during a demo leaves the cursor hidden unless somebody puts
    # it back, and a shell with an invisible cursor is a shell nobody
    # can type into comfortably. Only armed when there is a cursor to
    # hide: a redirected run would otherwise write the escape sequence
    # into whatever it was piped to.
    if [[ ${SORA_SCENE_INTERACTIVE} -eq 1 ]]; then
        trap 'printf "\033[?25h"; exit 130' INT TERM
        trap 'printf "\033[?25h"' EXIT
        printf '\033[?25l'
    fi

    sora_scene__emit "${SORA_C_HEADING}" "$2"
    if [[ ${SORA_SCENE_INTERACTIVE} -eq 0 ]]; then
        sora_scene__emit "" "  (not a terminal: no paging, no colour, every scene printed at once)"
    fi
    SORA_SCENE_LAST_BLOCK="heading"
}
# }}}

# {{{ sora_demo_note()
# A wrapped paragraph belonging to the whole demo rather than a scene.
sora_demo_note()
{
    sora_scene__block prose
    sora_scene__wrapped "" "$1"
}
# }}}

# {{{ sora_scene_open()
# sora_scene_open <number> <heading>
# Ends the previous page, then prints this scene's heading.
sora_scene_open()
{
    sora_scene__page_end
    sora_scene__newline
    sora_scene__emit "${SORA_C_HEADING}" "scene $1 — $2"
    SORA_SCENE_LAST_BLOCK="heading"
}
# }}}

# {{{ sora_scene_problem()
# The problem this scene exists to show, in the engine's own
# vocabulary. No analogy: a reader who already knows the engine should
# be able to read this and skip the rest of the page.
sora_scene_problem()
{
    sora_scene__block prose
    sora_scene__wrapped "" "$1"
}
# }}}

# {{{ sora_scene_imagine()
# The image to hold the problem by. The framing sentence belongs here
# rather than to each demo, so the hinge between mechanism and analogy
# is worded the same way in all thirty-five scenes.
sora_scene_imagine()
{
    sora_scene__block imagine
    sora_scene__wrapped "${SORA_C_STORY}" "To picture it more easily, imagine $1"
}
# }}}

# {{{ sora_scene_stands_for()
# sora_scene_stands_for <engine term> <story thing> <because...>
#
# The third argument is the point of the interface. "A ring cell // a
# pigeonhole" asserts a similarity and leaves the reader to guess at
# it; the justification is the thing actually worth reading. Write it
# as a clause completing "X is like Y because ...".
sora_scene_stands_for()
{
    SORA_MAP_ENGINE+=("$1")
    SORA_MAP_STORY+=("$2")
    SORA_MAP_BECAUSE+=("$3")
}
# }}}

# {{{ sora_scene_measured()
# sora_scene_measured <label> <in story units> [in engine units]
#
# Two columns when the story's unit is the engine's unit, three when
# they differ — the same rule the compiled demos follow.
sora_scene_measured()
{
    local row

    sora_scene__block measured
    if [[ $# -ge 3 && -n "$3" ]]; then
        row="    $(sora_scene__pad "$1" "${SORA_SCENE_LABEL_WIDTH}")$(sora_scene__pad "$2" "${SORA_SCENE_STORY_WIDTH}")$3"
    else
        row="    $(sora_scene__pad "$1" "${SORA_SCENE_LABEL_WIDTH}")$2"
    fi
    sora_scene__emit "${SORA_C_FIGURE}" "${row}"
}
# }}}

# {{{ sora_scene_line()
# An unstructured indented line, for evidence that is a listing rather
# than a row — emitted C, a registry entry, a compiler's complaint.
sora_scene_line()
{
    sora_scene__block line
    sora_scene__emit "" "    $1"
}
# }}}

# {{{ sora_scene_quote()
# Several lines of something the demo did not write — a file, a
# command's output — indented as one block and mirrored. Reads standard
# input so a command's output can be handed over whole.
sora_scene_quote()
{
    local line

    sora_scene__block line
    while IFS= read -r line; do
        # A blank line in the quoted material stays blank. Indenting it
        # would put four spaces into the report that nobody can see and
        # nobody will ever remove.
        if [[ -z "${line}" ]]; then
            sora_scene__newline
        else
            sora_scene__emit "${SORA_C_QUIET}" "    ${line}"
        fi
    done
}
# }}}

# {{{ sora_scene_finding()
sora_scene_finding()
{
    sora_scene__block finding
    sora_scene__wrapped "" "$1"
}
# }}}

# {{{ sora_scene_blank()
sora_scene_blank()
{
    if [[ ${#SORA_MAP_ENGINE[@]} -gt 0 ]]; then
        sora_scene__flush_mapping
    fi
    sora_scene__newline
    SORA_SCENE_LAST_BLOCK=""
}
# }}}

# {{{ sora_demo_page_break()
# Ends the current page without opening a scene. Needed where the demo
# is handed to a compiled half partway through: whichever side is about
# to stop printing settles its own page first, so the other side's
# pager never erases a count it did not write.
sora_demo_page_break()
{
    sora_scene__page_end
}
# }}}

# {{{ sora_demo_close()
# Erases the last page, prints the closing line, and gives the cursor
# back.
sora_demo_close()
{
    sora_scene__page_end
    if [[ ${SORA_SCENE_INTERACTIVE} -eq 0 ]]; then
        sora_scene__newline
    fi
    sora_scene__emit "${SORA_C_HEADING}" "$1"
    SORA_SCENE_LAST_BLOCK=""
}
# }}}
