#------------------------------------------------------------------------------
#
# Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

.text
.align 5

GCC_ASM_EXPORT(PeiTemporaryRamMigration)

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# PeiTemporaryRamMigration (
#   IN  PEI_CORE_TEMPORARY_RAM_TRANSITION  *TempRamTransitionData
#   );
#
# @param[in]  x0    Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
#
# @return     None  This routine does not return
#------------------------------------------------------------------------------
ASM_PFX(PeiTemporaryRamMigration):

    #
    # We store the TempRamTransitionData pointer in x19. By the
    # AArch64 calling convention we should normally save x19, but we
    # won't be returning to the caller, so we don't need to save it.
    # By the same rule, the TemporaryRamMigration PPI call should
    # preserve x19 for us.
    #
    mov     x19, x0

    #
    # Setup parameters and call TemporaryRamSupport->TemporaryRamMigration
    #   (rcx) PeiServices
    #   (rdx) TemporaryMemoryBase
    #   (r8)  PermanentMemoryBase
    #   (r9)  CopySize
    #
    ldp     x0, x1, [x19, 0x08]
    ldp     x2, x3, [x19, 0x18]

    #
    # (x19) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION on stack
    #
    # Adjusted based on stack change made during
    # TemporaryRamSupport->TemporaryRamMigration call
    #
    ldr     x4, [x19]
    mov     x5, sp
    sub     x19, x19, x5
    blr     x4
    mov     x5, sp
    add     x19, x19, x5

    #
    # Setup parameters and call PeiTemporaryRamMigrated
    #   (x0) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    #
    mov     x0, x19
    bl      ASM_PFX(PeiTemporaryRamMigrated)
