/** @file
  EFI PEI Core temporary RAM migration

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>

This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "PeiMain.h"

/**
  To call the TemporaryRamMigration PPI, we might not be able to rely
  on C code's handling of the stack. In these cases we use an assembly
  function to make sure the old stack is not used after the
  TemporaryRamMigration PPI is used.

  After calling the TemporaryRamMigration PPI, this function calls
  PeiTemporaryRamMigrated.

  This C based function provides an implementation that may work for
  some architectures.

  @param TempRamTransitionData
**/
VOID
EFIAPI
PeiTemporaryRamMigration (
  IN  PEI_CORE_TEMPORARY_RAM_TRANSITION  *TempRamTransitionData
  )
{
  //
  // Temporary Ram Support PPI is provided by platform, it will copy
  // temporary memory to permanent memory and do stack switching.
  // After invoking Temporary Ram Support PPI, the following code's
  // stack is in permanent memory.
  //
  TempRamTransitionData->TemporaryRamMigration (
                           TempRamTransitionData->PeiServices,
                           TempRamTransitionData->TemporaryMemoryBase,
                           TempRamTransitionData->PermanentMemoryBase,
                           TempRamTransitionData->CopySize
                           );

  PeiTemporaryRamMigrated(TempRamTransitionData);
}
