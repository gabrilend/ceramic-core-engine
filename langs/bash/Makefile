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

CFLAGS = -shared -fPIC -O2 -Wall -I$(DIR)/langs
# }}}

# {{{ Build rules
.PHONY: all clean

all: spec.so

spec.so: spec.c $(DIR)/langs/lang-spec.h
	@$(CC) $(CFLAGS) -o $@ $< -ldl

clean:
	@rm -f spec.so
# }}}
