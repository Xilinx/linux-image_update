# linux-image_update
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2021 Xilinx, Inc.
#

Introduction
linux-image_update.git - This is a user space application that updates the alternate image on QSPI while linux is running
from the current running image. This would help users to upgrade Boot Firmware in Qspi from remote locations.

The software consists of image_update.c and Makefile.

Usage: image_update <path of image file>
  The <input image> must be copied / downloaded to file system on linux and its path must be provided as an argument
  to image_update utility.
  
  image_update -p (--print) prints persistent state registers.
    This gives information about which image is running and which would be the "next booting image".
    
  image_update -h (--help) prints this menu:
    Usage: image_update <path of image file>
           image_update -p prints persistent state registers.
		       image_update --print prints persistent state registers
		       image_update -h prints this menu
		       image_update --help prints this menu.
