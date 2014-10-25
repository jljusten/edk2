;------------------------------------------------------------------------------
;*
;*   Copyright (c) 2008 - 2013, Intel Corporation. All rights reserved.<BR>
;*   This program and the accompanying materials
;*   are licensed and made available under the terms and conditions of the BSD License
;*   which accompanies this distribution.  The full text of the license may be found at
;*   http://opensource.org/licenses/bsd-license.php
;*
;*   THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
;*   WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;*
;*    CpuAsm.nasm
;*
;*   Abstract:
;*
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID
; SetCodeSelector (
;   UINT16 Selector
;   );
;------------------------------------------------------------------------------
global ASM_PFX(SetCodeSelector)
ASM_PFX(SetCodeSelector):
    sub     rsp, 0x10
    lea     rax, [setCodeSelectorLongJump]
    mov     [rsp], rax
    mov     [rsp+8], cx
    jmp     qword far [rsp]
setCodeSelectorLongJump:
    add     rsp, 0x10
    ret

;------------------------------------------------------------------------------
; VOID
; SetDataSelectors (
;   UINT16 Selector
;   );
;------------------------------------------------------------------------------
global ASM_PFX(SetDataSelectors)
ASM_PFX(SetDataSelectors):
    mov     ss, cx
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx
    ret

