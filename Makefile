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
CFLAGS += -I$(DIR)/libs -I$(DIR)/src

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
CFLAGS += -DSORA_CC='"$(CC)"'
CFLAGS += -DSORA_GENERATOR='"$(BUILD)/generate"'
# The project root, so a test can find the box sources the generated
# file names — those paths are shortened against it so that two
# machines building the same tree emit the same file (issue 311c).
CFLAGS += -DSORA_ROOT='"$(DIR)"'
CFLAGS += -DSORA_INCLUDE='"$(DIR)/src"'
CFLAGS += -DSORA_INCLUDE_LIBS='"$(DIR)/libs"'
CFLAGS += -DSORA_RAM_SHARED='"$(RAM_SHARED)"'
CFLAGS += -DSORA_RAM_EXEC='"$(RAM_EXEC)"'

# The generator (phase 3): box sources are whatever sits in
# src/boxes/ — discovered, never listed, so a box cannot exist that
# the generator silently does not see. The registry is derived from
# them at build time, lands outside history, and is rebuilt whenever
# any box source or the generator itself is newer. A failing
# generator writes nothing into place, so a build can never compile
# against yesterday's registry.
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

# The map reader, linked into the generator so it can compile a
# description into the calls it describes (issue 311d). It parses text
# into a description and touches nothing else of the engine — three
# enumerations for the station kinds and the door marks — which is why
# a build tool can hold it. Its eventual home is the generator alone;
# until the engine stops reading maps at run time it has two callers.
GEN_MAPS  := $(DIR)/src/041-mapfile.c

# Which maps this build compiles into the binary. Discovered, never
# listed, so a map cannot exist that the build silently does not see —
# the same rule box sources have always had.
MAP_SRC   := $(wildcard $(DIR)/maps/*.map)
MAP_FLAGS := $(patsubst %,--map=%,$(MAP_SRC))
GENERATOR := $(BUILD)/generate
GENERATED := $(DIR)/src/generated/registry.c

$(GENERATOR): $(GEN_SRC) $(GEN_MAPS) | $(BUILD)
	$(CC) $(CFLAGS) -I$(DIR)/scripts -o $@ $(GEN_SRC) $(GEN_MAPS)

# Maps join the dependency list, so editing one regenerates (issue
# 311d step 3).
$(GENERATED): $(BOX_SRC) $(MAP_SRC) $(GENERATOR)
	mkdir -p $(DIR)/src/generated

	$(GENERATOR) $(GENERATED) --root=$(DIR) $(MAP_FLAGS) $(BOX_SRC)
# Everything the engine is made of: the pool from libs/, the station
# layer and what follows from src/, and the generated registry.
# Discovered by wildcard so a new engine file enrolls itself.
ENGINE_SRC := $(wildcard $(DIR)/libs/*.c) $(wildcard $(DIR)/src/*.c) $(GENERATED)

# What the parser saw, for diagnosing a build problem by looking at
# the description rather than the emission.
.PHONY: describe
describe: $(GENERATOR)
	$(GENERATOR) --describe $(BOX_SRC)

# The HTML documentation set (issue 705): generated from the markdown,
# never maintained beside it. Regenerated on demand and as part of a
# full build, so stale HTML cannot ship.
.PHONY: html
html:
	luajit $(DIR)/scripts/054-docs-html.lua $(DIR)

TEST_SRC  := $(wildcard $(DIR)/tests/*.c)
TEST_BINS := $(patsubst $(DIR)/tests/%.c,$(BUILD)/%,$(TEST_SRC))

.PHONY: all test clean

all: $(TEST_BINS) html

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

$(BUILD)/%: $(DIR)/tests/%.c $(ENGINE_SRC) $(SURFACE) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(ENGINE_SRC) $(LDFLAGS)

# The generator's own unit test links the generator's pieces rather
# than the engine: it is testing the build tool, not the thing the
# tool builds. An explicit rule beats the pattern rule above, so this
# one file compiles differently without excluding it from the sweep.
$(BUILD)/071-test-gentext: $(DIR)/tests/071-test-gentext.c $(GEN_LIB) $(GEN_MAPS) | $(BUILD)
	$(CC) $(CFLAGS) -I$(DIR)/scripts -o $@ $< $(GEN_LIB) $(GEN_MAPS)

# Shell-driven tests sit beside the compiled ones — the generator's
# command-line conduct is proven from the shell.
TEST_SCRIPTS := $(wildcard $(DIR)/tests/*.sh)

# Each test is run in order; the first failure stops the run, because
# later tests build on machinery the earlier ones just proved broken.
test: $(TEST_BINS)
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
