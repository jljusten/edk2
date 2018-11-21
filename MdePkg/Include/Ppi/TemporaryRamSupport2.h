/** @file
  This file declares Temporary RAM Support PPI.
  This Ppi provides the service that migrates temporary RAM into permanent memory.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  @par Revision Reference:
  This PPI is introduced in PI Version X.

**/

#ifndef __TEMPORARY_RAM_SUPPORT2_H__
#define __TEMPORARY_RAM_SUPPORT2_H__

#define EFI_PEI_TEMPORARY_RAM_SUPPORT2_PPI_GUID  \
  { 0xb650afa1, 0x020f, 0x4ed2, {0xba, 0x1d, 0xe8, 0xd7, 0x4f, 0xc4, 0xf8, 0xb2} }


/**
  This callback function allows the PEI Core to resume execution after
  the temporary RAM has been migrated into the permanent memory.

  With callback *must* be called with the new stack in permanent
  memory active.

  @param Context        A pointer to undefined context data that
                        the callback function may require.

**/
typedef
VOID
(EFIAPI * TEMPORARY_RAM_MIGRATION_CALLBACK)(
  IN VOID               *Context
);


/**
  This service of the EFI_PEI_TEMPORARY_RAM_SUPPORT_PPI that migrates
  temporary RAM into permanent memory.

  @param PeiServices            Pointer to the PEI Services Table.
  @param TemporaryMemoryBase    Source Address in temporary memory from
                                which the SEC or PEIM will copy the
                                Temporary RAM contents.
  @param PermanentMemoryBase    Destination Address in permanent memory
                                into which the SEC or PEIM will copy the
                                Temporary RAM contents.
  @param CopySize               Amount of memory to migrate from
                                temporary to permanent memory.
  @param Callback               A callback function that will be called
                                after the stack was migrated to permanent
                                memory.
  @param Context                A pointer to undefined context data that
                                the callback function may require.

  @retval EFI_SUCCESS           This will never be returned. In the
                                success case, the Callback function
                                should not return.
  @retval EFI_INVALID_PARAMETER PermanentMemoryBase + CopySize > TemporaryMemoryBase when
                                TemporaryMemoryBase > PermanentMemoryBase.
  @retval EFI_INVALID_PARAMETER Callback is NULL.
  @retval EFI_NOT_STARTED       The Callback function returned.

**/
typedef
EFI_STATUS
(EFIAPI * TEMPORARY_RAM_MIGRATION2)(
  IN CONST EFI_PEI_SERVICES             **PeiServices,
  IN EFI_PHYSICAL_ADDRESS               TemporaryMemoryBase,
  IN EFI_PHYSICAL_ADDRESS               PermanentMemoryBase,
  IN UINTN                              CopySize,
  IN TEMPORARY_RAM_MIGRATION_CALLBACK   Callback,
  IN VOID                               *Context
);

///
/// This service abstracts the ability to migrate contents of the platform early memory store.
/// Note: The name EFI_PEI_TEMPORARY_RAM_SUPPORT_PPI is different from the current PI 1.2 spec.
///       This PPI was optional.
///
typedef struct {
  TEMPORARY_RAM_MIGRATION2   TemporaryRamMigration;
} EFI_PEI_TEMPORARY_RAM_SUPPORT2_PPI;

extern EFI_GUID gEfiTemporaryRamSupport2PpiGuid;

#endif
