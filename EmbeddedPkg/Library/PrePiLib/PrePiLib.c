/** @file

  Copyright (c) 2008-2009, Apple Inc. All rights reserved.
  
  All rights reserved. This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PrePi.h>

//
// Hack to work in NT32
//
EFI_STATUS

EFIAPI

SecWinNtPeiLoadFile (

  IN  VOID                    *Pe32Data,

  IN  EFI_PHYSICAL_ADDRESS    *ImageAddress,

  IN  UINT64                  *ImageSize,

  IN  EFI_PHYSICAL_ADDRESS    *EntryPoint

  );


EFI_STATUS
EFIAPI
LoadPeCoffImage (
  IN  VOID                                      *PeCoffImage,
  OUT EFI_PHYSICAL_ADDRESS                      *ImageAddress,
  OUT UINT64                                    *ImageSize,
  OUT EFI_PHYSICAL_ADDRESS                      *EntryPoint
  )
{
  RETURN_STATUS                 Status;
  PE_COFF_LOADER_IMAGE_CONTEXT  ImageContext;
  VOID                           *Buffer;

  ZeroMem (&ImageContext, sizeof (ImageContext));
    
  ImageContext.Handle    = PeCoffImage;
  ImageContext.ImageRead = PeCoffLoaderImageReadFromMemory;

  Status = PeCoffLoaderGetImageInfo (&ImageContext);
  ASSERT_EFI_ERROR (Status);
  

  //
  // Allocate Memory for the image
  //
  Buffer = AllocatePages (EFI_SIZE_TO_PAGES((UINT32)ImageContext.ImageSize));
  ASSERT (Buffer != 0);


  ImageContext.ImageAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)Buffer;

  //
  // Load the image to our new buffer
  //
  Status = PeCoffLoaderLoadImage (&ImageContext);
  ASSERT_EFI_ERROR (Status);

  //
  // Relocate the image in our new buffer
  //
  Status = PeCoffLoaderRelocateImage (&ImageContext);
  ASSERT_EFI_ERROR (Status);


  *ImageAddress = ImageContext.ImageAddress;
  *ImageSize    = ImageContext.ImageSize;
  *EntryPoint   = ImageContext.EntryPoint;

  //
  // Flush not needed for all architectures. We could have a processor specific
  // function in this library that does the no-op if needed.
  //
  InvalidateInstructionCacheRange ((VOID *)(UINTN)*ImageAddress, (UINTN)*ImageSize);

  return Status;
}



typedef 
VOID
(EFIAPI *DXE_CORE_ENTRY_POINT) (
  IN  VOID *HobStart
  );

EFI_STATUS
EFIAPI
LoadDxeCoreFromFfsFile (
  IN EFI_PEI_FILE_HANDLE  FileHandle,
  IN UINTN                StackSize
  )
{
  EFI_STATUS              Status;
  VOID                    *PeCoffImage;
  EFI_PHYSICAL_ADDRESS    ImageAddress;
  UINT64                  ImageSize;
  EFI_PHYSICAL_ADDRESS    EntryPoint;
  VOID                    *BaseOfStack;
  VOID                    *TopOfStack;
  VOID                    *Hob;
  EFI_FV_FILE_INFO        FvFileInfo;
  UINT64                  Tick;

  Tick = 0;
  PERF_START (NULL, "SEC", NULL, 1);

  Status = FfsFindSectionData (EFI_SECTION_PE32, FileHandle, &PeCoffImage);
  if (EFI_ERROR  (Status)) {
    return Status;
  }

  
  Status = LoadPeCoffImage (PeCoffImage, &ImageAddress, &ImageSize, &EntryPoint);
// For NT32 Debug  Status = SecWinNtPeiLoadFile (PeCoffImage, &ImageAddress, &ImageSize, &EntryPoint);
  ASSERT_EFI_ERROR (Status);

  //
  // Extract the DxeCore GUID file name.
  //
  Status = FfsGetFileInfo (FileHandle, &FvFileInfo);
  ASSERT_EFI_ERROR (Status);

  BuildModuleHob (&FvFileInfo.FileName, (EFI_PHYSICAL_ADDRESS)(UINTN)ImageAddress, EFI_SIZE_TO_PAGES ((UINT32) ImageSize) * EFI_PAGE_SIZE, EntryPoint);
  
  DEBUG ((EFI_D_INFO | EFI_D_LOAD, "Loading DxeCore at 0x%10p EntryPoint=0x%10p\n", (VOID *)(UINTN)ImageAddress, (VOID *)(UINTN)EntryPoint));

  Hob = GetHobList ();
  if (StackSize == 0) {
    // User the current stack
  
  
    if (PerformanceMeasurementEnabled ()) {
      Tick = GetPerformanceCounter ();
    }
    PERF_END (NULL, "SEC", NULL, Tick);

    ((DXE_CORE_ENTRY_POINT)(UINTN)EntryPoint) (Hob);
  } else {
    
    //
    // Allocate 128KB for the Stack
    //
    BaseOfStack = AllocatePages (EFI_SIZE_TO_PAGES (StackSize));
    ASSERT (BaseOfStack != NULL);
  
    //
    // Compute the top of the stack we were allocated. Pre-allocate a UINTN
    // for safety.
    //
    TopOfStack = (VOID *) ((UINTN) BaseOfStack + EFI_SIZE_TO_PAGES (StackSize) * EFI_PAGE_SIZE - CPU_STACK_ALIGNMENT);
    TopOfStack = ALIGN_POINTER (TopOfStack, CPU_STACK_ALIGNMENT);

    //
    // Update the contents of BSP stack HOB to reflect the real stack info passed to DxeCore.
    //    
    UpdateStackHob ((EFI_PHYSICAL_ADDRESS)(UINTN) BaseOfStack, StackSize);
    

    if (PerformanceMeasurementEnabled ()) {
      Tick = GetPerformanceCounter ();
    }
    PERF_END (NULL, "SEC", NULL, Tick);

    SwitchStack (
      (SWITCH_STACK_ENTRY_POINT)(UINTN)EntryPoint,
      Hob,
      NULL,
      TopOfStack
      );

  }
  
  // Should never get here as DXE Core does not return
  DEBUG ((EFI_D_ERROR, "DxeCore returned\n"));
  ASSERT (FALSE);
  
  return EFI_DEVICE_ERROR;
}



EFI_STATUS
EFIAPI
LoadDxeCoreFromFv (
  IN UINTN  *FvInstance,   OPTIONAL
  IN UINTN  StackSize
  )
{
  EFI_STATUS          Status;
  EFI_PEI_FV_HANDLE   VolumeHandle;
  EFI_PEI_FILE_HANDLE FileHandle = NULL;

  if (FvInstance != NULL) {
    //
    // Caller passed in a specific FV to try, so only try that one
    //
    Status = FfsFindNextVolume (*FvInstance, &VolumeHandle);
    if (!EFI_ERROR (Status)) {
      Status = FfsFindNextFile (EFI_FV_FILETYPE_DXE_CORE, VolumeHandle, &FileHandle);
    }
  } else {
    Status = FfsAnyFvFindFirstFile (EFI_FV_FILETYPE_DXE_CORE, &VolumeHandle, &FileHandle);
  }

  if (!EFI_ERROR (Status)) {
    return LoadDxeCoreFromFfsFile (FileHandle, StackSize);
  } 
  
  return Status;  
}


EFI_STATUS
EFIAPI
DecompressFirstFv (
  VOID
  )
{
  EFI_STATUS          Status;
  EFI_PEI_FV_HANDLE   VolumeHandle;
  EFI_PEI_FILE_HANDLE FileHandle;

  Status = FfsAnyFvFindFirstFile (EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE, &VolumeHandle, &FileHandle);
  if (!EFI_ERROR (Status)) {
    Status = FfsProcessFvFile (FileHandle);
  }
  
  return Status;
}


