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
# Immediate (:=) assignment so DIR is captured once at parse time
# from the project's own Makefile path. Recursive (?=) assignment
# would re-evaluate $(MAKEFILE_LIST) every time DIR is expanded —
# which becomes wrong after `-include` of build/**/*.d files at
# the bottom of this file, since the .d files become the last
# member of $(MAKEFILE_LIST) and their dir (build/src/, etc.) is
# not the project root.
DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
# }}}

# {{{ Toolchain & flags
CC      ?= gcc

# Shared header search path: every C source can find langs/lang-spec.h
# and any future libs/<name>/ headers via -I flags below.
#
# -MMD -MP makes gcc emit a `.d` file alongside each `.o` recording
# every header that translation unit included. The `-include $(DEPS)`
# below pulls those .d files back in so changing a header
# (e.g. extending box_t with a new field) reliably triggers a
# rebuild of every .o that includes it. Without this, a header
# field change can produce a mixed object set where different
# translation units see different struct layouts — field reads
# through a struct pointer land at wrong offsets and surface as
# bewildering memory-corruption symptoms. (Diagnosed on 319's
# runtime self-construction path after the 248 / 243 layout
# changes to box_t / routing_t.)
CPPFLAGS = -I$(DIR)/langs -I$(DIR)/libs/task-pool -I$(DIR)/libs/json -I$(DIR)/src -MMD -MP

# Default to release; DEBUG and STRICT toggle alternates.
ifeq ($(DEBUG),1)
  CFLAGS = -O0 -g -DDEBUG -Wall -pthread -fPIC
else ifeq ($(STRICT),1)
  CFLAGS = -O2 -Wall -Wextra -Wpedantic -Werror -pthread -fPIC
else
  CFLAGS = -O2 -Wall -pthread -fPIC
endif

LDFLAGS = -ldl -pthread -rdynamic
# -rdynamic exports the runner's symbols to dlopen'd spec.so
# plugins. Specs need this for the issue-319d self-construction
# builtins (runtime_create_box / runtime_connect) — those live in
# src/018-runtime-builtins.c, owned by the runner, and the Lua /
# C / Bash spec.so binaries call back into them at runtime.
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
                                           $(BUILD_DIR)/src/016-unified-allocator.o
$(BUILD_DIR)/tests/015-large-value-heap-test: $(BUILD_DIR)/src/015-large-value-heap.o
$(BUILD_DIR)/tests/016-unified-allocator-test: $(BUILD_DIR)/src/016-unified-allocator.o
$(BUILD_DIR)/tests/010-graph-loader-test: $(BUILD_DIR)/src/010-graph-loader.o \
                                          $(BUILD_DIR)/src/009-slot-store.o \
                                          $(BUILD_DIR)/src/016-unified-allocator.o \
                                          $(BUILD_DIR)/src/011-spec-registry.o \
                                          $(BUILD_DIR)/src/017-box-id.o \
                                          $(BUILD_DIR)/src/018-runtime-builtins.o \
                                          $(BUILD_DIR)/src/020-sentinels.o \
                                          $(BUILD_DIR)/libs/json/json.o
# Every test binary that loads a language spec via dlopen needs the
# runner-side symbols the specs call back into (issue 318's
# sentinel_*, issue 319d's runtime_set_active_context, etc.). The
# pool runner pulls them in through POOL_SOURCES' wildcard; tests
# name them explicitly.
SPEC_LOADER_DEPS = $(BUILD_DIR)/src/020-sentinels.o \
                   $(BUILD_DIR)/src/018-runtime-builtins.o \
                   $(BUILD_DIR)/src/017-box-id.o \
                   $(BUILD_DIR)/src/010-graph-loader.o \
                   $(BUILD_DIR)/src/009-slot-store.o \
                   $(BUILD_DIR)/src/016-unified-allocator.o \
                   $(BUILD_DIR)/libs/json/json.o

$(BUILD_DIR)/tests/011-spec-registry-test: $(BUILD_DIR)/src/011-spec-registry.o \
                                           $(SPEC_LOADER_DEPS)
$(BUILD_DIR)/tests/012-dispatch-test:      $(BUILD_DIR)/src/012-dispatch.o \
                                           $(BUILD_DIR)/src/010-graph-loader.o \
                                           $(BUILD_DIR)/src/011-spec-registry.o \
                                           $(BUILD_DIR)/src/009-slot-store.o \
                                           $(BUILD_DIR)/src/016-unified-allocator.o \
                                           $(BUILD_DIR)/src/013-jsonl-events.o \
                                           $(BUILD_DIR)/src/014-event-queue.o \
                                           $(BUILD_DIR)/src/017-box-id.o \
                                           $(BUILD_DIR)/src/018-runtime-builtins.o \
                                           $(BUILD_DIR)/src/020-sentinels.o \
                                           $(BUILD_DIR)/libs/json/json.o \
                                           $(BUILD_DIR)/libs/task-pool/pool.o
$(BUILD_DIR)/tests/013-jsonl-events-test:  $(BUILD_DIR)/src/013-jsonl-events.o \
                                           $(BUILD_DIR)/libs/json/json.o
$(BUILD_DIR)/tests/014-event-queue-test:   $(BUILD_DIR)/src/014-event-queue.o \
                                           $(BUILD_DIR)/src/013-jsonl-events.o \
                                           $(BUILD_DIR)/libs/json/json.o
$(BUILD_DIR)/tests/301-pool-test:         $(BUILD_DIR)/libs/task-pool/pool.o
$(BUILD_DIR)/tests/303-pool-spec-init-test: $(BUILD_DIR)/libs/task-pool/pool.o \
                                            $(BUILD_DIR)/src/011-spec-registry.o \
                                            $(SPEC_LOADER_DEPS)
$(BUILD_DIR)/tests/306-lua-spec-test:     $(BUILD_DIR)/src/011-spec-registry.o \
                                          $(SPEC_LOADER_DEPS)
$(BUILD_DIR)/tests/317-lua-bridge-test:   $(BUILD_DIR)/src/011-spec-registry.o \
                                          $(SPEC_LOADER_DEPS)
$(BUILD_DIR)/tests/317-text-bridge-test:  $(BUILD_DIR)/src/011-spec-registry.o \
                                          $(SPEC_LOADER_DEPS)
$(BUILD_DIR)/tests/307-c-spec-test:       $(BUILD_DIR)/src/011-spec-registry.o \
                                          $(SPEC_LOADER_DEPS)
$(BUILD_DIR)/tests/308-bash-spec-test:    $(BUILD_DIR)/src/011-spec-registry.o \
                                          $(SPEC_LOADER_DEPS)
$(BUILD_DIR)/tests/314-json-test:         $(BUILD_DIR)/libs/json/json.o
$(BUILD_DIR)/tests/017-box-id-test:       $(BUILD_DIR)/src/017-box-id.o
$(BUILD_DIR)/tests/020-sentinels-test:    $(BUILD_DIR)/src/020-sentinels.o \
                                          $(BUILD_DIR)/libs/json/json.o

# Pattern: link a test binary. $^ collects the .o files declared
# above plus the test's own .o file.
$(BUILD_DIR)/tests/%: $(BUILD_DIR)/tests/%.o
	@echo "  → $(patsubst $(DIR)/%,%,$@)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

# Auto-generated header dependencies (one .d per .o, courtesy of
# -MMD -MP in CPPFLAGS). Wildcard-include whatever .d files
# exist in the build tree — empty on a fresh build, populated
# after the first compile. Once an .o is compiled its .d travels
# with it until clean; touching any header it lists forces a
# rebuild of every dependent .o automatically.
-include $(wildcard $(BUILD_DIR)/src/*.d)
-include $(wildcard $(BUILD_DIR)/libs/task-pool/*.d)
-include $(wildcard $(BUILD_DIR)/libs/json/*.d)
-include $(wildcard $(BUILD_DIR)/tests/*.d)
# }}}
