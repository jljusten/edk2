;------------------------------------------------------------------------------
;
; Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

#include <Base.h>

    SECTION .text

extern ASM_PFX(PeiTemporaryRamMigrated)

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; PeiTemporaryRamMigration (
;   IN  PEI_CORE_TEMPORARY_RAM_TRANSITION  *TempRamTransitionData
;   );
;
; @param[in]  RCX   Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
;
; @return     None  This routine does not return
;------------------------------------------------------------------------------
global ASM_PFX(PeiTemporaryRamMigration)
ASM_PFX(PeiTemporaryRamMigration):

    ;
    ; We never return from this call
    ;
    add     rsp, 8

    ;
    ;   (rax) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    ;
    mov     rax, rcx

    ;
    ; We store the TempRamTransitionData pointer in rbx. By the X64
    ; calling convention we should normally save rbx, but we won't be
    ; returning to the caller, so we don't need to save it. By the
    ; same rule, the TemporaryRamMigration PPI call should preserve
    ; rbx for us.
    ;
    mov     rbx, rcx

    ;
    ; Setup parameters and call TemporaryRamSupport->TemporaryRamMigration
    ;   (rcx) PeiServices
    ;   (rdx) TemporaryMemoryBase
    ;   (r8)  PermanentMemoryBase
    ;   (r9)  CopySize
    ;
    mov     rcx, [rax + 0x08]
    mov     rdx, [rax + 0x10]
    mov     r8, [rax + 0x18]
    mov     r9, [rax + 0x20]

    ;
    ; (rbx) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION on stack
    ;
    ; Adjusted based on stack change made during
    ; TemporaryRamSupport->TemporaryRamMigration call
    ;
    sub     rbx, rsp
    call    [rax + 0x00]
    add     rbx, rsp

    ;
    ; Setup parameters and call PeiTemporaryRamMigrated
    ;   (rcx) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    ;
    mov     rcx, rbx
    call    ASM_PFX(PeiTemporaryRamMigrated)
