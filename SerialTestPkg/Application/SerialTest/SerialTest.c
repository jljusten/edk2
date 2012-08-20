/** @file
  EFI_SERIAL_IO_PROTOCOL Test Application

  Copyright (c) 2012, Ashley DeSimone
  
  Portions Copyright (c) 2009 - 2011, Intel Corporation. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SerialIo.h>

#include <BasicLib.h>

#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/HandleParsingLib.h>
#include <Protocol/DevicePathToText.h>

#define CHKEFIERR(ret,msg) if(EFI_ERROR(ret)) { \
  SystemTable->ConOut->SetAttribute(SystemTable->ConOut, EFI_RED); \
  Print(msg); return ret; }

/**
  Gets the name of the device specified by the device handle.

  This function gets the user-readable name of the device specified by the device
  handle. If no user-readable name could be generated, then *BestDeviceName will be
  NULL and EFI_NOT_FOUND will be returned.

  If EFI_DEVICE_NAME_USE_COMPONENT_NAME is set, then the function will return the
  device's name using the EFI_COMPONENT_NAME2_PROTOCOL, if present on
  DeviceHandle.

  If EFI_DEVICE_NAME_USE_DEVICE_PATH is set, then the function will return the
  device's name using the EFI_DEVICE_PATH_PROTOCOL, if present on DeviceHandle.
  If both EFI_DEVICE_NAME_USE_COMPONENT_NAME and
  EFI_DEVICE_NAME_USE_DEVICE_PATH are set, then
  EFI_DEVICE_NAME_USE_COMPONENT_NAME will have higher priority.

  @param DeviceHandle           The handle of the device.
  @param Flags                  Determines the possible sources of component names.
                                Valid bits are:
                                  EFI_DEVICE_NAME_USE_COMPONENT_NAME
                                  EFI_DEVICE_NAME_USE_DEVICE_PATH
  @param Language               A pointer to the language specified for the device
                                name, in the same format as described in the UEFI
                                specification, Appendix M
  @param BestDeviceName         On return, points to the callee-allocated NULL-
                                terminated name of the device. If no device name
                                could be found, points to NULL. The name must be
                                freed by the caller...

  @retval EFI_SUCCESS           Get the name successfully.
  @retval EFI_NOT_FOUND         Fail to get the device name.
  @retval EFI_INVALID_PARAMETER Flags did not have a valid bit set.
  @retval EFI_INVALID_PARAMETER BestDeviceName was NULL
  @retval EFI_INVALID_PARAMETER DeviceHandle was NULL
**/
EFI_STATUS
EFIAPI
EfiShellGetDeviceName(
  IN EFI_HANDLE DeviceHandle,
  IN EFI_SHELL_DEVICE_NAME_FLAGS Flags,
  IN CHAR8 *Language,
  OUT CHAR16 **BestDeviceName
  )
{
  EFI_STATUS                        Status;
  EFI_COMPONENT_NAME2_PROTOCOL      *CompName2;
  EFI_DEVICE_PATH_TO_TEXT_PROTOCOL  *DevicePathToText;
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
  EFI_HANDLE                        *HandleList;
  UINTN                             HandleCount;
  UINTN                             LoopVar;
  CHAR16                            *DeviceNameToReturn;
  CHAR8                             *Lang;
  CHAR8                             *TempChar;

  UINTN                             ParentControllerCount;
  EFI_HANDLE                        *ParentControllerBuffer;
  UINTN                             ParentDriverCount;
  EFI_HANDLE                        *ParentDriverBuffer;

  if (BestDeviceName == NULL ||
      DeviceHandle   == NULL
     ){
    return (EFI_INVALID_PARAMETER);
  }

  //
  // make sure one of the 2 supported bits is on
  //
  if (((Flags & EFI_DEVICE_NAME_USE_COMPONENT_NAME) == 0) &&
      ((Flags & EFI_DEVICE_NAME_USE_DEVICE_PATH) == 0)) {
    return (EFI_INVALID_PARAMETER);
  }

  DeviceNameToReturn  = NULL;
  *BestDeviceName     = NULL;
  HandleList          = NULL;
  HandleCount         = 0;
  Lang                = NULL;

  if ((Flags & EFI_DEVICE_NAME_USE_COMPONENT_NAME) != 0) {
    Status = ParseHandleDatabaseByRelationship(
      NULL,
      DeviceHandle,
      HR_DRIVER_BINDING_HANDLE|HR_DEVICE_DRIVER,
      &HandleCount,
      &HandleList);
    for (LoopVar = 0; LoopVar < HandleCount ; LoopVar++){
      //
      // Go through those handles until we get one that passes for GetComponentName
      //
      Status = gBS->OpenProtocol(
        HandleList[LoopVar],
        &gEfiComponentName2ProtocolGuid,
        (VOID**)&CompName2,
        gImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);
      if (EFI_ERROR(Status)) {
        Status = gBS->OpenProtocol(
          HandleList[LoopVar],
          &gEfiComponentNameProtocolGuid,
          (VOID**)&CompName2,
          gImageHandle,
          NULL,
          EFI_OPEN_PROTOCOL_GET_PROTOCOL);
      }

      if (EFI_ERROR(Status)) {
        continue;
      }
      if (Language == NULL) {
        Lang = AllocateZeroPool(AsciiStrSize(CompName2->SupportedLanguages));
        if (Lang == NULL) {
          return (EFI_OUT_OF_RESOURCES);
        }
        AsciiStrCpy(Lang, CompName2->SupportedLanguages);
        TempChar = AsciiStrStr(Lang, ";");
        if (TempChar != NULL){
          *TempChar = CHAR_NULL;
        }
      } else {
        Lang = AllocateZeroPool(AsciiStrSize(Language));
        if (Lang == NULL) {
          return (EFI_OUT_OF_RESOURCES);
        }
        AsciiStrCpy(Lang, Language);
      }
      Status = CompName2->GetControllerName(CompName2, DeviceHandle, NULL, Lang, &DeviceNameToReturn);
      FreePool(Lang);
      Lang = NULL;
      if (!EFI_ERROR(Status) && DeviceNameToReturn != NULL) {
        break;
      }
    }
    if (HandleList != NULL) {
      FreePool(HandleList);
    }

    //
    // Now check the parent controller using this as the child.
    //
    if (DeviceNameToReturn == NULL){
      PARSE_HANDLE_DATABASE_PARENTS(DeviceHandle, &ParentControllerCount, &ParentControllerBuffer);
      for (LoopVar = 0 ; LoopVar < ParentControllerCount ; LoopVar++) {
        PARSE_HANDLE_DATABASE_UEFI_DRIVERS(ParentControllerBuffer[LoopVar], &ParentDriverCount, &ParentDriverBuffer);
        for (HandleCount = 0 ; HandleCount < ParentDriverCount ; HandleCount++) {
          //
          // try using that driver's component name with controller and our driver as the child.
          //
          Status = gBS->OpenProtocol(
            ParentDriverBuffer[HandleCount],
            &gEfiComponentName2ProtocolGuid,
            (VOID**)&CompName2,
            gImageHandle,
            NULL,
            EFI_OPEN_PROTOCOL_GET_PROTOCOL);
          if (EFI_ERROR(Status)) {
            Status = gBS->OpenProtocol(
              ParentDriverBuffer[HandleCount],
              &gEfiComponentNameProtocolGuid,
              (VOID**)&CompName2,
              gImageHandle,
              NULL,
              EFI_OPEN_PROTOCOL_GET_PROTOCOL);
          }

          if (EFI_ERROR(Status)) {
            continue;
          }
          if (Language == NULL) {
            Lang = AllocateZeroPool(AsciiStrSize(CompName2->SupportedLanguages));
            if (Lang == NULL) {
              return (EFI_OUT_OF_RESOURCES);
            }
            AsciiStrCpy(Lang, CompName2->SupportedLanguages);
            TempChar = AsciiStrStr(Lang, ";");
            if (TempChar != NULL){
              *TempChar = CHAR_NULL;
            }
          } else {
            Lang = AllocateZeroPool(AsciiStrSize(Language));
            if (Lang == NULL) {
              return (EFI_OUT_OF_RESOURCES);
            }
            AsciiStrCpy(Lang, Language);
          }
          Status = CompName2->GetControllerName(CompName2, ParentControllerBuffer[LoopVar], DeviceHandle, Lang, &DeviceNameToReturn);
          FreePool(Lang);
          Lang = NULL;
          if (!EFI_ERROR(Status) && DeviceNameToReturn != NULL) {
            break;
          }



        }
        SHELL_FREE_NON_NULL(ParentDriverBuffer);
        if (!EFI_ERROR(Status) && DeviceNameToReturn != NULL) {
          break;
        }
      }
      SHELL_FREE_NON_NULL(ParentControllerBuffer);
    }
    //
    // dont return on fail since we will try device path if that bit is on
    //
    if (DeviceNameToReturn != NULL){
      ASSERT(BestDeviceName != NULL);
      StrnCatGrow(BestDeviceName, NULL, DeviceNameToReturn, 0);
      return (EFI_SUCCESS);
    }
  }
  if ((Flags & EFI_DEVICE_NAME_USE_DEVICE_PATH) != 0) {
    Status = gBS->LocateProtocol(
      &gEfiDevicePathToTextProtocolGuid,
      NULL,
      (VOID**)&DevicePathToText);
    //
    // we now have the device path to text protocol
    //
    if (!EFI_ERROR(Status)) {
      Status = gBS->OpenProtocol(
        DeviceHandle,
        &gEfiDevicePathProtocolGuid,
        (VOID**)&DevicePath,
        gImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);
      if (!EFI_ERROR(Status)) {
        //
        // use device path to text on the device path
        //
        *BestDeviceName = DevicePathToText->ConvertDevicePathToText(DevicePath, TRUE, TRUE);
        return (EFI_SUCCESS);
      }
    }
  }
  //
  // none of the selected bits worked.
  //
  return (EFI_NOT_FOUND);
}

//***************
//* Main Function
//***************
EFI_STATUS EFIAPI UefiMain(
  IN EFI_HANDLE ImageHandle,IN EFI_SYSTEM_TABLE  *SystemTable)
{
  INTN                   BufferSize;
  EFI_HANDLE             *Buffer;
  EFI_STATUS             Status;
  INTN                   i;
  EFI_SERIAL_IO_PROTOCOL *Serial;
  UINTN StringLength;
  CHAR16                 *DeviceName;
  UINTN                  Index;
  EFI_INPUT_KEY          Keystroke;
  BOOLEAN                ExitLoop;
  UINT32                 Control;
  UINT8                  DataBuffer[1024];
  UINTN                  DataBufferSize;

  Print (L"Serial Test Application\n");
  Print (L"Copyright 2012 (c) Ashley DeSimone\n");
  Print (L"Portions Copyright (c) 2009 - 2011, Intel Corporation. All rights reserved.\n");
  Print (L"\n");

  //First figure out how big of an array we need
  Index      = 0;
  BufferSize = 0;
  Buffer     = NULL;

  Status = gBS->LocateHandle (
    ByProtocol,
    &gEfiSerialIoProtocolGuid,
    NULL,
    &BufferSize,
    Buffer);
  
  //Check to make sure there is at least 1 handle in the system that implements our protocol
  if(Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"There are no serial ports attached to the system.\n");
    return EFI_SUCCESS;
  }

  Print (L"Found %d serial ports\n", (BufferSize / sizeof(EFI_HANDLE)));

  Buffer = AllocateZeroPool (sizeof (EFI_HANDLE) * BufferSize);
  if (Buffer == NULL) {
    Print(L"Out of memory\n");
    return EFI_SUCCESS;
  }

  Status = gBS->LocateHandle (
    ByProtocol,
    &gEfiSerialIoProtocolGuid,
    NULL,
    &BufferSize,
    Buffer);
  CHKEFIERR (Status, L"Unexpected error getting handles\n");

  Print (L"Select Serial Port to Open:\n");
  for (i = 0; i < (BufferSize / (INTN)sizeof(EFI_HANDLE)); i++) {
    Status = EfiShellGetDeviceName (Buffer[i], EFI_DEVICE_NAME_USE_COMPONENT_NAME | EFI_DEVICE_NAME_USE_DEVICE_PATH, "en", &DeviceName);

    if (!EFI_ERROR(Status)) {
      Print (L"\t%d. %s\n", (i + 1), DeviceName);
      FreePool (DeviceName);
    } else {
      Print (L"\t%d. <Name Unknown>\n", (i + 1));
    }
  }
  
  Keystroke.UnicodeChar = 0;
  while (Keystroke.UnicodeChar < 48 || Keystroke.UnicodeChar > 57) {
    Status = EFI_INVALID_PARAMETER;
    while (EFI_ERROR(Status)) {
      Status = gBS->WaitForEvent(1, &(gST->ConIn->WaitForKey), &Index);
    }

    gST->ConIn->ReadKeyStroke(gST->ConIn, &Keystroke);
    
    if (Keystroke.UnicodeChar < 49 || Keystroke.UnicodeChar > 57) {
      Print(L"Invalid Port Selection\n");
    }

    Index = Keystroke.UnicodeChar - 49;
    if (Index >= (UINTN)(BufferSize / (INTN)sizeof(EFI_HANDLE))) {
      Print(L"Invalid Port Selection\n");
      Keystroke.UnicodeChar = 0;
    }
  }


  Status = gBS->OpenProtocol (
    Buffer[Index],
    &gEfiSerialIoProtocolGuid,
    (void**)&Serial,
    ImageHandle,
    NULL,
    EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR (Status)) {
    Print (L"Unexpected error accessing protocol\n");
    return EFI_SUCCESS;
  }
  
  gST->ConOut->ClearScreen (gST->ConOut);
  Print (L"SERIAL PORT OPEN\nType to send data, Press ESC to quit.\n");

  /*Print(L"Writting To Serial Port...\n");
  StringLength = 13;
  Status = Serial->Write(Serial, &StringLength, "Hello World\r\n");
  if (EFI_ERROR (Status))
  {
    Print(L"Error Writting to Serial Port\n");
  }
  else
  {
    Print(L"Serial Port write successful\n");
  }
  Print(L"\n");*/
  ExitLoop = FALSE;
  while (ExitLoop != TRUE) {
    ///
    /// Check if there is data waiting.  If so read it and print to screen
    ///
    Status = Serial->GetControl(Serial, &Control);
    if (EFI_ERROR (Status)) {
      Print (L"Error Getting Serial Device Control Bits\n");
      ExitLoop = TRUE;
      break;
    }
    if ((Control & EFI_SERIAL_INPUT_BUFFER_EMPTY) == 0) {
      DataBufferSize = 1023;
      Status = Serial->Read (Serial, &DataBufferSize, DataBuffer);
      if (Status == EFI_TIMEOUT || !EFI_ERROR (Status)) {
        DataBuffer[DataBufferSize] = '\0';
        Print(L"%a", DataBuffer);
      } else {
        Print (L"Error Reading from serial device\n");
        ExitLoop = TRUE;
        break;
      }
    }

    ///
    ///Check if there is a keystroke waiting to be sent
    ///
    while (!EFI_ERROR (Status)) {
      Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Keystroke);
      if (!EFI_ERROR (Status)) {
        if (Keystroke.ScanCode == 0x17) {
          ExitLoop = TRUE;
          break;
        }
        if (Keystroke.UnicodeChar == 0x0) {
          continue;
        }
        StringLength = 1;
        DataBuffer[0] = (UINT8)Keystroke.UnicodeChar;
        Serial->Write (Serial, &StringLength, DataBuffer);
      }
    }
  }

  gBS->CloseProtocol (Buffer[Index], &gEfiSerialIoProtocolGuid, ImageHandle, NULL);
  FreePool(Buffer);
  Buffer = NULL;

  return EFI_SUCCESS;
}