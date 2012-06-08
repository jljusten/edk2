;------------------------------------------------------------------------------
; @file
; Transition from 16 bit real mode into 64 bit long mode
;
; Copyright (c) 2008 - 2012, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

;
; NOTE: This file is *NOT USED* in the build. It was used to help create
;       ApStartup.c.
;
;       To assemble:
;       * nasm -o ApStartup ApStartup.asm
;       Then disassemble:
;       * ndisasm -b 16 ApStartup
;       * ndisasm -b 16 -e 6 ApStartup
;       * ndisasm -b 32 -e 32 ApStartup (This -e offset may need adjustment)
;       * ndisasm -b 64 -e 0x83 ApStartup (This -e offset may need adjustment)
;

%define DEFAULT_CR0  0x00000023
%define DEFAULT_CR4  0x640

BITS    16

    jmp     short TransitionFromReal16To32BitFlat

ALIGN   2

Gdtr:
    dw      0x5a5a
    dd      0x5a5a5a5a

;
; Modified:  EAX, EBX
;
TransitionFromReal16To32BitFlat:

    cli
    mov     ax, 0x5a5a
    mov     ds, ax

    mov     bx, Gdtr
o32 lgdt    [ds:bx]

    mov     eax, cr4
    btc     eax, 5
    mov     cr4, eax

    mov     eax, DEFAULT_CR0
    mov     cr0, eax

    jmp     0x5a5a:dword jumpTo32BitAndLandHere
BITS    32
jumpTo32BitAndLandHere:

    mov     eax, DEFAULT_CR4
    mov     cr4, eax

    mov     ax, 0x5a5a
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax

;
; Jump to CpuDxe for IA32
;
    mov     eax, 0x5a5a5a5a
    or      eax, eax
    jz      Transition32FlatTo64Flat
    jmp     eax

;
; Transition to X64
;
Transition32FlatTo64Flat:
    mov     eax, 0x5a5a5a5a
    mov     cr3, eax

    mov     eax, cr4
    bts     eax, 5                      ; enable PAE
    mov     cr4, eax

    mov     ecx, 0xc0000080
    rdmsr
    bts     eax, 8                      ; set LME
    wrmsr

    mov     eax, cr0
    bts     eax, 31                     ; set PG
    mov     cr0, eax                    ; enable paging

;
; Jump to CpuDxe for X64
;
    jmp     0x5a5a:jumpTo64BitAndLandHere
BITS    64
jumpTo64BitAndLandHere:
    mov     rax, 0xcdcdcdcdcdcdcdcd
    jmp     rax

