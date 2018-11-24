/** @file
  Migrate Stack functions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "BaseLibInternals.h"

/**
  Transfers control to a function after adjusting the stack location.

  Transfers control to the function specified by EntryPoint using the
  new stack location which is adjusted relative to the current stack
  location by adding StackAdjustment to the stack location. The
  EntryPoint is called passing in the parameters specified by Context
  pointer. Context is optional and may be NULL. The function
  EntryPoint must never return.

  If EntryPoint is NULL, then ASSERT().

  @param  EntryPoint    A pointer to function to call with the new stack.
  @param  Context       A pointer to the context to pass into the EntryPoint
                        function.
  @param  StackAdjust   A value to adjust (add to) the stack location.

**/
VOID
EFIAPI
MigrateStack (
  IN      MIGRATE_STACK_ENTRY_POINT     EntryPoint,
  IN      VOID                          *Context,  OPTIONAL
  IN      INTN                          StackAdjustment
  )
{
  ASSERT (EntryPoint != NULL);

  //
  // New stack must be aligned with CPU_STACK_ALIGNMENT
  //
  ASSERT (((UINTN)StackAdjustment & (CPU_STACK_ALIGNMENT - 1)) == 0);

  InternalMigrateStack (EntryPoint, Context, StackAdjustment);

  //
  // InternalMigrateStack () should never return
  //
  ASSERT (FALSE);
  CpuDeadLoop ();
}
