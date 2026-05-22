# langs/bash/Makefile — build the Bash language spec.
#
# Produces spec.so, a small .so the pool runner dlopens at startup.
# Bash boxes run out-of-process via Unix domain sockets, so spec.so
# itself links nothing beyond libc; the heavy lifting lives in
# bash-server.sh (created with issue 308).
#
# Invokable standalone (`make -C langs/bash`) or from the top-level
# Makefile via `make specs`.

# {{{ Resolve project root
DIR ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
# }}}

# {{{ Toolchain & flags
CC ?= gcc

CFLAGS = -shared -fPIC -O2 -Wall -I$(DIR)/langs -I$(DIR)/libs/json
# }}}

# {{{ Build rules
# spec.c folds libs/json/json.c straight into spec.so for the
# native↔JSON bridges (issue 317) — keeps the spec self-contained,
# same pattern as the Lua and C specs.
.PHONY: all clean

all: spec.so

spec.so: spec.c $(DIR)/langs/lang-spec.h $(DIR)/libs/json/json.c $(DIR)/libs/json/json.h
	@$(CC) $(CFLAGS) -o $@ spec.c $(DIR)/libs/json/json.c -ldl

clean:
	@rm -f spec.so
# }}}
