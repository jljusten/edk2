;------------------------------------------------------------------------------
; @file
;   64-bit Math Worker Function.
;   Multiplies a 64-bit signed or unsigned value by a 64-bit signed or unsigned value
;   and returns a 64-bit result
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

extern ASM_PFX(MultS64x64)

;------------------------------------------------------------------------------
;
; void __cdecl __mulll (void)
;
;------------------------------------------------------------------------------
global ASM_PFX(_mulll)
ASM_PFX(__mulll):
    ; Original local stack when calling __mulll
    ;               -----------------
    ;               |               |
    ;               |---------------|
    ;               |               |
    ;               |--Multiplier --|
    ;               |               |
    ;               |---------------|
    ;               |               |
    ;               |--Multiplicand-|
    ;               |               |
    ;               |---------------|
    ;               |  ReturnAddr** |
    ;       ESP---->|---------------|
    ;

    ;
    ; Set up the local stack for Multiplicand parameter
    ;
    mov     eax, [esp + 16]
    push    eax
    mov     eax, [esp + 16]
    push    eax

    ;
    ; Set up the local stack for Multiplier parameter
    ;
    mov     eax, [esp + 16]
    push    eax
    mov     eax, [esp + 16]
    push    eax

    ;
    ; Call native MulS64x64 of BaseLib
    ;
    jmp     ASM_PFX(MultS64x64)

    ;
    ; Adjust stack
    ;
    add     esp, 16

    ret     16
