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
#include <Guid/VariableFormat.h>
#include <Guid/SystemNvDataGuid.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#include "Platform.h"

typedef struct {

  EFI_FIRMWARE_VOLUME_HEADER FvHdr;
  EFI_FV_BLOCK_MAP_ENTRY     EndBlockMap;
  VARIABLE_STORE_HEADER      VarHdr;

} FVB_FV_HDR_AND_VARS_TEMPLATE;

#define OVMF_FVB_BLOCK_SIZE (FixedPcdGet32 (PcdFlashNvStorageFtwSpareSize))
#define OVMF_FVB_SIZE (2 * FixedPcdGet32 (PcdFlashNvStorageFtwSpareSize))
#define OVMF_FV_HEADER_LENGTH OFFSET_OF (FVB_FV_HDR_AND_VARS_TEMPLATE, VarHdr)


/**
  Check the integrity of firmware volume header.

  @param[in] FwVolHeader - A pointer to a firmware volume header

  @retval  EFI_SUCCESS   - The firmware volume is consistent
  @retval  EFI_NOT_FOUND - The firmware volume has been corrupted.

**/
STATIC EFI_STATUS
ValidateFvHeader (
  IN EFI_FIRMWARE_VOLUME_HEADER   *FwVolHeader
  )
{
  UINT16  Checksum;

  //
  // Verify the header revision, header signature, length
  // Length of FvBlock cannot be 2**64-1
  // HeaderLength cannot be an odd number
  //
  if ((FwVolHeader->Revision != EFI_FVH_REVISION) ||
      (FwVolHeader->Signature != EFI_FVH_SIGNATURE) ||
      (FwVolHeader->FvLength != OVMF_FVB_SIZE) ||
      (FwVolHeader->HeaderLength != OVMF_FV_HEADER_LENGTH)
      ) {
    DEBUG ((EFI_D_INFO, "EMU Variable FVB: Basic FV headers were invalid\n"));
    return EFI_NOT_FOUND;
  }
  //
  // Verify the header checksum
  //
  Checksum = CalculateSum16((VOID*) FwVolHeader, FwVolHeader->HeaderLength);

  if (Checksum != 0) {
    DEBUG ((EFI_D_INFO, "EMU Variable FVB: FV checksum was invalid\n"));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}


/**
  Initializes the FV Header and Variable Store Header
  to support variable operations.

  @param[in]  Ptr - Location to initialize the headers

**/
STATIC VOID
InitializeFvAndVariableStoreHeaders (
  IN  VOID   *Ptr
  )
{
  //
  // Templates for authenticated variable FV header
  //
  STATIC FVB_FV_HDR_AND_VARS_TEMPLATE FvAndAuthenticatedVarTemplate = {
    { // EFI_FIRMWARE_VOLUME_HEADER FvHdr;
      // UINT8                     ZeroVector[16];
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },

      // EFI_GUID                  FileSystemGuid;
      EFI_SYSTEM_NV_DATA_FV_GUID,

      // UINT64                    FvLength;
      OVMF_FVB_SIZE,

      // UINT32                    Signature;
      EFI_FVH_SIGNATURE,

      // EFI_FVB_ATTRIBUTES_2      Attributes;
      0x4feff,

      // UINT16                    HeaderLength;
      OVMF_FV_HEADER_LENGTH,

      // UINT16                    Checksum;
      0,

      // UINT16                    ExtHeaderOffset;
      0,

      // UINT8                     Reserved[1];
      {0},

      // UINT8                     Revision;
      EFI_FVH_REVISION,

      // EFI_FV_BLOCK_MAP_ENTRY    BlockMap[1];
      {
        {
          2, // UINT32 NumBlocks;
          OVMF_FVB_BLOCK_SIZE  // UINT32 Length;
        }
      }
    },
    // EFI_FV_BLOCK_MAP_ENTRY     EndBlockMap;
    { 0, 0 }, // End of block map
    { // VARIABLE_STORE_HEADER      VarHdr;
        // EFI_GUID  Signature;     // need authenticated variables for secure boot
        EFI_AUTHENTICATED_VARIABLE_GUID,

      // UINT32  Size;
      (
        FixedPcdGet32 (PcdVariableStoreSize) -
        OFFSET_OF (FVB_FV_HDR_AND_VARS_TEMPLATE, VarHdr)
      ),

      // UINT8   Format;
      VARIABLE_STORE_FORMATTED,

      // UINT8   State;
      VARIABLE_STORE_HEALTHY,

      // UINT16  Reserved;
      0,

      // UINT32  Reserved1;
      0
    }
  };

  EFI_FIRMWARE_VOLUME_HEADER  *Fv;

  //
  // Copy the template structure into the location
  //
  CopyMem (
    Ptr,
    (VOID*) &FvAndAuthenticatedVarTemplate,
    sizeof (FvAndAuthenticatedVarTemplate)
    );

  //
  // Update the checksum for the FV header
  //
  Fv = (EFI_FIRMWARE_VOLUME_HEADER*) Ptr;
  Fv->Checksum = CalculateCheckSum16 (Ptr, Fv->HeaderLength);
}


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

  VOID *Ptr = (VOID*)(UINTN) VariableStore;

  if (EFI_ERROR (ValidateFvHeader (Ptr))) {
    SetMem (Ptr, OVMF_FVB_SIZE, 0xff);
    InitializeFvAndVariableStoreHeaders (Ptr);
  }

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
