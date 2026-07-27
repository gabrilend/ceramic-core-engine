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

DIR   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD := $(DIR)/tmp/build

CC     ?= gcc
CFLAGS := -std=gnu11 -Wall -Wextra -Werror -g -O2 -pthread
CFLAGS += -I$(DIR)/libs -I$(DIR)/src

# Everything the engine is made of: the pool from libs/, the station
# layer and what follows from src/. Discovered by wildcard so a new
# engine file enrolls itself.
ENGINE_SRC := $(wildcard $(DIR)/libs/*.c) $(wildcard $(DIR)/src/*.c)

TEST_SRC  := $(wildcard $(DIR)/tests/*.c)
TEST_BINS := $(patsubst $(DIR)/tests/%.c,$(BUILD)/%,$(TEST_SRC))

.PHONY: all test clean

all: $(TEST_BINS)

# The build tree lives in RAM (tmp/ -> /tmp/<project>). It must exist
# before anything writes into it; a build that dies on a missing
# directory says nothing about the code.
$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%: $(DIR)/tests/%.c $(ENGINE_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(ENGINE_SRC)

# Each test is run in order; the first failure stops the run, because
# later tests build on machinery the earlier ones just proved broken.
test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "== $$(basename $$t)"; \
		$$t || exit 1; \
	done
	@echo "all tests passed"

clean:
	rm -rf $(BUILD)
