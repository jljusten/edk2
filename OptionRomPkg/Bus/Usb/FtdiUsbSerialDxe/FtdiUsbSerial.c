/** @file
  Helper functions for USB Serial Driver.

Copyright (c) 2004 - 2011, Intel Corporation. All rights reserved.<BR>
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

#include "FtdiUsbSerial.h"

typedef struct {
  UINTN     VendorId;
  UINTN     DeviceId;
} USB_DEVICE;

//
// Table of supported devices. This is the device information that this
// driver was developed with. Add other FTDI devices as needed.
//
USB_DEVICE USBDeviceList[] = {
  {VID_FTDI, DID_FTDI_FT232},
  {0,0}
};

/**
  Uses USB I/O to check whether the device is a USB Serial device.

  @param  UsbIo    Pointer to a USB I/O protocol instance.

  @retval TRUE     Device is a USB Serial device.
  @retval FALSE    Device is a not USB Serial device.

**/
BOOLEAN
IsUsbSerial (
  IN  EFI_USB_IO_PROTOCOL  *UsbIo
  )
{
  EFI_STATUS                  Status;
  EFI_USB_DEVICE_DESCRIPTOR   DeviceDescriptor;
  CHAR16                      **StrMfg=NULL;
  BOOLEAN                     Found;
  UINT32                      Index;

  //
  // Get the default device descriptor
  //
  Status = UsbIo->UsbGetDeviceDescriptor (
    UsbIo,
    &DeviceDescriptor
    );

  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Found = FALSE;
  Index = 0;
  while (USBDeviceList[Index].VendorId != 0 && USBDeviceList[Index].DeviceId != 0 && !Found){
    if (DeviceDescriptor.IdProduct == USBDeviceList[Index].DeviceId &&
      DeviceDescriptor.IdVendor == USBDeviceList[Index].VendorId
      ){
        Status = UsbIo->UsbGetStringDescriptor (
                          UsbIo,
                          0x0409, // LANGID selector, should make this more robust to verify lang support for device
                          DeviceDescriptor.StrManufacturer,
                          StrMfg
                          );
        if (EFI_ERROR (Status)) {
          return FALSE;
        }
        return TRUE;
    }
    Index++;
  }
  return FALSE;
}

