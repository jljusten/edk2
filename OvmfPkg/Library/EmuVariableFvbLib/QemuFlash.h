/** @file
  Library to define platform customization functions for a
  Firmare Volume Block driver.

  Copyright (c) 2009 - 2013, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __QEMU_FLASH_LIB__
#define __QEMU_FLASH_LIB__

#include <Protocol/FirmwareVolumeBlock.h>

/**
  Determines if the QEMU flash memory device is present.

  @retval FALSE   The QEMU flash device is not present.
  @retval TRUE    The QEMU flash device is present.

**/
BOOLEAN
QemuFlashDetected (
  VOID
  );


/**
  This function will be called following a call to the
  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL Write function.

  @param[in] Lba      The starting logical block index to written to.
  @param[in] Offset   Offset into the block at which to begin writing.
  @param[in] NumBytes The number of bytes written.
  @param[in] Buffer   Pointer to the buffer that was written.

**/
VOID
QemuFlashFvbDataWritten (
  IN        EFI_LBA                              Lba,
  IN        UINTN                                Offset,
  IN        UINTN                                NumBytes,
  IN        UINT8                                *Buffer
  );


/**
  This function will be called following a call to the
  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL Erase function.

  @param List   The variable argument list as documented for
                the EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL Erase
                function.

**/
VOID
EFIAPI
QemuFlashFvbBlocksErased (
  IN  VA_LIST       List
  );


EFI_STATUS
QemuFlashFvbInitialize (
  VOID
  );


#endif

