/**@file
  Platform PEI Variable Store Initialization

  Copyright (c) 2006 - 2017, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made
  available under the terms and conditions of the BSD License which
  accompanies this distribution. The full text of the license may be
  found at http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS"
  BASIS, WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER
  EXPRESS OR IMPLIED.

**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#include "Platform.h"

#define OVMF_FVB_BLOCK_SIZE (FixedPcdGet32 (PcdFlashNvStorageFtwSpareSize))
#define OVMF_FVB_SIZE (2 * FixedPcdGet32 (PcdFlashNvStorageFtwSpareSize))
#define OVMF_FV_HEADER_LENGTH OFFSET_OF (FVB_FV_HDR_AND_VARS_TEMPLATE, VarHdr)


VOID
ReserveEmuVariableNvStore (
  )
{
  EFI_PHYSICAL_ADDRESS VariableStore;
  UINT32               Offset;
  RETURN_STATUS        PcdStatus;

  //
  // Allocate storage for NV variables early on so it will be
  // at a consistent address.  Since VM memory is preserved
  // across reboots, this allows the NV variable storage to survive
  // a VM reboot.
  //
  VariableStore =
    (EFI_PHYSICAL_ADDRESS)(UINTN)
      AllocateAlignedRuntimePages (
        EFI_SIZE_TO_PAGES (OVMF_FVB_SIZE),
        PcdGet32 (PcdFlashNvStorageFtwSpareSize)
        );
  ASSERT (VariableStore != 0);
  ASSERT ((VariableStore + OVMF_FVB_SIZE) <= MAX_ADDRESS);
  DEBUG ((EFI_D_INFO,
          "Reserved variable store memory: 0x%lX; size: %dkb\n",
          VariableStore,
          (2 * PcdGet32 (PcdFlashNvStorageFtwSpareSize)) / 1024
        ));
  PcdStatus = PcdSet64S (PcdEmuVariableNvStoreReserved, VariableStore);
  ASSERT_RETURN_ERROR (PcdStatus);

  //
  // Initialize the main FV header and variable store header
  //
  PcdStatus = PcdSet64S (
                PcdFlashNvStorageVariableBase64,
                VariableStore);
  ASSERT_RETURN_ERROR (PcdStatus);

  //
  // Initialize the Fault Tolerant Write data area
  //
  Offset = PcdGet32 (PcdVariableStoreSize);
  PcdStatus = PcdSet32S (
                PcdFlashNvStorageFtwWorkingBase,
                VariableStore + Offset);
  ASSERT_RETURN_ERROR (PcdStatus);

  //
  // Initialize the Fault Tolerant Write spare block
  //
  Offset = FixedPcdGet32 (PcdFlashNvStorageFtwSpareSize);
  PcdStatus = PcdSet32S (
                PcdFlashNvStorageFtwSpareBase,
                VariableStore + Offset);
  ASSERT_RETURN_ERROR (PcdStatus);
}


VOID
SetupVariables (
  VOID
  )
{
  RETURN_STATUS        PcdStatus;

  if (PcdGetBool (PcdOvmfFlashVariablesEnable)) {
    //
    // If flash is enabled, then set the variable PCD to point
    // directly at flash.
    //
    PcdStatus = PcdSet64S (
      PcdFlashNvStorageVariableBase64,
      (UINTN) PcdGet32 (PcdOvmfFlashNvStorageVariableBase)
      );
    ASSERT_RETURN_ERROR (PcdStatus);
    PcdStatus = PcdSet32S (
      PcdFlashNvStorageFtwWorkingBase,
      PcdGet32 (PcdOvmfFlashNvStorageFtwWorkingBase)
      );
    ASSERT_RETURN_ERROR (PcdStatus);
    PcdStatus = PcdSet32S (
      PcdFlashNvStorageFtwSpareBase,
      PcdGet32 (PcdOvmfFlashNvStorageFtwSpareBase)
      );
    ASSERT_RETURN_ERROR (PcdStatus);
  } else {
    //
    // If flash is not enabled, then allocate a buffer and initialize
    // it if necessary for variable operations.
    //
    ReserveEmuVariableNvStore ();
  }
}
