;------------------------------------------------------------------------------
;
; Copyright (c) 2010 - 2013, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Module Name:
;
;   AsmFuncs.nasm
;
; Abstract:
;
;   Debug interrupt handle functions.
;
;------------------------------------------------------------------------------

#include "DebugException.h"

;
; InterruptProcess()
;
extern ASM_PFX(InterruptProcess)

global ASM_PFX(Exception0Handle)
global ASM_PFX(TimerInterruptHandle)
global ASM_PFX(ExceptionStubHeaderSize)

%macro AGENT_HANDLER_SIGNATURE 0
  db   0x41, 0x47, 0x54, 0x48       ; SIGNATURE_32('A','G','T','H')
%endmacro

%macro DECLARE_HANDLER 2
AGENT_HANDLER_SIGNATURE
ASM_PFX(%1 %+ Handle):
    cli
    push    eax
    mov     eax, %2
    jmp     dword [CommonEntryAddr]
%endmacro

SECTION .data

ASM_PFX(ExceptionStubHeaderSize): dd ASM_PFX(Exception1Handle) - ASM_PFX(Exception0Handle)
CommonEntryAddr: DD CommonEntry

SECTION .text

%assign i 0
%rep 20
DECLARE_HANDLER Exception %+ i, i
%assign i i+1
%endrep

DECLARE_HANDLER TimerInterrupt, 32

CommonEntry:
;
; +---------------------+
; +    EFlags           +
; +---------------------+
; +    CS               +
; +---------------------+
; +    EIP              +
; +---------------------+
; +    Error Code       +
; +---------------------+
; + EAX / Vector Number +
; +---------------------+
; +    EBP              +
; +---------------------+ <-- EBP
;
    cmp     eax, DEBUG_EXCEPT_DOUBLE_FAULT
    je      NoExtrPush
    cmp     eax, DEBUG_EXCEPT_INVALID_TSS
    je      NoExtrPush
    cmp     eax, DEBUG_EXCEPT_SEG_NOT_PRESENT
    je      NoExtrPush
    cmp     eax, DEBUG_EXCEPT_STACK_FAULT
    je      NoExtrPush
    cmp     eax, DEBUG_EXCEPT_GP_FAULT
    je      NoExtrPush
    cmp     eax, DEBUG_EXCEPT_PAGE_FAULT
    je      NoExtrPush
    cmp     eax, DEBUG_EXCEPT_ALIGNMENT_CHECK
    je      NoExtrPush

    push    dword [esp]
    mov     dword [esp + 4], 0

NoExtrPush:

    push    ebp
    mov     ebp, esp        ; save esp in ebp
    ;
    ; Make stack 16-byte alignment to make sure save fxrstor later
    ;
    and     esp, 0xfffffff0
    sub     esp, 12

    ; store UINT32  Edi, Esi, Ebp, Ebx, Edx, Ecx, Eax;
    push    dword [ebp + 4]  ; original eax
    push    ebx
    push    ecx
    push    edx
    mov     ebx, eax         ; save vector in ebx
    mov     eax, ebp
    add     eax, 4 * 6
    push    eax              ; original ESP
    push    dword [ebp]  ; EBP
    push    esi
    push    edi

    ;; UINT32  Cr0, Cr1, Cr2, Cr3, Cr4;
    ;; insure FXSAVE/FXRSTOR is enabled in CR4...
    ;; ... while we're at it, make sure DE is also enabled...
    mov     eax, cr4
    push    eax       ; push cr4 firstly
    or      eax, 0x208
    mov     cr4, eax
    mov     eax, cr3
    push    eax
    mov     eax, cr2
    push    eax
    push    0         ; cr0 will not saved???
    mov     eax, cr0
    push    eax

    xor     ecx, ecx
    mov     ecx, Ss
    push    ecx
    mov     ecx, Cs
    push    ecx
    mov     ecx, Ds
    push    ecx
    mov     ecx, Es
    push    ecx
    mov     ecx, Fs
    push    ecx
    mov     ecx, Gs
    push    ecx

    ;; EIP
    mov     ecx, [ebp + 4 * 3] ; EIP
    push    ecx

    ;; UINT32  Gdtr[2], Idtr[2];
    sub  esp, 8
    sidt [esp]
    sub  esp, 8
    sgdt [esp]

    ;; UINT32  Ldtr, Tr;
    xor  eax, eax
    str  ax
    push eax
    sldt ax
    push eax

    ;; EFlags
    mov     ecx, [ebp + 4 * 5]
    push    ecx

    ;; UINT32  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    mov     eax, dr7
    push    eax

    ;; clear Dr7 while executing debugger itself
    xor     eax, eax
    mov     dr7, eax

    ;; Dr6
    mov     eax, dr6
    push    eax

    ;; insure all status bits in dr6 are clear...
    xor     eax, eax
    mov     dr6, eax

    mov     eax, dr3
    push    eax
    mov     eax, dr2
    push    eax
    mov     eax, dr1
    push    eax
    mov     eax, dr0
    push    eax

    ;; FX_SAVE_STATE_IA32 FxSaveState;
    sub esp, 512
    mov edi, esp
    db 0xf, 0xae, 00000111y ;fxsave [edi]

    ;; save the exception data
    push    dword [ebp + 8]

    ;; Clear Direction Flag
    cld

    ; call the C interrupt process function
    push    esp     ; Structure
    push    ebx     ; vector
    call    ASM_PFX(InterruptProcess)
    add     esp, 8

    ; skip the exception data
    add     esp, 4

    ;; FX_SAVE_STATE_IA32 FxSaveState;
    mov esi, esp
    db 0xf, 0xae, 00001110y ; fxrstor [esi]
    add esp, 512

    ;; UINT32  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    pop     eax
    mov     dr0, eax
    pop     eax
    mov     dr1, eax
    pop     eax
    mov     dr2, eax
    pop     eax
    mov     dr3, eax
    ;; skip restore of dr6.  We cleared dr6 during the context save.
    add     esp, 4
    pop     eax
    mov     dr7, eax

    ;; set EFlags
    pop     dword [ebp + 4 * 5]  ; set EFLAGS in stack

    ;; UINT32  Ldtr, Tr;
    ;; UINT32  Gdtr[2], Idtr[2];
    ;; Best not let anyone mess with these particular registers...
    add     esp, 24

    ;; UINT32  Eip;
    pop     dword [ebp + 4 * 3]   ; set EIP in stack

    ;; UINT32  Gs, Fs, Es, Ds, Cs, Ss;
    ;; NOTE - modified segment registers could hang the debugger...  We
    ;;        could attempt to insulate ourselves against this possibility,
    ;;        but that poses risks as well.
    ;;
    pop     gs
    pop     fs
    pop     es
    pop     ds
    pop     dword [ebp + 4 * 4]    ; set CS in stack
    pop     ss

    ;; UINT32  Cr0, Cr1, Cr2, Cr3, Cr4;
    pop     eax
    mov     cr0, eax
    add     esp, 4    ; skip for Cr1
    pop     eax
    mov     cr2, eax
    pop     eax
    mov     cr3, eax
    pop     eax
    mov     cr4, eax

    ;; restore general register
    pop     edi
    pop     esi
    pop     dword [ebp]         ; save updated ebp
    pop     dword [ebp + 4]     ; save updated esp
    pop     edx
    pop     ecx
    pop     ebx
    pop     eax

    mov     esp, ebp
    pop     ebp         ; restore ebp maybe updated
    pop     esp         ; restore esp maybe updated
    sub     esp, 4 * 3  ; restore interupt pushced stack

    iretd

