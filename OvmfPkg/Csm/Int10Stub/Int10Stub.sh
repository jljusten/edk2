#!/bin/sh
###
# @file
# Installs an INT 10 stub which allows some UEFI OS's with a legacy INT 10
# dependency to boot.
#
# Copyright (c) 2013 - 2014, Intel Corporation. All rights reserved.<BR>
#
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
###

set -e

DST=`dirname $0`/`basename $0 .sh`
SRC=$DST.asm

#
# Assemble the source file
#
nasm $SRC -o $DST

#
# Use hexdump and xargs to convert to C code
#
hexdump -v -e '/1 "0x%02x, "' $DST | xargs -n 16 echo " "

#
# Remove the intermediate assembly output file
#
rm $DST
