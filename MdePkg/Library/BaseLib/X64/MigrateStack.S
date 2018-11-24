#------------------------------------------------------------------------------
#
# Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php.
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
# Module Name:
#
#   MigrateStack.nasm
#
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------
# Routine Description:
#
#   Routine for migrating the stack with 1 context parameter
#
# Arguments:
#
#   (rcx) EntryPoint    - Entry point with new stack.
#   (rdx) Context       - Parameter for entry point.
#   (r8)  StackAdjust   - The INTN value to add to the stack pointer
#
# Returns:
#
#   None
#
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(InternalMigrateStack)
ASM_PFX(InternalMigrateStack):
    #
    # Since we are migrating the stack to another location, the
    # current call stack will have made the 32 byte 'shadow space'
    # available. We'll remove the current return location from the
    # stack, migrate to the new location, pop the return location on
    # the new stack, and then jump to the entry point function.
    #
    pop     %rax
    add     %r8, %rsp
    push    %rax

    #
    # Move context into the first parameter, and jump to the entry
    # point.
    #
    xchg    %rcx, %rdx
    jmp     %rdx
