;------------------------------------------------------------------------------
; @file
;   64-bit Math Worker Function.
;   Divides a 64-bit unsigned value with a 64-bit unsigned value and returns
;   a 64-bit unsigned result.
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

extern ASM_PFX(DivU64x64Remainder)

;------------------------------------------------------------------------------
;
; void __cdecl __udivdi3 (void)
;
;------------------------------------------------------------------------------
global ASM_PFX(__udivdi3)
ASM_PFX(__udivdi3):
    ; Original local stack when calling __udivdi3
    ;               -----------------
    ;               |               |
    ;               |---------------|
    ;               |               |
    ;               |--  Divisor  --|
    ;               |               |
    ;               |---------------|
    ;               |               |
    ;               |--  Dividend --|
    ;               |               |
    ;               |---------------|
    ;               |  ReturnAddr** |
    ;       ESP---->|---------------|
    ;

    ;
    ; Set up the local stack for NULL Reminder pointer
    ;
    xor     eax, eax
    push    eax

    ;
    ; Set up the local stack for Divisor parameter
    ;
    mov     eax, [esp + 20]
    push    eax
    mov     eax, [esp + 20]
    push    eax

    ;
    ; Set up the local stack for Dividend parameter
    ;
    mov     eax, [esp + 20]
    push    eax
    mov     eax, [esp + 20]
    push    eax

    ;
    ; Call native DivU64x64Remainder of BaseLib
    ;
    jmp     ASM_PFX(DivU64x64Remainder)

    ;
    ; Adjust stack
    ;
    add     esp, 20

    ret     16
