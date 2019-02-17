#------------------------------------------------------------------------------
#
#  Copyright (c) 2018 - 2019, Intel Corporation. All rights reserved.<BR>
#
#  This program and the accompanying materials are licensed and made
#  available under the terms and conditions of the BSD License which
#  accompanies this distribution. The full text of the license may be
#  found at http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS"
#  BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER
#  EXPRESS OR IMPLIED.
#
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# PeiTemporaryRamMigration (
#   IN  PEI_CORE_TEMPORARY_RAM_TRANSITION  *TempRamTransitionData
#   );
#
# @param[in]  Stack Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
#
# @return     None  This routine does not return
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(PeiTemporaryRamMigration)
ASM_PFX(PeiTemporaryRamMigration):

    #
    # We never return from this call
    #
    add     $4, %esp

    #
    #   (eax) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    #
    mov     (%esp), %eax

    #
    # We'll store the new location of TempRamTransitionData after
    # migration in ebx. By the IA32 calling convention we should
    # normally save ebx, but we won't be returning to the caller, so
    # we don't need to save it. By the same rule, the
    # TemporaryRamMigration PPI call should preserve ebx for us.
    #
    mov     (%esp), %ebx
    add     0x10(%eax), %ebx
    sub     0x08(%eax), %ebx

    #
    # Setup parameters and call TemporaryRamSupport->TemporaryRamMigration
    #   32-bit PeiServices
    #   64-bit TemporaryMemoryBase
    #   64-bit PermanentMemoryBase
    #   32-bit CopySize
    #
    pushl   0x18(%eax) # CopySize
    pushl   0x14(%eax) # PermanentMemoryBase Upper 32-bit
    pushl   0x10(%eax) # PermanentMemoryBase Lower 32-bit
    pushl   0x0c(%eax) # TemporaryMemoryBase Upper 32-bit
    pushl   0x08(%eax) # TemporaryMemoryBase Lower 32-bit
    pushl   0x04(%eax) # PeiServices
    calll   *(%eax)
    sub     0x18, %esp

    #
    # Setup parameters and call PeiTemporaryRamMigrated
    #   32-bit Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    #
    push    %ebx
    call    ASM_PFX(PeiTemporaryRamMigrated)
