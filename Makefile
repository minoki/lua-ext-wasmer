# Makefile for macOS

LUA= /usr/local
LUAINC= $(LUA)/include
LUALIB= $(LUA)/lib
LUABIN= $(LUA)/bin

WASMER= ../wasmer
WASMERINC= $(WASMER)/lib/runtime-c-api
WASMERLIB= $(WASMER)/target/release/

CC= clang
CFLAGS= -std=c99 $(INCS) $(WARN) -O2 -Wno-gnu-empty-struct $G
# On Linux, set LDFLAGS=-shared
LDFLAGS= -bundle -undefined dynamic_lookup
WARN= -pedantic -Wall
INCS= -I$(LUAINC) -I$(WASMERINC)
LIBS= -L$(WASMERLIB) -lwasmer_runtime_c_api

T= wasmer.so
OBJS= wasmer-lua.o

all: $T

$T: $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $T

# macOS binary
libwasmer_runtime_c_api.dylib:
	curl -LO https://github.com/wasmerio/wasmer/releases/download/0.8.0/libwasmer_runtime_c_api.dylib

# Linux?
libwasmer_runtime_c_api.so:
	curl -LO https://github.com/wasmerio/wasmer/releases/download/0.8.0/libwasmer_runtime_c_api.so

# Windows
wasmer_runtime_c_api.dll:
	curl -LO https://github.com/wasmerio/wasmer/releases/download/0.8.0/wasmer_runtime_c_api.dll

# curl -LO https://github.com/wasmerio/wasmer/releases/download/0.8.0/wasmer-darwin-amd64.tar.gz
