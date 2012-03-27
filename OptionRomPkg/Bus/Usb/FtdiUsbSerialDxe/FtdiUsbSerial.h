/** @file
  Function prototype for USB Serial Driver.

Copyright (c) 2004 - 2011, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _FTDI_USB_SERIAL_H_
#define _FTDI_USB_SERIAL_H_

#include "FtdiUsbSerialDriver.h"

#define VID_FTDI            0x0403
#define DID_FTDI_FT232      0x6001

/**
  Uses USB I/O to check whether the device is a USB Serial device.

  @param  UsbIo    Pointer to a USB I/O protocol instance.

  @retval TRUE     Device is a USB Serial device.
  @retval FALSE    Device is a not USB Serial device.

**/
BOOLEAN
IsUsbSerial (
  IN  EFI_USB_IO_PROTOCOL       *UsbIo
  );

#endif