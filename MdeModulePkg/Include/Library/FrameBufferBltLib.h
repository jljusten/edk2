/** @file
  Library for performing UEFI GOP Blt operations on a framebuffer

  Copyright (c) 2009 - 2015, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
  IMPLIED.

**/

#ifndef __FRAMEBUFFER_BLT_LIB__
#define __FRAMEBUFFER_BLT_LIB__

#include <Protocol/GraphicsOutput.h>


/**
  Configure the FrameBufferBltLib for a frame-buffer

  The configuration is stored in a buffer returned. The returned buffer should
  only be freed by calling FrameBufferBltFreeConfiguration.

  @param[in] FrameBuffer      Pointer to the start of the frame buffer
  @param[in] FrameBufferInfo  Describes the frame buffer characteristics
  @param[in,out] Config       Buffer to store configuration information
  @param[in,out] ConfigSize   Size of Buffer.

  @retval RETURN_SUCCESS            The configuration was successful
  @retval RETURN_BUFFER_TOO_SMALL   The Config buffer is to too small. The
                                    required size is returned in ConfigSize.
  @retval RETURN_UNSUPPORTED        The requested mode is not supported by
                                    this implementaion.

**/
RETURN_STATUS
EFIAPI
FrameBufferBltConfigure (
  IN      VOID                                  *FrameBuffer,
  IN      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *FrameBufferInfo,
  IN OUT  VOID                                  *Config,
  IN OUT  UINTN                                 *ConfigSize
  );


/**
  Performs a UEFI Graphics Output Protocol Blt operation.

  @param[in]     Config        - Pointer to a configuration which was
                                 successfully configured by
                                 FrameBufferBltConfigure
  @param[in,out] BltBuffer     - The data to transfer to screen
  @param[in]     BltOperation  - The operation to perform
  @param[in]     SourceX       - The X coordinate of the source for
                                 BltOperation
  @param[in]     SourceY       - The Y coordinate of the source for
                                 BltOperation
  @param[in]     DestinationX  - The X coordinate of the destination for
                                 BltOperation
  @param[in]     DestinationY  - The Y coordinate of the destination for
                                 BltOperation
  @param[in]     Width         - The width of a rectangle in the blt rectangle
                                 in pixels
  @param[in]     Height        - The height of a rectangle in the blt rectangle
                                 in pixels
  @param[in]     Delta         - Not used for EfiBltVideoFill and
                                 EfiBltVideoToVideo operation. If a Delta of 0
                                 is used, the entire BltBuffer will be operated
                                 on. If a subrectangle of the BltBuffer is
                                 used, then Delta represents the number of
                                 bytes in a row of the BltBuffer.

  @retval  RETURN_INVALID_PARAMETER - Invalid parameter passed in
  @retval  RETURN_SUCCESS      - Blt operation success

**/
RETURN_STATUS
EFIAPI
FrameBufferBlt (
  IN     VOID                               *Config,
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer, OPTIONAL
  IN     EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN     UINTN                              SourceX,
  IN     UINTN                              SourceY,
  IN     UINTN                              DestinationX,
  IN     UINTN                              DestinationY,
  IN     UINTN                              Width,
  IN     UINTN                              Height,
  IN     UINTN                              Delta
  );


#endif

