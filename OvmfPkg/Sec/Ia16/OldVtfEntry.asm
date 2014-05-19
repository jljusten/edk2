;------------------------------------------------------------------------------
; @file
; First code executed by processor after resetting.
;
; Copyright (c) 2008 - 2011, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

BITS    16

ALIGN   32

;GLOBAL ASM_PFX(_ModuleEntryPoint)
;ASM_PFX(_ModuleEntryPoint):
GLOBAL _ModuleEntryPoint
_ModuleEntryPoint:
    jmp     short EarlyBspInitReal16
