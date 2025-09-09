# Makefile for cfetch

SHELL := /bin/sh

PREFIX ?= /usr/local
DESTDIR ?=

CC ?= cc
DEFS ?= -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2 -Wall -Wextra -std=c11 $(DEFS)
LDFLAGS ?=

BIN ?= cfetch

DEFAULT_MAIN := $(shell ls -t main*.c 2>/dev/null | head -n1)
MAIN ?= $(DEFAULT_MAIN)
ifeq ($(strip $(MAIN)),)
MAIN := main.c
endif

all: $(BIN)

$(BIN): $(MAIN)
	$(CC) $(CFLAGS) '$(MAIN)' -o '$(BIN)' $(LDFLAGS)

clean:
	rm -f '$(BIN)'

install: $(BIN)
	install -Dm755 '$(BIN)' '$(DESTDIR)$(PREFIX)/bin/$(BIN)'

uninstall:
	rm -f '$(DESTDIR)$(PREFIX)/bin/$(BIN)'

user-config:
	mkdir -p "$$HOME/.config/cfetch"
	touch "$$HOME/.config/cfetch/config"

.PHONY: all clean install uninstall user-config
