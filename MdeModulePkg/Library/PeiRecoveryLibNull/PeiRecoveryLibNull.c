/** @file
  Null Recovery Library instance. 
  This library class defines a set of methods related do recovery.

Copyright (c) 2006 - 2008, Intel Corporation. <BR>
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include <PiPei.h>

/**
  Calling this function causes the system do recovery.
  
  @retval EFI_UNSUPPORTED       Recovery is not supported.
**/
EFI_STATUS
EFIAPI
PeiRecoverFirmware (
  VOID
  )
{
  return EFI_UNSUPPORTED;
}

