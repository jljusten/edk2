;------------------------------------------------------------------------------
;
; Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
;
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
;   TemporaryRamMigration.nasm
;
; Abstract:
;
;   Implementation of PeiTemporaryRamMigration for X64
;
;------------------------------------------------------------------------------

#include <Base.h>

    SECTION .text

extern ASM_PFX(PeiTemporaryRamMigrated)
extern ASM_PFX(CpuBreakpoint)

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; PeiTemporaryRamMigration (
;   IN  PEI_CORE_TEMPORARY_RAM_TRANSITION  *TempRamTransitionData
;   );
;
; @param[in]  EAX   Pointer to PEI_CORE_TEMPORARY_RAM_TRANSITION
;
; @return     None  This routine does not return
;------------------------------------------------------------------------------
global ASM_PFX(PeiTemporaryRamMigration)
ASM_PFX(PeiTemporaryRamMigration):

    push rbp
    mov rbp, rsp

    mov rax, rcx

    ; We'll store the new location of TempRamTransitionData after
    ; migration in rbx. By the X64 calling convention we should
    ; normally be save rbx, but we won't be returning to the caller,
    ; so we don't need to save it. By the same rule, the
    ; TemporaryRamMigration PPI call should preserve rbx for us.
    mov rbx, rcx
    add rbx, [rax + 0x18]
    sub rbx, [rax + 0x10]

    ;   (rcx) PeiServices
    ;   (rdx) TemporaryMemoryBase
    ;   (r8)  PermanentMemoryBase
    ;   (r9)  CopySize
    mov rcx, [rax + 0x08]
    mov rdx, [rax + 0x10]
    mov r8, [rax + 0x18]
    mov r9, [rax + 0x20]
    call [rax + 0x00]

    ;
    ; Setup parameters and call SecCoreStartupWithStack
    ;   [esp]   return address for call
    ;   [esp+4] BootFirmwareVolumePtr
    ;   [esp+8] TopOfCurrentStack
    ;
    mov rcx, rbx
    call    ASM_PFX(PeiTemporaryRamMigrated)
