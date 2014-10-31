;------------------------------------------------------------------------------
; @file
;   64-bit Math Worker Function.
;   Divides a 64-bit unsigned value by another 64-bit unsigned value and returns
;   the 64-bit unsigned remainder
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
; void __cdecl __umoddi3 (void)
;
;------------------------------------------------------------------------------
global ASM_PFX(__umoddi3)
ASM_PFX(__umoddi3):
    ; Original local stack when calling __umoddi3
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
    ; Set up the local stack for Reminder pointer
    ;
    sub     esp, 8
    push    esp

    ;
    ; Set up the local stack for Divisor parameter
    ;
    mov     eax, [esp + 28]
    push    eax
    mov     eax, [esp + 28]
    push    eax

    ;
    ; Set up the local stack for Dividend parameter
    ;
    mov     eax, [esp + 28]
    push    eax
    mov     eax, [esp + 28]
    push    eax

    ;
    ; Call native DivU64x64Remainder of BaseLib
    ;
    jmp     ASM_PFX(DivU64x64Remainder)

    ;
    ; Put the Reminder in EDX:EAX as return value
    ;
    mov     eax, [esp + 20]
    mov     edx, [esp + 24]

    ;
    ; Adjust stack
    ;
    add     esp, 28

    ret     16
