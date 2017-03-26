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

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Uefi.h>

#include "QemuFlash.h"


/**
  Initializes QEMU flash memory support

  @retval !RETURN_SUCCESS          Failed to set the PCD
  @retval RETURN_SUCCESS           The constructor was successful

**/
RETURN_STATUS
EFIAPI
DetectFlashConstructor (
  VOID
  )
{
  if (QemuFlashDetected ()) {
    return PcdSetBoolS (PcdOvmfFlashVariablesEnable, TRUE);
  }

  return RETURN_SUCCESS;
}
