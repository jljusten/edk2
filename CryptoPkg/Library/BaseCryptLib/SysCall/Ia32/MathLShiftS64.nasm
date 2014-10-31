;------------------------------------------------------------------------------
; @file
;   64-bit Math Worker Function.
;   Shifts a 64-bit signed value left by a certain number of bits.
;
; Copyright (c) 2009 - 2014, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

SECTION .text

;------------------------------------------------------------------------------
;
; void __cdecl __ashldi3 (void)
;
;------------------------------------------------------------------------------
global ASM_PFX(__ashldi3)
ASM_PFX(__ashldi3):
    ;
    ; Handle shifting of 64 or more bits (return 0)
    ;
    cmp     cl, 64
    jae     ReturnZero

    ;
    ; Handle shifting of between 0 and 31 bits
    ;
    cmp     cl, 32
    jae     More32
    shld    edx, eax, cl
    shl     eax, cl
    ret

    ;
    ; Handle shifting of between 32 and 63 bits
    ;
More32:
    mov     edx, eax
    xor     eax, eax
    and     cl, 31
    shl     edx, cl
    ret

ReturnZero:
    xor     eax,eax
    xor     edx,edx
    ret
