/** @file
  USB Serial Driver that manages USB to Serial and produces Serial IO Protocol.

Portions Copyright 2012 Ashley DeSimone
Copyright (c) 2004 - 2012, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

//
// Tested with VEND_ID 0x0403, DEVICE_ID 0x6001
//
// Driver is hard coded to 115200, N, 8, 1, No Flow control
//

#include "FtdiUsbSerial.h"

#define VERBOSE_TRANSFER_DEBUG
#define ENABLE_SERIAL_READS

#define USB_IS_ERROR(Result, Error)           (((Result) & (Error)) != 0)

#define WDR_TIMEOUT 5000 /* default urb timeout */
#define WDR_SHORT_TIMEOUT 1000 /* shorter urb timeout */

#define MAX_BUFFER_SIZE 1024

//
// USB USB Serial Driver Global Variables
//
EFI_DRIVER_BINDING_PROTOCOL gUsbSerialDriverBinding = {
  UsbSerialDriverBindingSupported,
  UsbSerialDriverBindingStart,
  UsbSerialDriverBindingStop,
  0xa,
  NULL,
  NULL
};

/**
  Entrypoint of USB Serial Driver.

  This function is the entrypoint of USB Serial Driver. It installs Driver Binding
  Protocols together with Component Name Protocols.

  @param  ImageHandle       The firmware allocated handle for the EFI image.
  @param  SystemTable       A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.

**/
EFI_STATUS
EFIAPI
FtdiUsbSerialEntryPoint (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS              Status;

  Status = EfiLibInstallDriverBindingComponentName2 (
    ImageHandle,
    SystemTable,
    &gUsbSerialDriverBinding,
    ImageHandle,
    &gUsbSerialComponentName,
    &gUsbSerialComponentName2
    );
  ASSERT_EFI_ERROR (Status);
  return EFI_SUCCESS;
}

/**
  Check whether USB Serial driver supports this device.

  @param  This                   The USB Serial driver binding protocol.
  @param  Controller             The controller handle to check.
  @param  RemainingDevicePath    The remaining device path.

  @retval EFI_SUCCESS            The driver supports this controller.
  @retval other                  This device isn't supported.

**/
EFI_STATUS
EFIAPI
UsbSerialDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS          Status;
  EFI_USB_IO_PROTOCOL *UsbIo;

  //
  // Check if USB I/O Protocol is attached on the controller handle.
  //
  Status = gBS->OpenProtocol (
    Controller,
    &gEfiUsbIoProtocolGuid,
    (VOID **) &UsbIo,
    This->DriverBindingHandle,
    Controller,
    EFI_OPEN_PROTOCOL_BY_DRIVER
    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Use the USB I/O Protocol interface to check whether Controller is
  // a USB Serial device that can be managed by this driver.
  //
  Status = EFI_SUCCESS;

  if (!IsUsbSerial (UsbIo)) {
    Status = EFI_UNSUPPORTED;
  }

  gBS->CloseProtocol (
    Controller,
    &gEfiUsbIoProtocolGuid,
    This->DriverBindingHandle,
    Controller
    );

  return Status;
}

/**
  Starts the USB Serial device with this driver.

  This function produces initializes the USB Serial device and
  produces the Serial IO Protocol.

  @param  This                   The USB Serial driver binding instance.
  @param  Controller             Handle of device to bind driver to.
  @param  RemainingDevicePath    Optional parameter use to pick a specific child
                                 device to start.

  @retval EFI_SUCCESS            The controller is controlled by the usb USB Serial driver.
  @retval EFI_UNSUPPORTED        No interrupt endpoint can be found.
  @retval Other                  This controller cannot be started.

**/
EFI_STATUS
EFIAPI
UsbSerialDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS                  Status;
  EFI_USB_IO_PROTOCOL         *UsbIo;
  USB_SER_DEV                 *UsbSerialDevice;
  UINT8                       EndpointNumber;
  EFI_USB_ENDPOINT_DESCRIPTOR EndpointDescriptor;
  UINT8                       Index;
  BOOLEAN                     FoundIn;
  BOOLEAN                     FoundOut;
  EFI_TPL                     OldTpl;
  UINT32                      Data32;
  EFI_USB_DEVICE_REQUEST      DevReq;
  UINT32                      ReturnValue;
  UINT8                       ConfigurationValue;
  UINT64                      CheckInputTriggerTime;
  UINT16                      EncodedBaudRate;
                 
  OldTpl = gBS->RaiseTPL (TPL_CALLBACK);

  //
  // Open USB I/O Protocol
  //
  Status = gBS->OpenProtocol (
    Controller,
    &gEfiUsbIoProtocolGuid,
    (VOID **) &UsbIo,
    This->DriverBindingHandle,
    Controller,
    EFI_OPEN_PROTOCOL_BY_DRIVER
    );
  if (EFI_ERROR (Status)) {
    goto ErrorExit1;
  }

  UsbSerialDevice = AllocateZeroPool (sizeof (USB_SER_DEV));
  ASSERT (UsbSerialDevice != NULL);

  //
  // Get the Device Path Protocol on Controller's handle
  //
  Status = gBS->OpenProtocol (
    Controller,
    &gEfiDevicePathProtocolGuid,
    (VOID **) &UsbSerialDevice->DevicePath,
    This->DriverBindingHandle,
    Controller,
    EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  //
  // Report that the USB Serial is being enabled
  //
  REPORT_STATUS_CODE_WITH_DEVICE_PATH (
    EFI_PROGRESS_CODE,
    (EFI_PERIPHERAL_UNSPECIFIED | EFI_P_PC_ENABLE),
    UsbSerialDevice->DevicePath
    );

  //
  // This is pretty close to detection, so log progress
  //
  REPORT_STATUS_CODE_WITH_DEVICE_PATH (
    EFI_PROGRESS_CODE,
    (EFI_PERIPHERAL_UNSPECIFIED | EFI_P_PC_PRESENCE_DETECT),
    UsbSerialDevice->DevicePath
    );

  UsbSerialDevice->UsbIo = UsbIo;

  //
  // Get interface & endpoint descriptor
  //
  UsbIo->UsbGetInterfaceDescriptor (
    UsbIo,
    &UsbSerialDevice->InterfaceDescriptor
    );

  EndpointNumber = UsbSerialDevice->InterfaceDescriptor.NumEndpoints;

  //
  // Traverse endpoints to find the IN and OUT endpoints that will send and receive data.
  //
  FoundIn = FALSE;
  FoundOut = FALSE;
  for (Index = 0; Index < EndpointNumber; Index++) {

    Status = UsbIo->UsbGetEndpointDescriptor (
      UsbIo,
      Index,
      &EndpointDescriptor
      );
    if (EFI_ERROR (Status)) {
      return FALSE;
    }

    if (EndpointDescriptor.EndpointAddress == 0x02) {
      //
      // Set the Out endpoint device
      //
      CopyMem (&UsbSerialDevice->OutEndpointDescriptor, &EndpointDescriptor, sizeof(EndpointDescriptor));
      FoundOut = TRUE;
    }

    if (EndpointDescriptor.EndpointAddress == 0x81) {
      //
      // Set the In endpoint device
      //
      CopyMem (&UsbSerialDevice->InEndpointDescriptor, &EndpointDescriptor, sizeof(EndpointDescriptor));
      FoundIn = TRUE;
    }
  }

  if (!FoundIn || !FoundOut) {
    //
    // No interrupt endpoint found, then return unsupported.
    //
    Status = EFI_UNSUPPORTED;
    goto ErrorExit;
  }

  DevReq.Request      = FTDI_COMMAND_SET_DATA_BITS,
  DevReq.RequestType  = REQ_TYPE,
  DevReq.Value = SET_DATA_BITS_8;
  DevReq.Index = 1;
  DevReq.Length = 0;
  Status = UsbIo->UsbControlTransfer (
    UsbIo,
    &DevReq,
    EfiUsbDataOut,
    WDR_SHORT_TIMEOUT,
    &ConfigurationValue,
    1,
    &ReturnValue
    );
  ASSERT_EFI_ERROR (Status);
  Data32 = ReturnValue;

  DevReq.Request      = FTDI_COMMAND_SET_FLOW_CTRL,
  DevReq.RequestType  = REQ_TYPE,
  DevReq.Value = NO_FLOW_CTRL;
  DevReq.Index = 0x0001;
  DevReq.Length = 0;
  Status = UsbIo->UsbControlTransfer (
    UsbIo,
    &DevReq,
    EfiUsbDataOut,
    WDR_TIMEOUT,
    &ConfigurationValue,
    1,
    &ReturnValue
    );
  ASSERT_EFI_ERROR (Status);
  Data32 = ReturnValue;

  DevReq.Request      = FTDI_COMMAND_SET_BAUDRATE;
  DevReq.RequestType  = REQ_TYPE;
  Status = EncodeBaudRateForFtdi(115200, &EncodedBaudRate);
  ASSERT_EFI_ERROR(Status);
  DevReq.Value = EncodedBaudRate;
  DevReq.Index = 1;
  DevReq.Length = 0;
  Status = UsbIo->UsbControlTransfer (
    UsbIo,
    &DevReq,
    EfiUsbDataOut,
    WDR_SHORT_TIMEOUT,
    &ConfigurationValue,
    1,
    &ReturnValue
    );
  ASSERT_EFI_ERROR (Status);
  Data32 = ReturnValue;

  //
  // Publish Serial GUID and protocol
  //
  UsbSerialDevice->Signature = USB_SER_DEV_SIGNATURE;
  UsbSerialDevice->SerialIo.Reset  = SerialReset;
  UsbSerialDevice->SerialIo.SetControl  = SetControlBits;
  UsbSerialDevice->SerialIo.SetAttributes  = SetAttributes;
  UsbSerialDevice->SerialIo.GetControl  = GetControlBits;
  UsbSerialDevice->SerialIo.Read  = ReadSerialIo;
  UsbSerialDevice->SerialIo.Write  = WriteSerialIo;

  //
  // Set the static Serial IO modes that will display when running
  // "sermode" within the UEFI shell.
  //
  UsbSerialDevice->SerialIo.Mode->Timeout = 0;
  UsbSerialDevice->SerialIo.Mode->BaudRate = 115200;
  UsbSerialDevice->SerialIo.Mode->DataBits = 8;
  UsbSerialDevice->SerialIo.Mode->Parity = 1;
  UsbSerialDevice->SerialIo.Mode->StopBits = 1;

  //
  // Allocate space for the receive buffer
  //
  UsbSerialDevice->DataBuffer = AllocateZeroPool(MAX_BUFFER_SIZE);

  //
  // Initialize data buffer pointers.
  // Head==Tail = true means buffer is empty.
  //
  UsbSerialDevice->DataBufferHead = 0;
  UsbSerialDevice->DataBufferTail = 0;
  Status = gBS->InstallMultipleProtocolInterfaces (
    &Controller,
    &gEfiSerialIoProtocolGuid,
    &UsbSerialDevice->SerialIo,
    NULL
    );
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  UsbSerialDevice->ControllerNameTable = NULL;
  AddUnicodeString2 (
    "eng",
    gUsbSerialComponentName.SupportedLanguages,
    &UsbSerialDevice->ControllerNameTable,
    L"Generic Usb Serial1",
    TRUE
    );
  AddUnicodeString2 (
    "en",
    gUsbSerialComponentName2.SupportedLanguages,
    &UsbSerialDevice->ControllerNameTable,
    L"Generic Usb Serial2",
    FALSE
    );
    
  //
  // Create a polling loop to check for input
  //
  gBS->CreateEvent (
    EVT_TIMER | EVT_NOTIFY_SIGNAL,
    TPL_NOTIFY,
    UsbSerialDriverCheckInput,
    UsbSerialDevice,
    &(UsbSerialDevice->PollingLoop)
    );
  // add code to set trigger time based on baud rate
  // setting to 0.5ms for now
  CheckInputTriggerTime = 5000000; // 100 nano seconds is 0.1 microseconds
  gBS->SetTimer (
    UsbSerialDevice->PollingLoop,
    TimerPeriodic,
    CheckInputTriggerTime
    );
  gBS->RestoreTPL (OldTpl);

  UsbSerialDevice->Shutdown = FALSE;

  return EFI_SUCCESS;

ErrorExit:
  //
  // Error handler
  //
  Status = gBS->UninstallMultipleProtocolInterfaces (
    Controller,
    &gEfiSerialIoProtocolGuid,
    &UsbSerialDevice->SerialIo,
    NULL
    );
  if (EFI_ERROR (Status)) {
    goto ErrorExit1;
  }

  FreePool (UsbSerialDevice->DataBuffer);
  FreePool (UsbSerialDevice);

  UsbSerialDevice = NULL;
  gBS->CloseProtocol (
    Controller,
    &gEfiUsbIoProtocolGuid,
    This->DriverBindingHandle,
    Controller
    );

ErrorExit1:
  return Status;

}

/**
  Stop the USB Serial device handled by this driver.

  @param  This                   The USB Serial driver binding protocol.
  @param  Controller             The controller to release.
  @param  NumberOfChildren       The number of handles in ChildHandleBuffer.
  @param  ChildHandleBuffer      The array of child handle.

  @retval EFI_SUCCESS            The device was stopped.
  @retval EFI_UNSUPPORTED        Serial IO Protocol is not installed on Controller.
  @retval EFI_DEVICE_ERROR       The device could not be stopped due to a device error.
  @retval Others                 Fail to uninstall protocols attached on the device.

**/
EFI_STATUS
EFIAPI
UsbSerialDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN  EFI_HANDLE                     Controller,
  IN  UINTN                          NumberOfChildren,
  IN  EFI_HANDLE                     *ChildHandleBuffer
  )
{
  EFI_STATUS                      Status;
  EFI_SERIAL_IO_PROTOCOL          *SerialIo;
  USB_SER_DEV                     *UsbSerialDevice;

  Status = gBS->OpenProtocol (
    Controller,
    &gEfiSerialIoProtocolGuid,
    (VOID **) &SerialIo,
    This->DriverBindingHandle,
    Controller,
    EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  UsbSerialDevice = USB_SER_DEV_FROM_THIS (SerialIo);

  REPORT_STATUS_CODE_WITH_DEVICE_PATH (
    EFI_PROGRESS_CODE,
    (EFI_PERIPHERAL_UNSPECIFIED | EFI_P_PC_DISABLE),
    UsbSerialDevice->DevicePath
    );

  gBS->CloseProtocol (
    Controller,
    &gEfiUsbIoProtocolGuid,
    This->DriverBindingHandle,
    Controller
    );

  Status = gBS->UninstallMultipleProtocolInterfaces (
    Controller,
    &gEfiSerialIoProtocolGuid,
    &UsbSerialDevice->SerialIo,
    NULL
    );

  gBS->SetTimer (
    UsbSerialDevice->PollingLoop,
    TimerCancel,
    0
    );
  gBS->CloseEvent(
    UsbSerialDevice->PollingLoop
    );

  UsbSerialDevice->Shutdown = TRUE;

  //
  // Free all resources.
  //
  if (UsbSerialDevice->ControllerNameTable != NULL) {
    FreeUnicodeStringTable (UsbSerialDevice->ControllerNameTable);
  }

  FreePool (UsbSerialDevice->DataBuffer);
  FreePool (UsbSerialDevice);

  return Status;
}

/**
  Transfer the data between the device and host.

  This function transfers the data between the device and host.
  BOT transfer is composed of three phases: Command, Data, and Status.
  This is the Data phase.

  @param  UsbBot                The USB BOT device
  @param  DataDir               The direction of the data
  @param  Data                  The buffer to hold data
  @param  TransLen              The expected length of the data
  @param  Timeout               The time to wait the command to complete

  @retval EFI_SUCCESS           The data is transferred
  @retval EFI_SUCCESS           No data to transfer
  @retval EFI_NOT_READY         The device return NAK to the transfer
  @retval Others                Failed to transfer data

**/
EFI_STATUS
UsbSerialDataTransfer (
  IN USB_SER_DEV              *UsbBot,
  IN EFI_USB_DATA_DIRECTION   DataDir,
  IN OUT VOID                 *Data,
  IN OUT UINTN                *TransLen,
  IN UINT32                   Timeout
  )
{
  EFI_USB_ENDPOINT_DESCRIPTOR *Endpoint;
  EFI_STATUS                  Status;
  UINT32                      Result;
  UINT32                      Data32;

  //
  // If no data to transfer, just return EFI_SUCCESS.
  //
  if ((DataDir == EfiUsbNoData) || (*TransLen == 0)) {
    return EFI_SUCCESS;
  }

  //
  // Select the endpoint then issue the transfer
  //
  if (DataDir == EfiUsbDataIn) {
    Endpoint = &UsbBot->InEndpointDescriptor;
    DEBUG ((EFI_D_INFO,"Input Transfer\n"));
  } else {
    Endpoint = &UsbBot->OutEndpointDescriptor;
    //DEBUG ((EFI_D_INFO,"Output Transfer\n"));
  }

  Result  = 0;
  Data32 = Endpoint->EndpointAddress;
  if (DataDir == EfiUsbDataIn) {
    DEBUG ((EFI_D_INFO,"check1\n"));
    DEBUG ((EFI_D_INFO,"Timeout = %d\n", Timeout));
  }
  Status = UsbBot->UsbIo->UsbBulkTransfer (
    UsbBot->UsbIo,
    Endpoint->EndpointAddress,
    Data,
    TransLen,
    Timeout,
    &Result
    );

  if (DataDir == EfiUsbDataIn) {
    DEBUG ((EFI_D_INFO,"check2\n"));
  }

  if (EFI_ERROR (Status)) {
    if (USB_IS_ERROR (Result, EFI_USB_ERR_STALL)) {
      DEBUG ((EFI_D_INFO, "UsbSerialDataTransferInfo: (%r)\n", Status));
      DEBUG ((EFI_D_INFO, "UsbSerialDataTransferInfo: DataIn Stall\n"));
    } else if (USB_IS_ERROR (Result, EFI_USB_ERR_NAK)) {
      Status = EFI_NOT_READY;
    } else if (Status == EFI_TIMEOUT) {
      DEBUG ((EFI_D_INFO, "Transfer Timed Out\n"));
    } else {
      UsbBot->Shutdown = TRUE; //Fixes infinite loop in older EFI
      DEBUG ((EFI_D_ERROR, "UsbSerialDataTransferErr: (%r)\n", Status));
    }
    if(Status == EFI_TIMEOUT){
      DEBUG ((EFI_D_ERROR, "UsbBotDataTransferErr: (%r)\n", Status));
    }
    return Status;
  }

  return Status;
}

/**
  Writes data to a serial device.

  @param  This              Protocol instance pointer.
  @param  BufferSize        On input, the size of the Buffer. On output, the amount of
                            data actually written.
  @param  Buffer            The buffer of data to write

  @retval EFI_SUCCESS       The data was written.
  @retval EFI_DEVICE_ERROR  The device reported an error.
  @retval EFI_TIMEOUT       The data write was stopped due to a timeout.

**/
EFI_STATUS
EFIAPI
WriteSerialIo (
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  IN OUT UINTN                      *BufferSize,
  IN VOID                           *Buffer
  )
{
  EFI_STATUS           Status;
  USB_SER_DEV          *UsbSerialDevice;
  EFI_TPL              Tpl;

  UsbSerialDevice = USB_SER_DEV_FROM_THIS (This);

  if (UsbSerialDevice->Shutdown) {
    DEBUG ((EFI_D_INFO, "Shutdown is set\n"));
    return EFI_DEVICE_ERROR;
  }

  Tpl     = gBS->RaiseTPL (TPL_NOTIFY);

  Status = UsbSerialDataTransfer (
    UsbSerialDevice,
    EfiUsbDataOut,
    Buffer,
    BufferSize,
    40000
    );
  gBS->RestoreTPL (Tpl);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_INFO, "Write returning device error\n"));
    return EFI_DEVICE_ERROR;
  }
  //ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  Reads data from a serial device.

  @param  This              Protocol instance pointer.
  @param  BufferSize        On input, the size of the Buffer. On output, the amount of
                            data returned in Buffer.
  @param  Buffer            The buffer to return the data into.

  @retval EFI_SUCCESS       The data was read.
  @retval EFI_DEVICE_ERROR  The device reported an error.
  @retval EFI_TIMEOUT       The data write was stopped due to a timeout.

**/
EFI_STATUS
EFIAPI
ReadSerialIo (
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  IN OUT UINTN                      *BufferSize,
  OUT VOID                          *Buffer
  )
{
  UINTN        Index;
  UINTN        RemainingCallerBufferSize;
  USB_SER_DEV  *UsbSerialDevice;
  EFI_STATUS   Status;

  if (*BufferSize == 0) {
    return EFI_SUCCESS;
  }

  if (Buffer == NULL) {
    return EFI_DEVICE_ERROR;
  }

  Status          = EFI_SUCCESS;
  UsbSerialDevice = USB_SER_DEV_FROM_THIS (This);

  //return ReadDataFromUsb (UsbSerialDevice, BufferSize, Buffer);
  ///
  /// Clear out any data that we already have in our internal buffer
  ///
  for (Index=0; Index < *BufferSize; Index++) {
    if ( UsbSerialDevice->DataBufferHead == UsbSerialDevice->DataBufferTail) {
      break;
    }

    //
    // Still have characters in the buffer to return
    //
    ((UINT8 *)Buffer)[Index] = UsbSerialDevice->DataBuffer[UsbSerialDevice->DataBufferHead];
    UsbSerialDevice->DataBufferHead = (UsbSerialDevice->DataBufferHead + 1) % MAX_BUFFER_SIZE;
  }

  ///
  /// If we haven't filled the caller's buffer using data that we already had on hand
  /// We need to generate an additional USB request to try and fill the caller's buffer
  ///
  if (Index != *BufferSize) {
    RemainingCallerBufferSize = *BufferSize - Index;
    Status = ReadDataFromUsb (UsbSerialDevice, &RemainingCallerBufferSize, (VOID*)(((CHAR8*)Buffer) + Index));
    *BufferSize = RemainingCallerBufferSize + Index;
  }

  if (UsbSerialDevice->DataBufferHead == UsbSerialDevice->DataBufferTail) {
    ///
    /// Data buffer has no data, set the EFI_SERIAL_INPUT_BUFFER_EMPTY flag
    ///
    UsbSerialDevice->ControlBits |= EFI_SERIAL_INPUT_BUFFER_EMPTY;
  } else {
    ///
    /// There is some leftover data, clear the EFI_SERIAL_INPUT_BUFFER_EMPTY flag
    ///
    UsbSerialDevice->ControlBits &= ~(EFI_SERIAL_INPUT_BUFFER_EMPTY);
  }
  return Status;
}

EFI_STATUS
EFIAPI
ReadDataFromUsb (
  IN USB_SER_DEV                    *UsbSerialDevice,
  IN OUT UINTN                      *BufferSize,
  OUT VOID                          *Buffer
  )
{
#ifdef ENABLE_SERIAL_READS
  EFI_STATUS      Status;
  UINTN           ReadBufferSize;
  UINT8           *ReadBuffer = NULL;
  UINTN           Index;
  EFI_TPL         Tpl;

  //DEBUG ((EFI_D_INFO, "BufferSize = %d\n", *BufferSize));

  Index = 0;
  ReadBuffer = AllocateZeroPool (512);
  ReadBufferSize = 512;

  if (UsbSerialDevice->Shutdown) {
    return EFI_DEVICE_ERROR;
  }

  Tpl     = gBS->RaiseTPL (TPL_NOTIFY);

  Status = UsbSerialDataTransfer (
    UsbSerialDevice,
    EfiUsbDataIn,
    ReadBuffer,
    &ReadBufferSize,
    40
    );
  if (EFI_ERROR(Status)) {
    gBS->RestoreTPL (Tpl);
    if (Status == EFI_TIMEOUT) {
      DEBUG ((EFI_D_INFO, "Return Timeout\n"));
      return EFI_TIMEOUT;
    } else {
      DEBUG ((EFI_D_INFO, "Return Device Error\n"));
      return EFI_DEVICE_ERROR;
    }
  }
  Index = 0;

#ifdef VERBOSE_TRANSFER_DEBUG
  DEBUG ((EFI_D_INFO,"ReadBuffer (as hex bytes): {"));
  for (Index=0; Index < ReadBufferSize; Index++) {
    DEBUG ((EFI_D_INFO,"%02x", ReadBuffer[Index]));
    if (Index != ReadBufferSize-1) {
      DEBUG ((EFI_D_INFO,", "));
    }
  }
  DEBUG ((EFI_D_INFO,"}\n"));

  DEBUG ((EFI_D_INFO,"ReadBuffer (as characters, including status bytes):"));
  for (Index=0; Index < ReadBufferSize; Index++) {
    //
    // Uncomment this to exclude status bytes
    //
    //if (ReadBuffer[Index] > 0x7F || ReadBuffer[Index] < 0x02) {
    //  Index+=2;
    //}
    DEBUG ((EFI_D_INFO,"%c", ReadBuffer[Index]));
    if (Index != ReadBufferSize-1) {
      DEBUG ((EFI_D_INFO,", "));
    }
  }
  DEBUG ((EFI_D_INFO,"\n"));
#endif

  //
  // Store the read data in the read buffer
  //
  for (Index = 0; Index < ReadBufferSize; Index++) {
    if (((UsbSerialDevice->DataBufferTail + 1) % MAX_BUFFER_SIZE) == UsbSerialDevice->DataBufferHead) {
      break;
    }

    //
    // Ignore status bytes.
    //
    if (ReadBuffer[Index] > 0x7F || ReadBuffer[Index] == 0x01)      ///< This looks wrong
      Index+=2;
    if (ReadBuffer[Index] == 0x00) {
      //
      // This is null, do not add
      //
    } else {
      UsbSerialDevice->DataBuffer[UsbSerialDevice->DataBufferTail] = ReadBuffer[Index];
      UsbSerialDevice->DataBufferTail = (UsbSerialDevice->DataBufferTail + 1) % MAX_BUFFER_SIZE;
    }
  }

  //
  // Read characters out of the buffer to satisfy caller's request.
  //
  for (Index=0; Index < *BufferSize; Index++) {
    if ( UsbSerialDevice->DataBufferHead == UsbSerialDevice->DataBufferTail) {
      //DEBUG ((EFI_D_INFO,"No characters to return!"));
      break;
    }

    //
    // Still have characters in the buffer to return
    //
    ((UINT8 *)Buffer)[Index] = UsbSerialDevice->DataBuffer[UsbSerialDevice->DataBufferHead];
    UsbSerialDevice->DataBufferHead = (UsbSerialDevice->DataBufferHead + 1) % MAX_BUFFER_SIZE;
  }

  //
  // Return actual number of bytes returned.
  //
  *BufferSize = Index;

  gBS->RestoreTPL (Tpl);

  return EFI_SUCCESS;
#else
  DEBUG ((EFI_D_INFO, "Returning Timeout because not implemented\n"));
  return EFI_TIMEOUT;
#endif
}

/**
  Retrieves the status of the control bits on a serial device

  @param  This              Protocol instance pointer.
  @param  Control           A pointer to return the current Control signals from the serial device.

  @retval EFI_SUCCESS       The control bits were read from the serial device.
  @retval EFI_DEVICE_ERROR  The serial device is not functioning correctly.

**/
EFI_STATUS
EFIAPI
GetControlBits (
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  OUT UINT32                        *Control
  )
{
  USB_SER_DEV  *UsbSerialDevice;

  UsbSerialDevice = USB_SER_DEV_FROM_THIS (This);
  *Control = UsbSerialDevice->ControlBits;
  return EFI_SUCCESS;
}

/**
  Set the control bits on a serial device

  @param  This             Protocol instance pointer.
  @param  Control          Set the bits of Control that are settable.

  @retval EFI_SUCCESS      The new control bits were set on the serial device.
  @retval EFI_UNSUPPORTED  The serial device does not support this operation.
  @retval EFI_DEVICE_ERROR The serial device is not functioning correctly.

**/
EFI_STATUS
EFIAPI
SetControlBits (
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  IN UINT32                         Control
  )
{
  USB_SER_DEV  *UsbSerialDevice;

  UsbSerialDevice = USB_SER_DEV_FROM_THIS (This);
  UsbSerialDevice->ControlBits = Control;
  return EFI_SUCCESS;
}

/**
  Sets the baud rate, receive FIFO depth, transmit/receice time out, parity,
  data buts, and stop bits on a serial device.

  @param  This             Protocol instance pointer.
  @param  BaudRate         The requested baud rate. A BaudRate value of 0 will use the
                           device's default interface speed.
  @param  ReveiveFifoDepth The requested depth of the FIFO on the receive side of the
                           serial interface. A ReceiveFifoDepth value of 0 will use
                           the device's default FIFO depth.
  @param  Timeout          The requested time out for a single character in microseconds.
                           This timeout applies to both the transmit and receive side of the
                           interface. A Timeout value of 0 will use the device's default time
                           out value.
  @param  Parity           The type of parity to use on this serial device. A Parity value of
                           DefaultParity will use the device's default parity value.
  @param  DataBits         The number of data bits to use on the serial device. A DataBits
                           vaule of 0 will use the device's default data bit setting.
  @param  StopBits         The number of stop bits to use on this serial device. A StopBits
                           value of DefaultStopBits will use the device's default number of
                           stop bits.

  @retval EFI_SUCCESS      The device was reset.
  @retval EFI_DEVICE_ERROR The serial device could not be reset.

**/
EFI_STATUS
EFIAPI
SetAttributes (
  IN EFI_SERIAL_IO_PROTOCOL         *This,
  IN UINT64                         BaudRate,
  IN UINT32                         ReceiveFifoDepth,
  IN UINT32                         Timeout,
  IN EFI_PARITY_TYPE                Parity,
  IN UINT8                          DataBits,
  IN EFI_STOP_BITS_TYPE             StopBits
  )
{
  return EFI_UNSUPPORTED;
}

//
// Serial IO Member Functions
//
/**
  Reset the serial device.

  @param  This              Protocol instance pointer.

  @retval EFI_SUCCESS       The device was reset.
  @retval EFI_DEVICE_ERROR  The serial device could not be reset.

**/
EFI_STATUS
EFIAPI
SerialReset (
  IN EFI_SERIAL_IO_PROTOCOL *This
  )
{
  EFI_STATUS                Status;
  USB_SER_DEV               *UsbSerialDevice;
  EFI_USB_DEVICE_REQUEST    DevReq;
  UINT8                     ConfigurationValue;
  UINT32                    ReturnValue;

  UsbSerialDevice = USB_SER_DEV_FROM_THIS (This);

  DevReq.Request = FTDI_COMMAND_RESET_PORT,
  DevReq.RequestType = REQ_TYPE,
  DevReq.Value = 0x0;
  DevReq.Index = 1;
  DevReq.Length = 0;
  Status = UsbSerialDevice->UsbIo->UsbControlTransfer (
    UsbSerialDevice->UsbIo,
    &DevReq,
    EfiUsbDataIn,
    WDR_TIMEOUT,
    &ConfigurationValue,
    1,
    &ReturnValue
    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DevReq.Request = FTDI_COMMAND_RESET_PORT,
  DevReq.RequestType = REQ_TYPE,
  DevReq.Value = 0x1;
  DevReq.Index = 1;
  DevReq.Length = 0;
  Status = UsbSerialDevice->UsbIo->UsbControlTransfer (
    UsbSerialDevice->UsbIo,
    &DevReq,
    EfiUsbDataIn,
    WDR_TIMEOUT,
    &ConfigurationValue,
    1,
    &ReturnValue
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  DevReq.Request = FTDI_COMMAND_RESET_PORT,
  DevReq.RequestType = REQ_TYPE,
  DevReq.Value = 0x2;
  DevReq.Index = 1;
  DevReq.Length = 0;
  Status = UsbSerialDevice->UsbIo->UsbControlTransfer (
    UsbSerialDevice->UsbIo,
    &DevReq,
    EfiUsbDataIn,
    WDR_TIMEOUT,
    &ConfigurationValue,
    1,
    &ReturnValue
    );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  return Status;
}

VOID
EFIAPI
UsbSerialDriverCheckInput (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  UINTN          BufferSize;
  USB_SER_DEV    *UsbSerialDevice;

  UsbSerialDevice = (USB_SER_DEV*)Context;
  
  if (UsbSerialDevice->DataBufferHead == UsbSerialDevice->DataBufferTail) {
    ///
    /// Data buffer is empty, try to read from device
    ///
    BufferSize = 0;
    ReadDataFromUsb (UsbSerialDevice, &BufferSize, NULL);
    if (UsbSerialDevice->DataBufferHead == UsbSerialDevice->DataBufferTail) {
      ///
      /// Data buffer still has no data, set the EFI_SERIAL_INPUT_BUFFER_EMPTY flag
      ///
      UsbSerialDevice->ControlBits |= EFI_SERIAL_INPUT_BUFFER_EMPTY;
    } else {
      ///
      /// Read has returned some data, clear the EFI_SERIAL_INPUT_BUFFER_EMPTY flag
      ///
      UsbSerialDevice->ControlBits &= ~(EFI_SERIAL_INPUT_BUFFER_EMPTY);
    }
  } else {
    ///
    /// Data buffer has data, no read attempt required
    ///
    UsbSerialDevice->ControlBits &= ~(EFI_SERIAL_INPUT_BUFFER_EMPTY);
  }
}

EFI_STATUS
EFIAPI
EncodeBaudRateForFtdi (
  IN  UINT64  BaudRate,
  OUT UINT16  *EncodedBaudRate
  )
{
  UINT32 Divisor;
  UINT32 AdjustedFrequency;
  UINT16 Result;

  ///
  /// Table with the nearest power of 2 for the numbers 0-15
  ///
  UINT8 RoundedPowersOf2[16] = { 0, 2, 2, 4, 4, 4, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16 };

  ///
  /// Check to make sure we won't get an integer overflow
  ///
  if ( (BaudRate < 178) || ( BaudRate > ((FTDI_UART_FREQUENCY * 100) / 97) )) {
    return EFI_INVALID_PARAMETER;
  }

  ///
  /// Baud Rates of 2000000 and 3000000 are special cases
  ///
  if ( (BaudRate >= ((3000000 * 100) / 103)) && (BaudRate <= ((3000000 * 100) / 97))) {
    *EncodedBaudRate = 0;
    return EFI_SUCCESS;
  }
  if ( (BaudRate >= ((2000000 * 100) / 103)) && (BaudRate <= ((2000000 * 100) / 97))) {
    *EncodedBaudRate = 1;
    return EFI_SUCCESS;
  }

  ///
  /// Compute divisor
  ///
  Divisor = (FTDI_UART_FREQUENCY << 4) / (UINT32)BaudRate;

  ///
  /// Round the last 4 bits to the nearest power of 2
  ///
  Divisor = (Divisor & ~(0xF)) + (RoundedPowersOf2 [Divisor & 0xF]);

  ///
  /// Check to make sure computed divisor is within 
  /// the min and max that FTDI controller will accept
  ///
  if (Divisor < FTDI_MIN_DIVISOR) {
    Divisor = FTDI_MIN_DIVISOR;
  } else if (Divisor > FTDI_MAX_DIVISOR) {
    Divisor = FTDI_MAX_DIVISOR;
  }

  ///
  /// Check to make sure the frequency that the FTDI chip will need to
  /// generate to attain the requested Baud Rate is within 3% of the
  /// 3MHz clock frequency that the FTDI chip runs at.
  ///
  /// (3MHz * 1600) / 103 = 46601941
  /// (3MHz * 1600) / 97  = 49484536
  ///
  AdjustedFrequency = (((UINT32)BaudRate) * Divisor);
  if ((AdjustedFrequency < 46601941) || (AdjustedFrequency > 49484536)) {
    return EFI_INVALID_PARAMETER;
  }

  ///
  /// Encode the Divisor into the format FTDI expects
  ///
  Result = (UINT16)(Divisor >> 4);
  if        ((Divisor & 0x8) != 0) {
    Result |= 0x4000;
  } else if ((Divisor & 0x4) != 0) {
    Result |= 0x8000;
  } else if ((Divisor & 0x2) != 0) {
    Result |= 0xC000;
  }

  *EncodedBaudRate = Result;
  return EFI_SUCCESS;
}