# langs/lua/Makefile — build the Lua language spec.
#
# Produces spec.so, a small .so the pool runner dlopens at startup.
# Links against LuaJIT 2.1 (override LUA_INCLUDE / LUA_LIB if the
# headers and library live somewhere other than the Debian/Arch
# default).
#
# Invokable standalone (`make -C langs/lua`) or from the top-level
# Makefile via `make specs`. The DIR variable lets the top-level
# pass the project root so the -I path to langs/lang-spec.h resolves.

# {{{ Resolve project root
DIR ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
# }}}

# {{{ Toolchain & flags
CC          ?= gcc
LUA_INCLUDE ?= /usr/include/luajit-2.1
LUA_LIB     ?= /usr/lib

CFLAGS  = -shared -fPIC -O2 -Wall -I$(LUA_INCLUDE) -I$(DIR)/langs
LDFLAGS = -L$(LUA_LIB) -lluajit-5.1
# }}}

# {{{ Build rules
.PHONY: all clean

all: spec.so

spec.so: spec.c $(DIR)/langs/lang-spec.h
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	@rm -f spec.so
# }}}
