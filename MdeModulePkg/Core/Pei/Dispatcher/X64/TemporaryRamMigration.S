#------------------------------------------------------------------------------
#
# Copyright (c) 2018 - 2019, Intel Corporation. All rights reserved.<BR>
#
# This program and the accompanying materials are licensed and made
# available under the terms and conditions of the BSD License which
# accompanies this distribution. The full text of the license may be
# found at http://opensource.org/licenses/bsd-license.php.
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS"
# BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER
# EXPRESS OR IMPLIED.
#
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# PeiTemporaryRamMigration (
#   IN  PEI_CORE_TEMPORARY_RAM_TRANSITION  *TempRamTransitionData
#   );
#
# @param[in]  RCX   Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
#
# @return     None  This routine does not return
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(PeiTemporaryRamMigration)
ASM_PFX(PeiTemporaryRamMigration):

    #
    # We never return from this call
    #
    add     $8, %rsp

    #
    #   (rax) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    #
    mov     %rcx, %rax

    #
    # We'll store the new location of TempRamTransitionData after
    # migration in rbx. By the X64 calling convention we should
    # normally save rbx, but we won't be returning to the caller, so
    # we don't need to save it. By the same rule, the
    # TemporaryRamMigration PPI call should preserve rbx for us.
    #
    mov     %rcx, %rbx
    add     0x18(%rax), %rbx
    sub     0x10(%rax), %rbx

    #
    # Setup parameters and call TemporaryRamSupport->TemporaryRamMigration
    #   (rcx) PeiServices
    #   (rdx) TemporaryMemoryBase
    #   (r8)  PermanentMemoryBase
    #   (r9)  CopySize
    #
    mov     0x08(%rax), %rcx
    mov     0x10(%rax), %rdx
    mov     0x18(%rax), %r8
    mov     0x20(%rax), %r9
    callq   *(%rax)

    #
    # Setup parameters and call PeiTemporaryRamMigrated
    #   (rcx) Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
    #
    mov     %rbx, %rcx
    call    ASM_PFX(PeiTemporaryRamMigrated)
