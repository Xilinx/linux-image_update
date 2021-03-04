# SPDX-License-Identifier: MIT
#
# Copyright (c) 2021 Xilinx, Inc.
#

CC := $(CROSS_COMPILE)gcc
EXEC := image_update
c_SOURCES := $(wildcard *.c)
INCLUDES := $(wildcard *.h)
OBJS := $(patsubst %.c, %.o, $(c_SOURCES))

all: $(EXEC)

$(EXEC): $(c_SOURCES)
	$(CC) $< -o $@

clean:
	rm -rf $(OBJS) image_update
