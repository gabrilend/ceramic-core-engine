# SoraMech — top-level build orchestration.
#
# What it does, in a sentence: builds the phase 3 pool runner binary
# plus one shared library per language spec. Release by default;
# DEBUG=1 keeps symbols and disables optimization, STRICT=1 promotes
# warnings to errors. `make clean` removes everything that was built.
#
# Invokable from any directory: `make -C /path/to/soramech` works,
# and the DIR variable can be overridden if needed (e.g. for an
# out-of-tree checkout). All paths in this Makefile are relative to
# $(DIR) per the project convention.
#
# Designed in issue 309. Implementation of the runner's runtime is
# split across issues 301–308; this Makefile builds whatever sources
# exist under src/, libs/task-pool/, and libs/json/, so it scales as
# those issues land.

# {{{ Resolve project root
DIR ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
# }}}

# {{{ Toolchain & flags
CC      ?= gcc

# Shared header search path: every C source can find langs/lang-spec.h
# and any future libs/<name>/ headers via -I flags below.
CPPFLAGS = -I$(DIR)/langs -I$(DIR)/libs/task-pool -I$(DIR)/libs/json -I$(DIR)/src

# Default to release; DEBUG and STRICT toggle alternates.
ifeq ($(DEBUG),1)
  CFLAGS = -O0 -g -DDEBUG -Wall -pthread -fPIC
else ifeq ($(STRICT),1)
  CFLAGS = -O2 -Wall -Wextra -Wpedantic -Werror -pthread -fPIC
else
  CFLAGS = -O2 -Wall -pthread -fPIC
endif

LDFLAGS = -ldl -pthread
# }}}

# {{{ Paths and source discovery
BUILD_DIR = $(DIR)/build
POOL_BIN  = $(DIR)/soramech-pool

# Wildcards: empty subdirs return empty lists, which is fine. As
# issues 301/302/303/304/305 land their C sources here, they get
# compiled automatically — no Makefile edit required.
POOL_SOURCES = \
  $(wildcard $(DIR)/src/*.c) \
  $(wildcard $(DIR)/libs/task-pool/*.c) \
  $(wildcard $(DIR)/libs/json/*.c)

POOL_OBJECTS = $(POOL_SOURCES:$(DIR)/%.c=$(BUILD_DIR)/%.o)

# Test sources live in tests/ as C files named to match the source
# they exercise (e.g. tests/009-slot-store-test.c exercises
# src/009-slot-store.c). Each builds to a separate executable that
# `make test` runs in turn.
TEST_SOURCES = $(wildcard $(DIR)/tests/*.c)
TEST_BINS    = $(TEST_SOURCES:$(DIR)/tests/%.c=$(BUILD_DIR)/tests/%)

LANGS = lua c bash
# }}}

# {{{ Public targets
.PHONY: all runner specs clean test help

all: runner specs

runner: $(POOL_BIN)

specs:
	@for lang in $(LANGS); do \
	  echo "  → langs/$$lang"; \
	  $(MAKE) -s -C $(DIR)/langs/$$lang DIR=$(DIR) || exit 1; \
	done

clean:
	rm -f $(POOL_BIN)
	rm -rf $(BUILD_DIR)
	@for lang in $(LANGS); do \
	  $(MAKE) -s -C $(DIR)/langs/$$lang DIR=$(DIR) clean || true; \
	done

test: all $(TEST_BINS)
	@echo
	@fail=0; \
	for t in $(TEST_BINS); do \
	  $$t || fail=1; \
	  echo; \
	done; \
	if [ -x $(DIR)/scripts/run-tests.sh ]; then \
	  $(DIR)/scripts/run-tests.sh $(DIR) || fail=1; \
	fi; \
	exit $$fail

help:
	@echo "SoraMech build:"
	@echo "  make          — build runner + specs (release)"
	@echo "  make DEBUG=1  — debug info, no optimization"
	@echo "  make STRICT=1 — promote warnings to errors"
	@echo "  make clean    — remove every built artifact"
	@echo "  make test     — run integration tests (when 311 lands)"
# }}}

# {{{ Build rules
$(POOL_BIN): $(POOL_OBJECTS)
	@echo "  → $(notdir $@)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  → $(patsubst $(DIR)/%,%,$<)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# Per-test dependency lines. A test binary needs its own .o plus
# the .o files for the source(s) it exercises. As each test lands,
# add one line here naming the dependencies (no globbing — explicit
# is clearer than clever for tests).
$(BUILD_DIR)/tests/009-slot-store-test:    $(BUILD_DIR)/src/009-slot-store.o \
                                           $(BUILD_DIR)/src/015-large-value-heap.o
$(BUILD_DIR)/tests/015-large-value-heap-test: $(BUILD_DIR)/src/015-large-value-heap.o
$(BUILD_DIR)/tests/010-graph-loader-test: $(BUILD_DIR)/src/010-graph-loader.o \
                                          $(BUILD_DIR)/libs/json/json.o
$(BUILD_DIR)/tests/011-spec-registry-test: $(BUILD_DIR)/src/011-spec-registry.o
$(BUILD_DIR)/tests/301-pool-test:         $(BUILD_DIR)/libs/task-pool/pool.o
$(BUILD_DIR)/tests/303-pool-spec-init-test: $(BUILD_DIR)/libs/task-pool/pool.o \
                                            $(BUILD_DIR)/src/011-spec-registry.o
$(BUILD_DIR)/tests/314-json-test:         $(BUILD_DIR)/libs/json/json.o

# Pattern: link a test binary. $^ collects the .o files declared
# above plus the test's own .o file.
$(BUILD_DIR)/tests/%: $(BUILD_DIR)/tests/%.o
	@echo "  → $(patsubst $(DIR)/%,%,$@)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)
# }}}
