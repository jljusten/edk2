/** @file
*
*  Copyright (c) 2011, ARM Limited. All rights reserved.
*  
*  This program and the accompanying materials                          
*  are licensed and made available under the terms and conditions of the BSD License         
*  which accompanies this distribution.  The full text of the license may be found at        
*  http://opensource.org/licenses/bsd-license.php                                            
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             
*
**/

#include <Library/ArmPlatformLib.h>
#include <Library/DebugLib.h>

/**
  Return the Virtual Memory Map of your platform

  This Virtual Memory Map is used by MemoryInitPei Module to initialize the MMU on your platform.

  @param[out]   VirtualMemoryMap    Array of ARM_MEMORY_REGION_DESCRIPTOR describing a Physical-to-
                                    Virtual Memory mapping. This array must be ended by a zero-filled
                                    entry

**/
VOID
ArmPlatformGetVirtualMemoryMap (
  IN ARM_MEMORY_REGION_DESCRIPTOR** VirtualMemoryMap
  )
{
  ASSERT(0);
}

/**
  Return the EFI Memory Map of your platform

  This EFI Memory Map of the System Memory is used by MemoryInitPei module to create the Resource
  Descriptor HOBs used by DXE core.

  @param[out]   EfiMemoryMap        Array of ARM_SYSTEM_MEMORY_REGION_DESCRIPTOR describing an
                                    EFI Memory region. This array must be ended by a zero-filled entry

**/
EFI_STATUS
ArmPlatformGetAdditionalSystemMemory (
  OUT ARM_SYSTEM_MEMORY_REGION_DESCRIPTOR** EfiMemoryMap
  )
{
  return EFI_UNSUPPORTED;
}
