#/******************************************************************************
#* Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
#* SPDX-License-Identifier: MIT
#******************************************************************************/


CROSS_COMP := aarch64-linux-gnu
CC := $(CROSS_COMP)-gcc
LINKER := $(CROSS_COMP)-ld
EXEC := image_update
c_SOURCES := $(wildcard *.c)
INCLUDES := $(wildcard *.h)
OBJS := $(patsubst %.c, %.o, $(c_SOURCES))

all: $(EXEC)

$(EXEC): $(c_SOURCES)
	$(CC) $< -o $@

clean:
	rm -rf $(OBJS) image_update
