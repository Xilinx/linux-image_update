# SPDX-License-Identifier: MIT
#
# Copyright (c) 2021 Xilinx, Inc.
#

CROSS_COMPILE ?= aarch64-linux-gnu-
CC ?= $(CROSS_COMPILE)gcc
INSTALL_PROGRAM ?= install
bindir ?= /usr/bin
EXEC := image_update
c_SOURCES := $(wildcard *.c)
INCLUDES := $(wildcard *.h)
OBJS := $(patsubst %.c, %.o, $(c_SOURCES))

all: $(EXEC)

$(EXEC): $(c_SOURCES)
	$(CC) $< -o $@

install: $(EXEC)
	$(INSTALL_PROGRAM) -D -m 755 $(EXEC) $(DESTDIR)$(bindir)/$(EXEC)

clean:
	rm -rf $(OBJS) image_update
