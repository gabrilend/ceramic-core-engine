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

CFLAGS  = -shared -fPIC -O2 -Wall -I$(DIR)/langs
LDFLAGS = -ldl
# }}}

# {{{ Build rules
.PHONY: all clean

all: spec.so

spec.so: spec.c
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	@rm -f spec.so
# }}}
