# Makefile — how anything in this project gets built or tested.
#
# What this is: the one entry point for compiling the engine, running
# the generator (from phase 3 on), and running every test. A person
# types `make test` and learns whether the machine works; nothing else
# needs remembering.
#
# How it does it, in general terms: sources live in libs/ and src/,
# tests live in tests/, and everything compiled lands in the RAM-backed
# tmp/ tier so the repository never holds a build artifact. Test
# binaries are discovered by wildcard, so writing a new test file is
# enough to enroll it — no list to edit.
#
# The project root is derived from this file's own location, so make
# works from any directory.
#
# What it needs: a C compiler, and nothing else, for everything on the
# path from sources to running tests — the generator is itself C
# (issue 308). Regenerating the HTML documentation additionally needs
# LuaJIT, which is project tooling rather than part of the build path
# a consumer walks.

DIR   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD := $(DIR)/tmp/build

# The two RAM tiers, derived from the project directory's own name so
# nothing here is typed twice: tmp/ is a symlink into /tmp for things
# that get executed, and tmp/shared-memory a symlink into /dev/shm for
# things that get read. Both targets vanish on reboot while the
# symlinks in the repository survive, so a first build after a restart
# used to die on `mkdir: File exists` — a dangling symlink, reported as
# if the source were at fault. Recreating them is idempotent and costs
# nothing, so it happens before anything wants to write.
PROJECT    := $(notdir $(DIR))
RAM_EXEC   := /tmp/$(PROJECT)
RAM_SHARED := /dev/shm/$(PROJECT)

CC     ?= gcc
CFLAGS := -std=gnu11 -Wall -Wextra -Werror -g -O2 -pthread
# One include path, because there is one header (issue 904). This was
# two — libs/ for the pool and src/ for everything above it — and a
# consumer inherited both along with having to know that the entry
# point was called 018-station.h.
CFLAGS += -I$(DIR)/src

# Every function and every piece of static data in a section of its
# own, so that the linker can throw away the ones nothing reaches
# (issue 311d). Without this the unit of discard is a whole object
# file, and one shim in use keeps every shim beside it. The cost is a
# larger object file during the build and nothing at all afterwards.
CFLAGS += -ffunction-sections -fdata-sections

# Three build-time facts a program needs at run time, and only if it
# ever brings in new code (issue 310): which compiler built it, where
# the generator is, and where the headers that generated code includes
# live. Baked in rather than discovered, because **the compiler that
# built the binary is the one that must compile anything added to it**
# — that is what gives a program exactly one answer to sizeof by
# construction rather than by checking.
CFLAGS += -DCERA_CC='"$(CC)"'
CFLAGS += -DCERA_GENERATOR='"$(BUILD)/generate"'
# The project root, so a test can find the box sources the generated
# file names — those paths are shortened against it so that two
# machines building the same tree emit the same file (issue 311c).
CFLAGS += -DCERA_ROOT='"$(DIR)"'
CFLAGS += -DCERA_INCLUDE='"$(DIR)/src"'
CFLAGS += -DCERA_RAM_SHARED='"$(RAM_SHARED)"'
CFLAGS += -DCERA_RAM_EXEC='"$(RAM_EXEC)"'

# The generator (phase 3): box sources are whatever sits in
# src/boxes/ — discovered, never listed, so a box cannot exist that
# the generator silently does not see. What it emits is derived from
# them at build time, lands outside history, and is rebuilt whenever
# any box source or the generator itself is newer. A failing
# generator writes nothing into place, so a build can never compile
# against yesterday's emission.
#
# The generator is C (issue 308), compiled here before it is run. It
# depends on nothing the engine provides, so there is no bootstrap
# problem: compile it, run it, compile everything else. It used to be
# a LuaJIT script, which meant anyone building a program with this
# engine inherited an interpreter dependency that nothing at run time
# ever used.
BOX_SRC   := $(wildcard $(DIR)/src/boxes/*.c)
GEN_SRC   := $(wildcard $(DIR)/scripts/*.c)
GEN_LIB   := $(filter-out $(DIR)/scripts/070-generate.c,$(GEN_SRC))

# The map reader used to be named here separately, because it lived in
# src/ and had two callers — the generator and the engine. The engine
# stopped reading descriptions (issue 311d), so it moved in beside the
# rest of the generator and is picked up by the wildcard above like
# anything else there. Nothing links it into a program any more.

# Which maps this build compiles into the binary. Discovered, never
# listed, so a map cannot exist that the build silently does not see —
# the same rule box sources have always had.
MAP_SRC   := $(wildcard $(DIR)/maps/*.map)
MAP_FLAGS := $(patsubst %,--map=%,$(MAP_SRC))
GENERATOR := $(BUILD)/generate
GENERATED := $(DIR)/src/generated/emitted.c

$(GENERATOR): $(GEN_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -I$(DIR)/scripts -o $@ $(GEN_SRC)

# Maps join the dependency list, so editing one regenerates (issue
# 311d step 3).
$(GENERATED): $(BOX_SRC) $(MAP_SRC) $(GENERATOR)
	mkdir -p $(DIR)/src/generated

	$(GENERATOR) $(GENERATED) --root=$(DIR) $(MAP_FLAGS) $(BOX_SRC)
# Everything the engine is made of: one file, and the generated one
# (issue 901). This used to be two wildcards over libs/ and src/, which
# discovered engine files so that a new one enrolled itself. There is
# nothing left to discover — an engine file cannot be added because
# there is one, and a new part of the runtime is a new section inside
# it.
#
# The generated file stays separate and always will: it is derived at
# build time from box sources this engine's author has never seen, so
# it cannot be inside cera.c. That is what makes the header a real
# boundary rather than a courtesy — everything generated code calls has
# to be public whether anyone likes it or not.
#
# The numbered sources cera.c was made from are still on disk and are
# compiled by nothing. They go in issue 904, once the two builds have
# been shown to produce identical output.
CERA_C     := $(DIR)/src/cera.c
CERA_H     := $(DIR)/src/cera.h
ENGINE_SRC := $(CERA_C) $(GENERATED)

# What the parser saw, for diagnosing a build problem by looking at
# the description rather than the emission.
.PHONY: describe
describe: $(GENERATOR)
	$(GENERATOR) --describe $(BOX_SRC)

# What the build wants that is not a C compiler, and where it is. The
# script changes nothing in this mode; it reports, and fails if
# something required is absent. Run it directly, without --check, to
# have it fetch what is missing.
.PHONY: dependencies
dependencies:
	@$(DIR)/scripts/109-dependencies.sh --check $(DIR)

# A tool this project fetched for itself wins over the system's copy of
# the same tool, which is the whole point of having fetched it: a local
# copy exists precisely so that what the build runs stops depending on
# what somebody's package manager did last week. Falls back to the name
# alone when there is no local copy, which finds the system's.
LUAJIT := $(firstword $(wildcard $(DIR)/toolchain/bin/luajit) luajit)

# The HTML documentation set (issue 705): generated from the markdown,
# never maintained beside it. Regenerated on demand and as part of a
# full build, so stale HTML cannot ship.
.PHONY: html
html:
	$(LUAJIT) $(DIR)/scripts/054-docs-html.lua $(DIR)

# The example a reader is pointed at first. It is built by `all` so it
# can never quietly stop compiling, and run by `make example`, which is
# the shortest path from cloning this to watching it do the thing it
# exists to do.
EXAMPLE := $(BUILD)/108-hello-graph

$(EXAMPLE): $(DIR)/example/108-hello-graph.c $(ENGINE_SRC) $(CERA_H) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(ENGINE_SRC) $(LDFLAGS)

.PHONY: example
example: $(EXAMPLE)
	@$(EXAMPLE)

TEST_SRC  := $(wildcard $(DIR)/tests/*.c)
TEST_BINS := $(patsubst $(DIR)/tests/%.c,$(BUILD)/%,$(TEST_SRC))

.PHONY: all test clean

all: $(TEST_BINS) $(EXAMPLE) $(VIEWER) $(WATCHED) $(MECHANISM) $(ANYMAP) html

# The build tree lives in RAM (tmp/ -> /tmp/<project>). It must exist
# before anything writes into it; a build that dies on a missing
# directory says nothing about the code.
# Order-only, so a phony prerequisite cannot make the build directory
# look perpetually out of date.
.PHONY: ramdirs
ramdirs:
	@mkdir -p $(RAM_EXEC)
	@mkdir -p $(RAM_SHARED)
	@ln -sfn $(RAM_SHARED) $(RAM_EXEC)/shared-memory

$(BUILD): | ramdirs
	mkdir -p $(BUILD)

# Two linker instructions that only make sense together, and used to
# be one instruction that cancelled the other out (issue 311d).
#
# The engine's own symbols have to appear in the executable's dynamic
# table, because a box compiled while the program runs arrives as a
# shared object and is dlopened, its generated placement function calls
# straight into the station layer, and a shared object cannot see a
# symbol the host did not export. That was not needed while generated
# code held only shims — a shim calls the box, and the box is inside
# the shared object with it. It became needed the moment generated code
# started *building stations*, which is the whole point of a placement
# function.
#
# But the sweeping form of that instruction, -rdynamic, exports every
# global symbol in the binary, and an exported symbol is a root the
# section collector must keep. So -rdynamic silently declared the whole
# program reachable and --gc-sections collected nothing. Naming the
# families that are actually public — which is what
# src/098-engine-surface.syms does, with the measurements in it —
# exports the engine and lets everything nothing reaches be thrown
# away.
#
# Anyone linking a program with this engine inherits both halves,
# which belongs with the rest of the packaging story.
SURFACE := $(DIR)/src/098-engine-surface.syms
LDFLAGS := -Wl,--dynamic-list=$(SURFACE) -Wl,--gc-sections

$(BUILD)/%: $(DIR)/tests/%.c $(ENGINE_SRC) $(CERA_H) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(ENGINE_SRC) $(LDFLAGS)

# The white-box tests, which examine the engine's own machinery rather
# than only what a program built with it calls — slots, pages,
# destination sets, the constants a port holds (issue 903).
#
# Those functions are private, and a private function cannot be reached
# from another translation unit no matter what is declared. So these
# tests include cera.c and are compiled as one unit with it. The engine
# is therefore NOT on their link line: it is already inside them, and
# putting it in both places is every symbol defined twice.
#
# A static pattern rule, which beats the general rule above for exactly
# these names and leaves every other test alone.
WHITEBOX := 021-test-station-table 035-test-statics 064-test-slot-states \
            072-test-width-wiring 076-test-destinations 077-test-removal \
            080-test-page-growth
WHITEBOX_BINS := $(patsubst %,$(BUILD)/%,$(WHITEBOX))

$(WHITEBOX_BINS): $(BUILD)/%: $(DIR)/tests/%.c $(CERA_C) $(CERA_H) $(GENERATED) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(GENERATED) $(LDFLAGS)

# The generator's own unit test links the generator's pieces rather
# than the engine: it is testing the build tool, not the thing the
# tool builds. An explicit rule beats the pattern rule above, so this
# one file compiles differently without excluding it from the sweep.
$(BUILD)/071-test-gentext: $(DIR)/tests/071-test-gentext.c $(GEN_LIB) | $(BUILD)
	$(CC) $(CFLAGS) -I$(DIR)/scripts -o $@ $< $(GEN_LIB)

# The map format's round trip is the same kind of test, for the same
# reason: it exercises the thing that reads and writes descriptions,
# not the thing that runs them (issue 801).
$(BUILD)/106-test-mapwrite: $(DIR)/tests/106-test-mapwrite.c $(GEN_LIB) | $(BUILD)
	$(CC) $(CFLAGS) -I$(DIR)/scripts -o $@ $< $(GEN_LIB)

# The trail's test is the one thing built with watching turned on, since
# it is the only thing that has anything to watch. Everything else is
# built without it, which is also what proves an ordinary program pays
# nothing: the emitting is not in those binaries at all.
$(BUILD)/118-test-the-trail: $(DIR)/tests/118-test-the-trail.c $(ENGINE_SRC) $(CERA_H) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -DCERA_WATCH -o $@ $< $(ENGINE_SRC) $(LDFLAGS)

# The viewer, and something for it to watch. Neither is part of the
# engine — they are phase 8 tools that stand outside it — but both are
# built by the ordinary build so that a change to the engine's own
# reader cannot quietly stop them compiling.
#
# The program being watched is built with watching on, since that is its
# entire purpose. The viewer is not: it only ever reads somebody else's
# ring, and building it with the flag would suggest otherwise.
VIEWER  := $(BUILD)/119-viewer
WATCHED := $(BUILD)/123-a-program-to-watch
MECHANISM := $(BUILD)/126-a-mechanism-to-watch
ANYMAP    := $(BUILD)/128-watch-a-map

$(VIEWER): $(DIR)/viewer/119-viewer.c $(ENGINE_SRC) $(CERA_H) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(ENGINE_SRC) $(LDFLAGS)

$(WATCHED): $(DIR)/viewer/123-a-program-to-watch.c $(ENGINE_SRC) $(CERA_H) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -DCERA_WATCH -o $@ $< $(ENGINE_SRC) $(LDFLAGS)

$(MECHANISM): $(DIR)/viewer/126-a-mechanism-to-watch.c $(ENGINE_SRC) $(CERA_H) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -DCERA_WATCH -o $@ $< $(ENGINE_SRC) $(LDFLAGS)

$(ANYMAP): $(DIR)/viewer/128-watch-a-map.c $(ENGINE_SRC) $(CERA_H) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -DCERA_WATCH -o $@ $< $(ENGINE_SRC) $(LDFLAGS)

# What to type to watch something. Two processes, so it says how rather
# than starting them: the one being watched is somebody's own program.
.PHONY: viewer
viewer: $(VIEWER) $(WATCHED) $(MECHANISM) $(ANYMAP)
	@echo "One command:"
	@echo "  $(MECHANISM) --trail=$(RAM_SHARED)/live.ring --view"
	@echo ""
	@echo "Or any map this binary was built with:"
	@echo "  $(ANYMAP) --map=127-the-ladder.map \\"
	@echo "      --trail=$(RAM_SHARED)/live.ring --view"
	@echo ""
	@echo "Or in two terminals:"
	@echo "  $(WATCHED) --trail=$(RAM_SHARED)/live.ring"
	@echo "In another:"
	@echo "  $(VIEWER) --trail=$(RAM_SHARED)/live.ring \\"
	@echo "      --map=$(DIR)/maps/107-example.map --root=$(DIR)/viewer"
	@echo "Then open http://localhost:8723/"

# Shell-driven tests sit beside the compiled ones — the generator's
# command-line conduct is proven from the shell.
TEST_SCRIPTS := $(wildcard $(DIR)/tests/*.sh)

# Each test is run in order; the first failure stops the run, because
# later tests build on machinery the earlier ones just proved broken.
test: $(TEST_BINS) $(VIEWER) $(WATCHED) $(MECHANISM) $(ANYMAP)
	@for t in $(TEST_BINS); do \
		echo "== $$(basename $$t)"; \
		$$t || exit 1; \
	done
	@for s in $(TEST_SCRIPTS); do \
		echo "== $$(basename $$s)"; \
		bash $$s $(DIR) || exit 1; \
	done
	@echo "all tests passed"

clean:
	rm -rf $(BUILD)
	rm -rf $(DIR)/src/generated
