#!/usr/bin/env python
## @file ConvertMasmToNasm.py
# This script assists with conversion of MASM assembly syntax to NASM
#
#  Copyright (c) 2007 - 2014, Intel Corporation
#
#  All rights reserved. This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

##
# Import Modules
#
import os.path
import re
import sys
from optparse import OptionParser

class ConvertAsmApp:

  # Version and Copyright
  VersionNumber = "0.01"
  __version__ = "%prog Version " + VersionNumber
  __copyright__ = "Copyright (c) 2007 - 2014, Intel Corporation  All rights reserved."
  __usage__ = "%prog [options] source.asm [destination.nasm]"

  def __init__(self):
    (self.Opt, self.Args) = self.ProcessCommandLine()
    self.inputFilename = self.Args[0]
    assert (os.path.splitext(self.inputFilename)[1] != '.nasm')
    lines = open(self.inputFilename).readlines()
    self.inputFileBase = os.path.basename(self.inputFilename)
    self.inputFileRe = self.inputFileBase.replace('.', r'\.')
    self.inputFileRe = re.compile(self.inputFileRe, re.IGNORECASE)

    if len(self.Args) == 1:
      self.outputFilename = os.path.splitext(self.inputFilename)[0] + '.nasm'
    else:
      self.outputFilename = self.Args[1]
    self.outputFileBase = os.path.basename(self.outputFilename)
    if self.outputFilename == '-':
        self.output = sys.stdout
    else:
        self.output = open(self.outputFilename, 'w')
    if not self.Opt.quiet:
      print 'Converting:', self.inputFilename, '->', self.outputFilename
    self.Convert(lines)
    if self.outputFilename != '-':
        self.output.close()

  def ProcessCommandLine(self):
    Parser = OptionParser(description=self.__copyright__,
                          version=self.__version__,
                          prog=sys.argv[0],
                          usage=self.__usage__
                         )
    Parser.add_option("-q", "--quiet", action="store_true", type=None, help="Disable all messages except FATAL ERRORS.")
    #Parser.add_option("-v", "--verbose", action="store_true", type=None, help="Turn on verbose output with informational messages printed, "\
    #                                                                           "including library instances selected, final dependency expression, "\
    #                                                                           "and warning messages, etc.")
    #Parser.add_option("-d", "--debug", action="store", type="int", help="Enable debug messages at specified level.")

    (Opt, Args)=Parser.parse_args()

    if len(Args) not in (1, 2):
      Parser.print_help()
      print '!! Incorrect number of parameters !!'
      sys.exit(-1)

    if not Opt.quiet:
      print self.__copyright__
      Parser.print_version()

    return (Opt, Args)


  endOfLineRe = re.compile(r'''
                             \s* ( ; .* )? \n $
                           ''',
                           re.VERBOSE | re.MULTILINE
                          )
  begOfLineRe = re.compile(r'''
                             \s*
                           ''',
                           re.VERBOSE
                          )

  def Convert(self, lines):
    self.emitLine(';')
    self.emitLine('; %s: Automatically generated from %s' % (sys.argv[0], self.inputFileBase))
    self.emitLine(';')
    self.anonLabelCount = -1
    output = self.output
    for line in lines:
      mo = self.endOfLineRe.search(line)
      if mo is None:
          endOfLine = ''
      else:
          endOfLine = mo.group()
      mo = self.begOfLineRe.search(line)
      if mo is None:
        raise Exception
      begOfLine = mo.group()
      oldAsm = line[len(begOfLine):len(line)-len(endOfLine)]
      self.TranslateAsm(begOfLine, oldAsm, endOfLine)
      #print '%s%s' % (line, newLine),

  procDeclRe = re.compile(r'''
                            (\w[\w0-9]*) \s+
                            PROC \s*
                            (USES (?: \s+ \w[\w0-9]* )+)?
                            \s* $
                          ''',
                          re.VERBOSE | re.IGNORECASE
                         )

  procEndRe = re.compile(r'''
                           (\w[\w0-9]*) \s+
                           ENDP
                           \s* $
                         ''',
                          re.VERBOSE | re.IGNORECASE
                        )

  externdefRe = re.compile(r'''
                             EXTERNDEF \s+ C \s+
                             (\w[\w0-9]*) \s* : \s* (\w+)
                             \s* $
                           ''',
                           re.VERBOSE | re.IGNORECASE
                          )

  whitespaceRe = re.compile(r'\s+', re.MULTILINE)

  def MatchAndSetMo(self, regexp, string):
    self.mo = regexp.match(string)
    return self.mo is not None

  def TranslateAsm(self, begOfLine, oldAsm, endOfLine):
    assert(oldAsm.strip() == oldAsm)

    #print '-%s' % oldAsm

    endOfLine = self.inputFileRe.sub(self.outputFileBase, endOfLine)

    oldOp = oldAsm.split()
    if len(oldOp) >= 1:
        oldOp = oldOp[0]
    else:
        oldOp = ''

    dataEmits = {
        'db': 'db',
        'dw': 'dw',
        'dd': 'dd',
        }

    print 'oldOp', oldOp

    if oldAsm == '':
      newAsm = oldAsm
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)
    elif oldOp in ('#include', ):
      newAsm = oldAsm
      self.emitLine(begOfLine + oldAsm + endOfLine)
    elif oldOp.lower() in ('end', 'title', 'text'):
      newAsm = ''
      self.emitLine(';' + begOfLine + oldAsm + endOfLine)
    elif oldAsm.lower() == '@@:':
      self.anonLabelCount += 1
      self.emitLine(self.anonLabel(self.anonLabelCount) + ':')
    elif oldOp.lower() in ('.code', '.686', '.686p', '.model'):
      newAsm = ''
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)
    elif oldOp.lower() in dataEmits:
      newOp = dataEmits[oldOp.lower()]
      self.emitAsmReplaceOp(begOfLine, oldAsm, oldOp, newOp, endOfLine)
    elif oldAsm.lower() == 'ret':
      for i in range(len(self.uses) - 1, -1, -1):
        register = self.uses[i]
        self.emitLine('    pop     ' + register)
      newAsm = 'ret'
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)
      self.uses = tuple()
    elif oldAsm.lower() == 'retf':
      newAsm = 'lret'
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)
    elif oldAsm.lower() == 'end':
      newAsm = ''
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)
      self.uses = tuple()
    elif self.MatchAndSetMo(self.externdefRe, oldAsm):
      newAsm = ''
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)
    elif self.MatchAndSetMo(self.procDeclRe, oldAsm):
      self.proc = self.mo.group(1)
      self.emitLine('global ASM_PFX(%s)' % self.proc)
      newAsm = 'ASM_PFX(%s):' % self.proc
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)
      uses = self.mo.group(2)
      if uses is not None:
        uses = filter(None, uses[5:].split())
      else:
        uses = tuple()
      self.uses = uses
      for register in self.uses:
        self.emitLine('    push    ' + register)
    elif self.MatchAndSetMo(self.procEndRe, oldAsm):
      newAsm = ''
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)
    else:
      newAsm = oldAsm
      anonLabel = self.anonLabel(self.anonLabelCount)
      newAsm = newAsm.replace('@b', anonLabel)
      newAsm = newAsm.replace('@B', anonLabel)
      anonLabel = self.anonLabel(self.anonLabelCount + 1)
      newAsm = newAsm.replace('@f', anonLabel)
      newAsm = newAsm.replace('@F', anonLabel)
      newAsm = newAsm.replace('@', '_atSym_')
      self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)

    #print '+%s' % newAsm

  def anonLabel(self, count):
    return '%s_al_%04d' % (self.proc, count)

  def emitString(self, string):
    self.output.write(string)

  def emitLine(self, string):
    self.emitString(string + '\n')

  def emitAsmReplaceOp(self, begOfLine, oldAsm, oldOp, newOp, endOfLine):
    newAsm = oldAsm.replace(oldOp, newOp, 1)
    self.emitAsmWithComment(begOfLine, oldAsm, newAsm, endOfLine)

  hexNumRe = re.compile(r'([0-9][0-9a-f]*)h', re.IGNORECASE)

  def emitAsmWithComment(self, begOfLine, oldAsm, newAsm, endOfLine):
    newAsm = self.hexNumRe.sub(r'0x\1', newAsm)
    newLine = begOfLine + newAsm + endOfLine
    if (newLine.strip() != '') or ((oldAsm + endOfLine).strip() == ''):
      self.emitLine(newLine.rstrip())

ConvertAsmApp()
