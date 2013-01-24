/** @file
  OVMF support for QEMU system firmware flash device

  Copyright (c) 2009 - 2013, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "PiDxe.h"
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Guid/EventGroup.h>

#include "QemuFlash.h"

#define WRITE_BYTE_CMD           0x10
#define BLOCK_ERASE_CMD          0x20
#define CLEAR_STATUS_CMD         0x50
#define READ_STATUS_CMD          0x70
#define READ_DEVID_CMD           0x90
#define BLOCK_ERASE_CONFIRM_CMD  0xd0
#define READ_ARRAY_CMD           0xff

#define CLEARED_ARRAY_STATUS  0x00


STATIC EFI_EVENT   mVirtualAddressChangeEvent = NULL;
STATIC UINT8       *mNvStorageVariableBase = NULL;
STATIC UINTN       mFdBlockSize = 0;


/**
  Notification function of EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE.

  This is a notification function registered on
  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE event.
  It converts pointer to new virtual address.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
STATIC
VOID
EFIAPI
QemuFlashFvbAddressChangeEvent (
  IN EFI_EVENT        Event,
  IN VOID             *Context
  )
{
  EfiConvertPointer (0x0, (VOID **) &mNvStorageVariableBase);
}


STATIC
volatile UINT8*
QemuFlashPtr (
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset
  )
{
  return mNvStorageVariableBase + (Lba * mFdBlockSize) + Offset;
}


/**
  Determines if the QEMU flash memory device is present.

  @retval FALSE   The QEMU flash device is not present.
  @retval TRUE    The QEMU flash device is present.

**/
BOOLEAN
QemuFlashDetected (
  VOID
  )
{
  STATIC BOOLEAN  DetectionRan = FALSE;
  STATIC BOOLEAN  FlashDetected = FALSE;
  volatile UINT8  *Ptr;

  if (!DetectionRan) {
    UINT8 OriginalUint8;
    UINT8 ProbeUint8;

    Ptr = QemuFlashPtr (0, 0);

    OriginalUint8 = *Ptr;
    *Ptr = CLEAR_STATUS_CMD;
    ProbeUint8 = *Ptr;
    if (OriginalUint8 != CLEAR_STATUS_CMD &&
        ProbeUint8 == CLEAR_STATUS_CMD) {
      DEBUG ((EFI_D_INFO, "QemuFlashDetected => FD behaves as RAM\n"));
      *Ptr = OriginalUint8;
    } else {
      *Ptr = READ_STATUS_CMD;
      ProbeUint8 = *Ptr;
      if (ProbeUint8 == READ_STATUS_CMD) {
        DEBUG ((EFI_D_INFO, "QemuFlashDetected => FD behaves as RAM\n"));
        *Ptr = OriginalUint8;
      } else if (ProbeUint8 == CLEARED_ARRAY_STATUS) {
        FlashDetected = TRUE;
      }
    }

    DetectionRan = TRUE;

    DEBUG ((EFI_D_INFO, "QemuFlashDetected => %a\n",
                        FlashDetected ? "Yes" : "No"));
  }

  return FlashDetected;
}


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
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN        UINTN                               NumBytes,
  IN        UINT8                               *Buffer
  )
{
  volatile UINT8  *Ptr;
  UINTN           Loop;

  if (!QemuFlashDetected ()) {
    return;
  }

  //
  // Only write to the first 64k. We don't bother saving the FTW Spare
  // block into the flash memory.
  //
  if (Lba > 0) {
    return;
  }

  //
  // Program flash
  //
  Ptr = QemuFlashPtr (Lba, Offset);
  for (Loop = 0; Loop < NumBytes; Loop++) {
    *Ptr = WRITE_BYTE_CMD;
    *Ptr = Buffer[Loop];
    Ptr++;
  }

  //
  // Restore flash to read mode
  //
  if (NumBytes > 0) {
    *Ptr = READ_ARRAY_CMD;
  }
}

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
  )
{
  volatile UINT8  *Ptr;

  while (1) {
    EFI_LBA  Lba;
    UINTN    Count;

    Lba = VA_ARG (List, EFI_LBA);
    if (Lba == EFI_LBA_LIST_TERMINATOR) {
      break;
    }

    Count = VA_ARG (List, UINTN);

    if (Lba == 0 && Count >= 1) {
      UINTN Loop;

      for (Loop = 0; Loop < SIZE_64KB; Loop += SIZE_4KB) {
        Ptr = QemuFlashPtr (Lba, Loop);
        *Ptr = BLOCK_ERASE_CMD;
        *Ptr = BLOCK_ERASE_CONFIRM_CMD;
      }
    }
  }
}


EFI_STATUS
QemuFlashFvbInitialize (
  VOID
  )
{
  EFI_STATUS Status;

  mNvStorageVariableBase =
    (UINT8*)(UINTN) PcdGet32 (PcdOvmfFlashNvStorageVariableBase);
  mFdBlockSize = PcdGet32 (PcdOvmfFirmwareBlockSize);


  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  QemuFlashFvbAddressChangeEvent,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mVirtualAddressChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

