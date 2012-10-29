/** @file

  Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>

#include <IndustryStandard/PeImage.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/LoadLinuxLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>


typedef struct {
  MEMMAP_DEVICE_PATH          MemMapDevPath;
  EFI_DEVICE_PATH_PROTOCOL    EndDevPath;
} MEMMAP_IMAGE_DEVICE_PATH;

MEMMAP_IMAGE_DEVICE_PATH mMemmapImageDevicePathTemplate = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_MEMMAP_DP,
      {
        (UINT8)(sizeof (MEMMAP_DEVICE_PATH)),
        (UINT8)(sizeof (MEMMAP_DEVICE_PATH) >> 8)
      }
    },
    EfiBootServicesData,
    (EFI_PHYSICAL_ADDRESS) 0,
    (EFI_PHYSICAL_ADDRESS) 0,
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      END_DEVICE_PATH_LENGTH,
      0
    }
  }
};

/**
  Determines if a buffer looks like a PE/COFF image.

  @param  Buffer    The buffer containing a potential PE/COFF image.
  @param  Size      The size of the buffer

  @retval TRUE      The buffer appears to be a PE/COFF image
  @retval FALSE     The buffer does not appear to be a PE/COFF image

**/
BOOLEAN
LooksLikeAPeCoffImage (
  IN VOID         *Buffer,
  IN UINTN        Size
  )
{
  EFI_IMAGE_DOS_HEADER                  *DosHdr;
  EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION   Hdr;

  ASSERT (Buffer   != NULL);

  DosHdr = (EFI_IMAGE_DOS_HEADER *)Buffer;
  if (DosHdr->e_magic == EFI_IMAGE_DOS_SIGNATURE) {
    //
    // DOS image header is present, so read the PE header after the DOS image header.
    //
    Hdr.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)((UINTN) Buffer + (UINTN) ((DosHdr->e_lfanew) & 0x0ffff));
  } else {
    //
    // DOS image header is not present, so PE header is at the image base.
    //
    Hdr.Pe32 = (EFI_IMAGE_NT_HEADERS32 *) Buffer;
  }

  //
  // Does the signature show a PE32 or TE image?
  //
  if ((Hdr.Te->Signature == EFI_TE_IMAGE_HEADER_SIGNATURE) ||
      (Hdr.Pe32->Signature == EFI_IMAGE_NT_SIGNATURE)) {
    return TRUE;
  } else {
    return FALSE;
  }
}


STATIC
EFI_STATUS
TryRunningPeCoffImage (
  IN VOID                   *SetupBuf,
  IN UINTN                  SetupSize,
  IN UINTN                  KernelSize
  )
{
  EFI_STATUS                Status;
  UINTN                     PeImageSize;
  VOID                      *PeImageData;
  EFI_HANDLE                ImageHandle;
  EFI_HANDLE                DevicePathHandle;
  MEMMAP_IMAGE_DEVICE_PATH  DevicePath;

  PeImageData = NULL;
  PeImageSize = 0;
  Status = EFI_SUCCESS;

  //
  // The first part of the PE/COFF image was loaded from the 'kernel setup'
  // area. QEMU will put the remaining portion in the 'kernel data' area.
  //
  // We combine these two items into a single buffer to retrieve the original
  // PE/COFF image.
  //
  PeImageSize = SetupSize + KernelSize;
  PeImageData = AllocatePages (EFI_SIZE_TO_PAGES (PeImageSize));
  if (PeImageData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeAndReturn;
  }
  CopyMem (PeImageData, SetupBuf, SetupSize);

  DEBUG ((EFI_D_INFO, "Remaining PE/COFF image size: 0x%x\n", (UINT32) KernelSize));
  DEBUG ((EFI_D_INFO, "Reading PE/COFF image ..."));
  QemuFwCfgSelectItem (QemuFwCfgItemKernelData);
  QemuFwCfgReadBytes (KernelSize, ((UINT8*) PeImageData) + SetupSize);
  DEBUG ((EFI_D_INFO, " [done]\n"));

  //
  // Create a device path for the image
  //
  CopyMem ((VOID*) &DevicePath,
           (VOID*) &mMemmapImageDevicePathTemplate,
           sizeof (DevicePath));
  DevicePath.MemMapDevPath.StartingAddress = (UINTN) PeImageData;
  DevicePath.MemMapDevPath.EndingAddress =
    DevicePath.MemMapDevPath.StartingAddress + PeImageSize;
  DevicePathHandle = NULL;
  Status = gBS->InstallProtocolInterface (
                  &DevicePathHandle,
                  &gEfiDevicePathProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  (VOID*) &DevicePath
                  );

  //
  // Load and start the PE/COFF image
  //
  ImageHandle = NULL;
  Status = gBS->LoadImage (
                  FALSE,
                  gImageHandle,
                  (VOID*) &DevicePath,
                  PeImageData,
                  PeImageSize,
                  &ImageHandle
                  );
  DEBUG ((EFI_D_INFO, "Loading PE/COFF image -> %r\n", Status));

  if (!EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INFO, "Starting PE/COFF image ...\n"));
    Status = gBS->StartImage (ImageHandle, NULL, NULL);
    DEBUG ((EFI_D_INFO, "PE/COFF image exited -> %r\n", Status));
  }

  //
  // Uninstall the device path for the image
  //
  Status = gBS->UninstallProtocolInterface (
                  DevicePathHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID*) &DevicePath
                  );
  ASSERT_EFI_ERROR (Status);

FreeAndReturn:
  if (PeImageData != NULL) {
    FreePages (PeImageData, EFI_SIZE_TO_PAGES (PeImageSize));
  }

  return Status;
}


STATIC
EFI_STATUS
TryRunningKernelImage (
  IN VOID                   *SetupBuf,
  IN UINTN                  SetupSize,
  IN UINTN                  KernelSize
  )
{
  EFI_STATUS                Status;
  UINTN                     CommandLineSize;
  CHAR8                     *CommandLine;
  UINTN                     KernelInitialSize;
  VOID                      *KernelBuf;
  UINTN                     InitrdSize;
  VOID*                     InitrdData;

  KernelBuf = NULL;
  KernelInitialSize = 0;
  CommandLine = NULL;
  CommandLineSize = 0;
  InitrdData = NULL;
  InitrdSize = 0;

  Status = LoadLinuxCheckKernelSetup (SetupBuf, SetupSize);
  if (EFI_ERROR (Status)) {
    goto FreeAndReturn;
  }

  Status = LoadLinuxInitializeKernelSetup (SetupBuf);
  if (EFI_ERROR (Status)) {
    goto FreeAndReturn;
  }

  KernelInitialSize = LoadLinuxGetKernelSize (SetupBuf, KernelSize);
  if (KernelInitialSize == 0) {
    Status = EFI_UNSUPPORTED;
    goto FreeAndReturn;
  }

  KernelBuf = LoadLinuxAllocateKernelPages (
                SetupBuf,
                EFI_SIZE_TO_PAGES (KernelInitialSize));
  if (KernelBuf == NULL) {
    DEBUG ((EFI_D_ERROR, "Unable to allocate memory for kernel!\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeAndReturn;
  }

  DEBUG ((EFI_D_INFO, "Kernel size: 0x%x\n", (UINT32) KernelSize));
  DEBUG ((EFI_D_INFO, "Reading kernel image ..."));
  QemuFwCfgSelectItem (QemuFwCfgItemKernelData);
  QemuFwCfgReadBytes (KernelSize, KernelBuf);
  DEBUG ((EFI_D_INFO, " [done]\n"));

  QemuFwCfgSelectItem (QemuFwCfgItemCommandLineSize);
  CommandLineSize = (UINTN) QemuFwCfgRead64 ();

  if (CommandLineSize > 0) {
    CommandLine = LoadLinuxAllocateCommandLinePages (
                    EFI_SIZE_TO_PAGES (CommandLineSize));
    QemuFwCfgSelectItem (QemuFwCfgItemCommandLineData);
    QemuFwCfgReadBytes (CommandLineSize, CommandLine);
  } else {
    CommandLine = NULL;
  }

  Status = LoadLinuxSetCommandLine (SetupBuf, CommandLine);
  if (EFI_ERROR (Status)) {
    goto FreeAndReturn;
  }

  QemuFwCfgSelectItem (QemuFwCfgItemInitrdSize);
  InitrdSize = (UINTN) QemuFwCfgRead64 ();

  if (InitrdSize > 0) {
    InitrdData = LoadLinuxAllocateInitrdPages (
                   SetupBuf,
                   EFI_SIZE_TO_PAGES (InitrdSize)
                   );
    DEBUG ((EFI_D_INFO, "Initrd size: 0x%x\n", (UINT32) InitrdSize));
    DEBUG ((EFI_D_INFO, "Reading initrd image ..."));
    QemuFwCfgSelectItem (QemuFwCfgItemInitrdData);
    QemuFwCfgReadBytes (InitrdSize, InitrdData);
    DEBUG ((EFI_D_INFO, " [done]\n"));
  } else {
    InitrdData = NULL;
  }

  Status = LoadLinuxSetInitrd (SetupBuf, InitrdData, InitrdSize);
  if (EFI_ERROR (Status)) {
    goto FreeAndReturn;
  }

  //
  // Signal the EVT_SIGNAL_READY_TO_BOOT event
  //
  EfiSignalEventReadyToBoot();

  Status = LoadLinux (KernelBuf, SetupBuf);

FreeAndReturn:
  if (KernelBuf != NULL) {
    FreePages (KernelBuf, EFI_SIZE_TO_PAGES (KernelInitialSize));
  }
  if (CommandLine != NULL) {
    FreePages (CommandLine, EFI_SIZE_TO_PAGES (CommandLineSize));
  }
  if (InitrdData != NULL) {
    FreePages (InitrdData, EFI_SIZE_TO_PAGES (InitrdSize));
  }

  return Status;
}


EFI_STATUS
TryRunningQemuKernel (
  VOID
  )
{
  EFI_STATUS                Status;
  UINTN                     SetupSize;
  VOID                      *SetupBuf;
  UINTN                     KernelSize;

  SetupBuf = NULL;
  SetupSize = 0;

  if (!QemuFwCfgIsAvailable ()) {
    return EFI_NOT_FOUND;
  }

  QemuFwCfgSelectItem (QemuFwCfgItemKernelSize);
  KernelSize = (UINTN) QemuFwCfgRead64 ();

  QemuFwCfgSelectItem (QemuFwCfgItemKernelSetupSize);
  SetupSize = (UINTN) QemuFwCfgRead64 ();

  if (KernelSize == 0 || SetupSize == 0) {
    DEBUG ((EFI_D_INFO, "qemu -kernel was not used.\n"));
    return EFI_NOT_FOUND;
  }

  SetupBuf = LoadLinuxAllocateKernelSetupPages (EFI_SIZE_TO_PAGES (SetupSize));
  if (SetupBuf == NULL) {
    DEBUG ((EFI_D_ERROR, "Unable to allocate memory for kernel setup!\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((EFI_D_INFO, "Setup size: 0x%x\n", (UINT32) SetupSize));
  DEBUG ((EFI_D_INFO, "Reading kernel setup image ..."));
  QemuFwCfgSelectItem (QemuFwCfgItemKernelSetupData);
  QemuFwCfgReadBytes (SetupSize, SetupBuf);
  DEBUG ((EFI_D_INFO, " [done]\n"));

  if (LooksLikeAPeCoffImage (SetupBuf, SetupSize)) {
    Status = TryRunningPeCoffImage (SetupBuf, SetupSize, KernelSize);
  } else {
    Status = TryRunningKernelImage (SetupBuf, SetupSize, KernelSize);
  }

  if (SetupBuf != NULL) {
    FreePages (SetupBuf, EFI_SIZE_TO_PAGES (SetupSize));
  }

  return Status;
}

