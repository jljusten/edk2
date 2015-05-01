## @file
#  Unit tests for AutoGen.UniClassObject
#
#  Copyright (c) 2015, Intel Corporation. All rights reserved.<BR>
#
#  This program and the accompanying materials
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
import os
import unittest

import codecs

import TestTools

from Common.Misc import PathClass
import AutoGen.UniClassObject as BtUni

from Common import EdkLogger
EdkLogger.InitializeForUnitTest()

class Tests(TestTools.BaseToolsTest):

    SampleData = u'''
        #langdef en-US "English"
        #string STR_A #language en-US "STR_A for en-US"
    '''

    def EncodeToFile(self, extension, encoding, string=None):
        if string is None:
            string = self.SampleData
        data = codecs.encode(string, encoding)
        path = 'input' + extension
        self.WriteTmpFile(path, data)
        return PathClass(self.GetTmpFilePath(path))

    def UnicodeErrorFailure(self, extension, encoding, shouldFail):
        msg = 'UnicodeError should '
        if not shouldFail:
            msg += 'not '
        msg += 'be generated for '
        msg += '%s data in a %s file' % (encoding, extension)
        self.fail(msg)

    def CheckFile(self, extension, encoding, shouldFail, string=None):
        path = self.EncodeToFile(extension, encoding, string)
        try:
            BtUni.UniFileClassObject([path])
            if not shouldFail:
                return
        except UnicodeError:
            if shouldFail:
                return
            else:
                self.UnicodeErrorFailure(extension, encoding, shouldFail)
        except Exception:
            pass

        self.UnicodeErrorFailure(extension, encoding, shouldFail)

    def CheckUniBadEnc(self, encoding):
        self.CheckFile('.uni', encoding, shouldFail=True)

    def testAsciiInUniFile(self):
        self.CheckUniBadEnc('ascii')

    def testUtf8InUniFile(self):
        self.CheckUniBadEnc('utf_8')

    def testUtf16beInUniFile(self):
        self.CheckUniBadEnc('utf_16_be')

    def testUtf16leInUniFile(self):
        #
        # utf_16_le does not include the BOM (Byte Order Mark). Therefore,
        # this should be considered invalid.
        #
        self.CheckUniBadEnc('utf_16_le')

    def testUtf16InUniFile(self):
        self.CheckFile('.uni', 'utf_16', shouldFail=False)

TheTestSuite = TestTools.MakeTheTestSuite(locals())

if __name__ == '__main__':
    allTests = TheTestSuite()
    unittest.TextTestRunner().run(allTests)
