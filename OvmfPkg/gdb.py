#!/usr/bin/env python
#
# Copyright (c) 2014, Intel Corporation.  All rights reserved.
#
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
# 
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

import os
import re
import sys
import time

assert(len(sys.argv) == 2)
map_file = sys.argv[1]
assert(os.path.exists(map_file))

f = open(map_file, 'rb')
data = f.read()
f.close()

out_file = os.path.splitext(map_file)[0] + '.gdb'
out = open(out_file, 'wb')

regex = re.compile(r'''
    (\w+) \s \(Fixed \s Memory \s Address, \s BaseAddress=(0x[0-9a-f]+), [^\n]+ \n
    ^ [^\n]+ \n
    \( IMAGE= ([^)\n]+) \)
    ''', re.VERBOSE|re.MULTILINE|re.IGNORECASE)


while True:
    mo = regex.search(data)
    if mo:
        filename = os.path.splitext(mo.group(3))[0] + '.debug'
        address = mo.group(2)
        print >> out, 'add-symbol-file', filename, address
        data = data[mo.end():]
    else:
        break

out.close()

print 'gdb script:', out_file

sys.exit(0)
