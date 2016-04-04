/** @file
  Support for PCI 3.0 standard.

  Copyright (c) 2006, Intel Corporation                                                         
  All rights reserved. This program and the accompanying materials                          
  are licensed and made available under the terms and conditions of the BSD License         
  which accompanies this distribution.  The full text of the license may be found at        
  http://opensource.org/licenses/bsd-license.php                                            

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

**/

#ifndef __PCI30_H__
#define __PCI30_H__

#define PCI_CLASS_MASS_STORAGE_SATADPA   0x06
#define PCI_CLASS_MASS_STORAGE_AHCI      PCI_CLASS_MASS_STORAGE_SATADPA

#pragma pack(1)

typedef struct {
  UINT32  Signature;    // "PCIR"
  UINT16  VendorId;
  UINT16  DeviceId;
  UINT16  DeviceListOffset;
  UINT16  Length;
  UINT8   Revision;
  UINT8   ClassCode[3];
  UINT16  ImageLength;
  UINT16  CodeRevision;
  UINT8   CodeType;
  UINT8   Indicator;
  UINT16  MaxRuntimeImageLength;
  UINT16  ConfigUtilityCodeHeaderOffset;
  UINT16  DMTFCLPEntryPointOffset;
} PCI_3_0_DATA_STRUCTURE;

#pragma pack()


#include <IndustryStandard/Pci23.h>

#endif
