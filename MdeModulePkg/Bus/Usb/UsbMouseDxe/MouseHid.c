/** @file

  Parse mouse hid descriptor.

Copyright (c) 2004 - 2008, Intel Corporation
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "MouseHid.h"


/**
  Get next item from report descriptor.

  @param  StartPos          Start Position.
  @param  EndPos            End Position.
  @param  HidItem           HidItem to return.

  @return Position.

**/
UINT8 *
GetNextItem (
  IN  UINT8    *StartPos,
  IN  UINT8    *EndPos,
  OUT HID_ITEM *HidItem
  )
{
  UINT8 Temp;

  if ((EndPos - StartPos) <= 0) {
    return NULL;
  }

  Temp = *StartPos;
  StartPos++;
  //
  // bit 2,3
  //
  HidItem->Type = (UINT8) ((Temp >> 2) & 0x03);
  //
  // bit 4-7
  //
  HidItem->Tag = (UINT8) ((Temp >> 4) & 0x0F);

  if (HidItem->Tag == HID_ITEM_TAG_LONG) {
    //
    // Long Items are not supported by HID rev1.0,
    // although we try to parse it.
    //
    HidItem->Format = HID_ITEM_FORMAT_LONG;

    if ((EndPos - StartPos) >= 2) {
      HidItem->Size = *StartPos++;
      HidItem->Tag  = *StartPos++;

      if ((EndPos - StartPos) >= HidItem->Size) {
        HidItem->Data.LongData = StartPos;
        StartPos += HidItem->Size;
        return StartPos;
      }
    }
  } else {
    HidItem->Format = HID_ITEM_FORMAT_SHORT;
    //
    // bit 0, 1
    //
    HidItem->Size   = (UINT8) (Temp & 0x03);
    switch (HidItem->Size) {

    case 0:
      //
      // No data
      //
      return StartPos;

    case 1:
      //
      // One byte data
      //
      if ((EndPos - StartPos) >= 1) {
        HidItem->Data.U8 = *StartPos++;
        return StartPos;
      }

    case 2:
      //
      // Two byte data
      //
      if ((EndPos - StartPos) >= 2) {
        CopyMem (&HidItem->Data.U16, StartPos, sizeof (UINT16));
        StartPos += 2;
        return StartPos;
      }

    case 3:
      //
      // 4 byte data, adjust size
      //
      HidItem->Size++;
      if ((EndPos - StartPos) >= 4) {
        CopyMem (&HidItem->Data.U32, StartPos, sizeof (UINT32));
        StartPos += 4;
        return StartPos;
      }
    }
  }

  return NULL;
}


/**
  Get item data from report descriptor.

  @param  HidItem           The pointer to HID_ITEM.

  @return The Data of HidItem.

**/
UINT32
GetItemData (
  IN  HID_ITEM *HidItem
  )
{
  //
  // Get Data from HID_ITEM structure
  //
  switch (HidItem->Size) {

  case 1:
    return HidItem->Data.U8;

  case 2:
    return HidItem->Data.U16;

  case 4:
    return HidItem->Data.U32;
  }

  return 0;
}


/**
  Parse local item from report descriptor.

  @param  UsbMouse          The instance of USB_MOUSE_DEV
  @param  LocalItem         The pointer to local hid item

**/
VOID
ParseLocalItem (
  IN  USB_MOUSE_DEV   *UsbMouse,
  IN  HID_ITEM        *LocalItem
  )
{
  UINT32  Data;

  if (LocalItem->Size == 0) {
    //
    // No expected data for local item
    //
    return ;
  }

  Data = GetItemData (LocalItem);

  switch (LocalItem->Tag) {

  case HID_LOCAL_ITEM_TAG_DELIMITER:
    //
    // we don't support delimiter here
    //
    return ;

  case HID_LOCAL_ITEM_TAG_USAGE:
    return ;

  case HID_LOCAL_ITEM_TAG_USAGE_MINIMUM:
    if (UsbMouse->PrivateData.ButtonDetected) {
      UsbMouse->PrivateData.ButtonMinIndex = (UINT8) Data;
    }

    return ;

  case HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM:
    {
      if (UsbMouse->PrivateData.ButtonDetected) {
        UsbMouse->PrivateData.ButtonMaxIndex = (UINT8) Data;
      }

      return ;
    }
  }
}


/**
  Parse global item from report descriptor.

  @param  UsbMouse          The instance of USB_MOUSE_DEV
  @param  GlobalItem          The pointer to global hid item

**/
VOID
ParseGlobalItem (
  IN  USB_MOUSE_DEV   *UsbMouse,
  IN  HID_ITEM        *GlobalItem
  )
{
  UINT8 UsagePage;

  switch (GlobalItem->Tag) {
  case HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
    {
      UsagePage = (UINT8) GetItemData (GlobalItem);

      //
      // We only care Button Page here
      //
      if (UsagePage == 0x09) {
        //
        // Button Page
        //
        UsbMouse->PrivateData.ButtonDetected = TRUE;
        return ;
      }
      break;
    }

  }
}


/**
  Parse main item from report descriptor.

  @param  UsbMouse         The instance of USB_MOUSE_DEV
  @param  MainItem          Main hid item to parse

**/
VOID
ParseMainItem (
  IN  USB_MOUSE_DEV   *UsbMouse,
  IN  HID_ITEM        *MainItem
  )
{
  //
  // we don't care any main items, just skip
  //
  return ;
}


/**
  Parse hid item from report descriptor.

  @param  UsbMouse          The instance of USB_MOUSE_DEV
  @param  HidItem           The hid item to parse

**/
VOID
ParseHidItem (
  IN  USB_MOUSE_DEV   *UsbMouse,
  IN  HID_ITEM        *HidItem
  )
{
  switch (HidItem->Type) {

  case HID_ITEM_TYPE_MAIN:
    //
    // For Main Item, parse main item
    //
    ParseMainItem (UsbMouse, HidItem);
    break;

  case HID_ITEM_TYPE_GLOBAL:
    //
    // For global Item, parse global item
    //
    ParseGlobalItem (UsbMouse, HidItem);
    break;

  case HID_ITEM_TYPE_LOCAL:
    //
    // For Local Item, parse local item
    //
    ParseLocalItem (UsbMouse, HidItem);
    break;
  }
}


/**
  Parse Mouse Report Descriptor.

  @param  UsbMouse          The instance of USB_MOUSE_DEV
  @param  ReportDescriptor  Report descriptor to parse
  @param  ReportSize        Report descriptor size

  @retval EFI_DEVICE_ERROR  Report descriptor error
  @retval EFI_SUCCESS       Parse descriptor success

**/
EFI_STATUS
ParseMouseReportDescriptor (
  IN  USB_MOUSE_DEV   *UsbMouse,
  IN  UINT8           *ReportDescriptor,
  IN  UINTN           ReportSize
  )
{
  UINT8     *DescriptorEnd;
  UINT8     *Ptr;
  HID_ITEM  HidItem;

  DescriptorEnd = ReportDescriptor + ReportSize;

  Ptr           = GetNextItem (ReportDescriptor, DescriptorEnd, &HidItem);

  while (Ptr != NULL) {
    if (HidItem.Format != HID_ITEM_FORMAT_SHORT) {
      //
      // Long Format Item is not supported at current HID revision
      //
      return EFI_DEVICE_ERROR;
    }

    ParseHidItem (UsbMouse, &HidItem);

    Ptr = GetNextItem (Ptr, DescriptorEnd, &HidItem);
  }

  UsbMouse->NumberOfButtons                 = (UINT8) (UsbMouse->PrivateData.ButtonMaxIndex - UsbMouse->PrivateData.ButtonMinIndex + 1);
  UsbMouse->XLogicMax                       = UsbMouse->YLogicMax = 127;
  UsbMouse->XLogicMin                       = UsbMouse->YLogicMin = -127;

  return EFI_SUCCESS;
}
