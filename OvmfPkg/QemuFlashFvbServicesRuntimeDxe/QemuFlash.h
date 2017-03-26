/** @file
  OVMF support for QEMU system firmware flash device

  Copyright (c) 2009 - 2017, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __QEMU_FLASH_H__
#define __QEMU_FLASH_H__

extern UINT8 *mFlashBase;

/**
  Detect if QEMU Flash is available and writable

  Note: This function does not use read or write global variables.

  @retval TRUE  Flash is writable

**/
BOOLEAN
QemuFlashDetected (
  VOID
  );


/**
  Read from QEMU Flash

  @param[in] Lba      The starting logical block index to read from.
  @param[in] Offset   Offset into the block at which to begin reading.
  @param[in] NumBytes On input, indicates the requested read size. On
                      output, indicates the actual number of bytes read
  @param[in] Buffer   Pointer to the buffer to read into.

**/
RETURN_STATUS
QemuFlashRead (
  IN        EFI_LBA                              Lba,
  IN        UINTN                                Offset,
  IN        UINTN                                *NumBytes,
  IN        UINT8                                *Buffer
  );


/**
  Write to QEMU Flash

  @param[in] Lba      The starting logical block index to write to.
  @param[in] Offset   Offset into the block at which to begin writing.
  @param[in] NumBytes On input, indicates the requested write size. On
                      output, indicates the actual number of bytes written
  @param[in] Buffer   Pointer to the data to write.

**/
RETURN_STATUS
QemuFlashWrite (
  IN        EFI_LBA                              Lba,
  IN        UINTN                                Offset,
  IN        UINTN                                *NumBytes,
  IN        UINT8                                *Buffer
  );


/**
  Erase a QEMU Flash block

  @param Lba    The logical block index to erase.

**/
RETURN_STATUS
QemuFlashEraseBlock (
  IN   EFI_LBA      Lba
  );


/**
  Initializes QEMU flash memory support

  @retval RETURN_WRITE_PROTECTED   The QEMU flash device is not present.
  @retval RETURN_SUCCESS           The QEMU flash device is supported.

**/
RETURN_STATUS
QemuFlashInitialize (
  VOID
  );


VOID
QemuFlashConvertPointers (
  VOID
  );

#endif

