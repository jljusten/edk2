;------------------------------------------------------------------------------
;
; Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT PeiTemporaryRamMigration
  AREA PeiCore_LowLevel, CODE, READONLY

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; PeiTemporaryRamMigration (
;   IN  PEI_CORE_TEMPORARY_RAM_TRANSITION  *TempRamTransitionData
;   );
;
; @param[in]  r0    Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
;
; @return     None  This routine does not return
;------------------------------------------------------------------------------
PeiTemporaryRamMigration

    ;
    ; We store the TempRamTransitionData pointer in r6. By the
    ; Arm calling convention we should normally save r6, but we
    ; won't be returning to the caller, so we don't need to save it.
    ; By the same rule, the TemporaryRamMigration PPI call should
    ; preserve r6 for us.
    ;
    mov     r6, r0

    ;
    ; Setup parameters and call TemporaryRamSupport->TemporaryRamMigration
    ;   (r0)      PeiServices
    ;   (r2,r3)   TemporaryMemoryBase
    ;   (stack)   PermanentMemoryBase
    ;   (stack)   CopySize
    ;
    add     r7, r6, #4
    ldmia   r7, {r0-r5}
    stmfd   sp!, {r3, r4, r5}
    mov     r3, r2
    mov     r2, r1

    ;
    ; (r6) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION on stack
    ;
    ; Adjusted based on stack change made during
    ; TemporaryRamSupport->TemporaryRamMigration call
    ;
    ldr     r4, [r6]
    mov     r5, sp
    sub     r6, r6, r5
    mov     lr, pc
    bx      r4
    mov     r0, sp
    add     r6, r6, r0
    add     sp, sp, #0xc

    ;
    ; Setup parameters and call PeiTemporaryRamMigrated
    ;   (r0) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    ;
    mov     r0, r6
    bl      ASM_PFX(PeiTemporaryRamMigrated)

  END
