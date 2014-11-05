;------------------------------------------------------------------------------
;
; Copyright (c) 2014, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Abstract:
;
;   Switch the stack from temporary memory to permenent memory.
;
;------------------------------------------------------------------------------

    SECTION .text

extern ASM_PFX(SwapStack)

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; Pei2LoaderSwitchStack (
;   VOID
;   )
;------------------------------------------------------------------------------
global ASM_PFX(Pei2LoaderSwitchStack)
ASM_PFX(Pei2LoaderSwitchStack):
    jmp     Loader2PeiSwitchStack

;------------------------------------------------------------------------------
; UINT32
; EFIAPI
; Loader2PeiSwitchStack (
;   VOID
;   )
;------------------------------------------------------------------------------
global ASM_PFX(Loader2PeiSwitchStack)
ASM_PFX(Loader2PeiSwitchStack):
    ; Save current contexts
    push    dword exit
    pushfd
    cli
    pushad
    sub     esp, 8
    sidt    [esp]

    ; Load new stack
    push    esp
    call    ASM_PFX(SwapStack)
    mov     esp, eax

    ; Restore previous contexts
    lidt    [esp]
    add     esp, 8
    popad
    popfd
exit:
    ret

