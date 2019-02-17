;------------------------------------------------------------------------------
;
; Copyright (c) 2018 - 2019, Intel Corporation. All rights reserved.<BR>
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
; @param[in]  Stack Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
;
; @return     None  This routine does not return
;------------------------------------------------------------------------------
global ASM_PFX(PeiTemporaryRamMigration)
ASM_PFX(PeiTemporaryRamMigration):

    ;
    ; We never return from this call
    ;
    add     esp, 4

    ;
    ;   (eax) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    ;
    mov     eax, [esp]

    ;
    ; We store the TempRamTransitionData pointer in ebx. By the IA32
    ; calling convention we should normally save ebx, but we won't be
    ; returning to the caller, so we don't need to save it. By the
    ; same rule, the TemporaryRamMigration PPI call should preserve
    ; ebx for us.
    ;
    mov     ebx, [esp]

    ;
    ; Setup parameters and call TemporaryRamSupport->TemporaryRamMigration
    ;   32-bit PeiServices
    ;   64-bit TemporaryMemoryBase
    ;   64-bit PermanentMemoryBase
    ;   32-bit CopySize
    ;
    push    DWORD [eax + 0x18] ; CopySize
    push    DWORD [eax + 0x14] ; PermanentMemoryBase Upper 32-bit
    push    DWORD [eax + 0x10] ; PermanentMemoryBase Lower 32-bit
    push    DWORD [eax + 0x0c] ; TemporaryMemoryBase Upper 32-bit
    push    DWORD [eax + 0x08] ; TemporaryMemoryBase Lower 32-bit
    push    DWORD [eax + 0x04] ; PeiServices

    ;
    ; (ebx) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION on stack
    ;
    ; Adjusted based on stack change made during
    ; TemporaryRamSupport->TemporaryRamMigration call
    ;
    sub     ebx, esp
    call    DWORD [eax + 0x00]
    add     ebx, esp
    add     esp, 0x18

    ;
    ; Setup parameters and call PeiTemporaryRamMigrated
    ;   32-bit Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    ;
    push    ebx
    call    ASM_PFX(PeiTemporaryRamMigrated)
