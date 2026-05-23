# langs/c/Makefile — build the C language spec.
#
# Produces spec.so, a small .so the pool runner dlopens at startup.
# Links against -ldl only; the C spec's runtime work is dlopen +
# dlsym, no third-party runtime needed.
#
# Invokable standalone (`make -C langs/c`) or from the top-level
# Makefile via `make specs`.

# {{{ Resolve project root
DIR ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
# }}}

# {{{ Toolchain & flags
CC ?= gcc

CFLAGS  = -shared -fPIC -O2 -Wall -I$(DIR)/langs -I$(DIR)/libs/json -I$(DIR)/src
LDFLAGS = -ldl
# }}}

# {{{ Build rules
# spec.c folds libs/json/json.c directly into spec.so for the
# JSON-input acceptance path (issue 307 slice unblocking 312's
# deferred Lua output side). Same pattern the Lua spec uses for
# its own JSON bridges.
.PHONY: all clean

all: spec.so

spec.so: spec.c $(DIR)/langs/lang-spec.h $(DIR)/libs/json/json.c $(DIR)/libs/json/json.h
	@$(CC) $(CFLAGS) -o $@ spec.c $(DIR)/libs/json/json.c $(LDFLAGS)

clean:
	@rm -f spec.so
# }}}
