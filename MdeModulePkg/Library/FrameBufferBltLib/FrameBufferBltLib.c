/** @file
  FrameBufferBltLib - Library to perform blt operations on a framebuffer.

  Copyright (c) 2007 - 2015, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
  IMPLIED.

**/

#include <Uefi/UefiBaseType.h>
#include <Protocol/GraphicsOutput.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/DebugLib.h>

#define MAX_LINE_BUFFER_SIZE \
  (SIZE_4KB * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL))

typedef struct _FRAMEBUFFER_CONFIG {
  UINTN                           ColorDepth;
  UINTN                           WidthInBytes;
  UINTN                           BytesPerPixel;
  UINTN                           WidthInPixels;
  UINTN                           Height;
  UINT8                           LineBuffer[MAX_LINE_BUFFER_SIZE];
  UINT8                           *FrameBuffer;
  EFI_GRAPHICS_PIXEL_FORMAT       PixelFormat;
  EFI_PIXEL_BITMASK               PixelMasks;
  INTN                            PixelShl[4]; // R-G-B-Rsvd
  INTN                            PixelShr[4]; // R-G-B-Rsvd
} FRAMEBUFFER_CONFIG;


VOID
ConfigurePixelBitMaskFormat (
  IN FRAMEBUFFER_CONFIG         *Config,
  IN EFI_PIXEL_BITMASK          *BitMask
  )
{
  UINTN   Loop;
  UINT32  *Masks;
  UINT32  MergedMasks;

  MergedMasks = 0;
  Masks = (UINT32*) BitMask;
  for (Loop = 0; Loop < 3; Loop++) {
    ASSERT ((Loop == 3) || (Masks[Loop] != 0));
    ASSERT ((MergedMasks & Masks[Loop]) == 0);
    Config->PixelShl[Loop] = HighBitSet32 (Masks[Loop]) - 23 + (Loop * 8);
    if (Config->PixelShl[Loop] < 0) {
      Config->PixelShr[Loop] = -Config->PixelShl[Loop];
      Config->PixelShl[Loop] = 0;
    } else {
      Config->PixelShr[Loop] = 0;
    }
    MergedMasks = (UINT32) (MergedMasks | Masks[Loop]);
    DEBUG ((EFI_D_VERBOSE, "%d: shl:%d shr:%d mask:%x\n", Loop,
            Config->PixelShl[Loop], Config->PixelShr[Loop], Masks[Loop]));
  }
  MergedMasks = (UINT32) (MergedMasks | Masks[3]);

  ASSERT (MergedMasks != 0);
  Config->BytesPerPixel = (UINTN) ((HighBitSet32 (MergedMasks) + 7) / 8);

  DEBUG ((EFI_D_VERBOSE, "Bytes per pixel: %d\n", Config->BytesPerPixel));

  CopyMem (&Config->PixelMasks, BitMask, sizeof (*BitMask));
}


/**
  Configure the FrameBufferBltLib for a frame-buffer

  The configuration is stored in a buffer returned. The returned buffer should
  only be freed by calling FrameBufferBltFreeConfiguration.

  @param[in] FrameBuffer      Pointer to the start of the frame buffer
  @param[in] FrameBufferInfo  Describes the frame buffer characteristics
  @param[in,out] Buffer       Buffer to store configuration information
  @param[in,out] BufferSize   Size of Buffer.

  @retval RETURN_SUCCESS            The configuration was successful
  @retval RETURN_BUFFER_TOO_SMALL   The Buffer is to too small. The required
                                    size is returned in BufferSize.
  @retval RETURN_UNSUPPORTED        The requested mode is not supported by
                                    this implementaion.

**/
RETURN_STATUS
EFIAPI
FrameBufferBltConfigure (
  IN      VOID                                  *FrameBuffer,
  IN      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *FrameBufferInfo,
  IN OUT  VOID                                  *Buffer,
  IN OUT  UINTN                                 *BufferSize
  )
{
  STATIC EFI_PIXEL_BITMASK  RgbPixelMasks =
    { 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 };
  STATIC EFI_PIXEL_BITMASK  BgrPixelMasks =
    { 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000 };
  FRAMEBUFFER_CONFIG  *Config = (FRAMEBUFFER_CONFIG *) Buffer;

  if (BufferSize == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  if (*BufferSize < sizeof (FRAMEBUFFER_CONFIG)) {
    *BufferSize = sizeof (FRAMEBUFFER_CONFIG);
    return RETURN_BUFFER_TOO_SMALL;
  }

  if (Buffer == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  switch (FrameBufferInfo->PixelFormat) {
  case PixelRedGreenBlueReserved8BitPerColor:
    ConfigurePixelBitMaskFormat (Config, &RgbPixelMasks);
    break;
  case PixelBlueGreenRedReserved8BitPerColor:
    ConfigurePixelBitMaskFormat (Config, &BgrPixelMasks);
    break;
  case PixelBitMask:
    ConfigurePixelBitMaskFormat (Config, &(FrameBufferInfo->PixelInformation));
    break;
  case PixelBltOnly:
    return RETURN_UNSUPPORTED;
  default:
    return RETURN_INVALID_PARAMETER;
  }
  Config->PixelFormat = FrameBufferInfo->PixelFormat;

  Config->FrameBuffer = (UINT8*) FrameBuffer;
  Config->WidthInPixels = (UINTN) FrameBufferInfo->HorizontalResolution;
  Config->Height = (UINTN) FrameBufferInfo->VerticalResolution;
  Config->WidthInBytes = Config->WidthInPixels * Config->BytesPerPixel;

  ASSERT (Config->WidthInBytes < sizeof (Config->LineBuffer));

  return RETURN_SUCCESS;
}


/**
  Performs a UEFI Graphics Output Protocol Blt Video Fill.

  @param[in]  Color         Color to fill the region with
  @param[in]  DestinationX  X location to start fill operation
  @param[in]  DestinationY  Y location to start fill operation
  @param[in]  Width         Width (in pixels) to fill
  @param[in]  Height        Height to fill

  @retval  RETURN_INVALID_PARAMETER - Invalid parameter passed in
  @retval  RETURN_SUCCESS - The sizes were returned

**/
EFI_STATUS
EFIAPI
FrameBufferBltVideoFill (
  IN FRAMEBUFFER_CONFIG                     *Config,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *Color,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height
  )
{
  UINTN                           DstY;
  VOID                            *BltMemDst;
  UINTN                           X;
  UINT8                           Uint8;
  UINT32                          Uint32;
  UINT64                          WideFill;
  BOOLEAN                         UseWideFill;
  BOOLEAN                         LineBufferReady;
  UINTN                           Offset;
  UINTN                           WidthInBytes;
  UINTN                           SizeInBytes;

  //
  // BltBuffer to Video: Source is BltBuffer, destination is Video
  //
  if (DestinationY + Height > Config->Height) {
    DEBUG ((EFI_D_VERBOSE, "VideoFill: Past screen (Y)\n"));
    return RETURN_INVALID_PARAMETER;
  }

  if (DestinationX + Width > Config->WidthInPixels) {
    DEBUG ((EFI_D_VERBOSE, "VideoFill: Past screen (X)\n"));
    return RETURN_INVALID_PARAMETER;
  }

  if (Width == 0 || Height == 0) {
    DEBUG ((EFI_D_VERBOSE, "VideoFill: Width or Height is 0\n"));
    return RETURN_INVALID_PARAMETER;
  }

  WidthInBytes = Width * Config->BytesPerPixel;

  Uint32 = *(UINT32*) Color;
  WideFill =
    (UINT32) (
        (((Uint32 << Config->PixelShl[0]) >> Config->PixelShr[0]) &
         Config->PixelMasks.RedMask) |
        (((Uint32 << Config->PixelShl[1]) >> Config->PixelShr[1]) &
         Config->PixelMasks.GreenMask) |
        (((Uint32 << Config->PixelShl[2]) >> Config->PixelShr[2]) &
         Config->PixelMasks.BlueMask)
      );
  DEBUG ((EFI_D_VERBOSE, "VideoFill: color=0x%x, wide-fill=0x%x\n",
          Uint32, WideFill));

  //
  // If the size of the pixel data evenly divides the sizeof
  // WideFill, then a wide fill operation can be used
  //
  UseWideFill = TRUE;
  if ((sizeof (WideFill) % Config->BytesPerPixel) == 0) {
    for (X = Config->BytesPerPixel; X < sizeof (WideFill); X++) {
      ((UINT8*)&WideFill)[X] = ((UINT8*)&WideFill)[X % Config->BytesPerPixel];
    }
  } else {
    //
    // If all the bytes in the pixel are the same value, then use
    // a wide fill operation.
    //
    for (
      X = 1, Uint8 = ((UINT8*)&WideFill)[0];
      X < Config->BytesPerPixel;
      X++) {
      if (Uint8 != ((UINT8*)&WideFill)[X]) {
        UseWideFill = FALSE;
        break;
      }
    }
    if (UseWideFill) {
      SetMem ((VOID*) &WideFill, sizeof (WideFill), Uint8);
    }
  }

  if (UseWideFill && (DestinationX == 0) && (Width == Config->WidthInPixels)) {
    DEBUG ((EFI_D_VERBOSE, "VideoFill (wide, one-shot)\n"));
    Offset = DestinationY * Config->WidthInPixels;
    Offset = Config->BytesPerPixel * Offset;
    BltMemDst = (VOID*) (Config->FrameBuffer + Offset);
    SizeInBytes = WidthInBytes * Height;
    if (SizeInBytes >= 8) {
      SetMem32 (BltMemDst, SizeInBytes & ~3, (UINT32) WideFill);
      SizeInBytes = SizeInBytes & 3;
    }
    if (SizeInBytes > 0) {
      SetMem (BltMemDst, SizeInBytes, (UINT8)(UINTN) WideFill);
    }
  } else {
    LineBufferReady = FALSE;
    for (DstY = DestinationY; DstY < (Height + DestinationY); DstY++) {
      Offset = (DstY * Config->WidthInPixels) + DestinationX;
      Offset = Config->BytesPerPixel * Offset;
      BltMemDst = (VOID*) (Config->FrameBuffer + Offset);

      if (UseWideFill && (((UINTN) BltMemDst & 7) == 0)) {
        DEBUG ((EFI_D_VERBOSE, "VideoFill (wide)\n"));
        SizeInBytes = WidthInBytes;
        if (SizeInBytes >= 8) {
          SetMem64 (BltMemDst, SizeInBytes & ~7, WideFill);
          SizeInBytes = SizeInBytes & 7;
        }
        if (SizeInBytes > 0) {
          CopyMem (BltMemDst, (VOID*) &WideFill, SizeInBytes);
        }
      } else {
        DEBUG ((EFI_D_VERBOSE, "VideoFill (not wide)\n"));
        if (!LineBufferReady) {
          CopyMem (Config->LineBuffer, &WideFill, Config->BytesPerPixel);
          for (X = 1; X < Width; ) {
            CopyMem(
              (Config->LineBuffer + (X * Config->BytesPerPixel)),
              Config->LineBuffer,
              MIN (X, Width - X) * Config->BytesPerPixel
              );
            X = X + MIN (X, Width - X);
          }
          LineBufferReady = TRUE;
        }
        CopyMem (BltMemDst, Config->LineBuffer, WidthInBytes);
      }
    }
  }

  return RETURN_SUCCESS;
}


/**
  Performs a UEFI Graphics Output Protocol Blt Video to Buffer operation
  with extended parameters.

  @param[out] BltBuffer     Output buffer for pixel color data
  @param[in]  SourceX       X location within video
  @param[in]  SourceY       Y location within video
  @param[in]  DestinationX  X location within BltBuffer
  @param[in]  DestinationY  Y location within BltBuffer
  @param[in]  Width         Width (in pixels)
  @param[in]  Height        Height
  @param[in]  Delta         Number of bytes in a row of BltBuffer

  @retval  RETURN_INVALID_PARAMETER - Invalid parameter passed in
  @retval  RETURN_SUCCESS - The sizes were returned

**/
EFI_STATUS
EFIAPI
FrameBufferBltVideoToBltBufferEx (
  IN FRAMEBUFFER_CONFIG                     *Config,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height,
  IN  UINTN                                 Delta
  )
{
  UINTN                           DstY;
  UINTN                           SrcY;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Blt;
  VOID                            *BltMemSrc;
  VOID                            *BltMemDst;
  UINTN                           X;
  UINT32                          Uint32;
  UINTN                           Offset;
  UINTN                           WidthInBytes;

  //
  // Video to BltBuffer: Source is Video, destination is BltBuffer
  //
  if (SourceY + Height > Config->Height) {
    return RETURN_INVALID_PARAMETER;
  }

  if (SourceX + Width > Config->WidthInPixels) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Width == 0 || Height == 0) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // If Delta is zero, then the entire BltBuffer is being used, so Delta is
  // the number of bytes in each row of BltBuffer. Since BltBuffer is Width
  // pixels size, the number of bytes in each row can be computed.
  //
  if (Delta == 0) {
    Delta = Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  }

  WidthInBytes = Width * Config->BytesPerPixel;

  //
  // Video to BltBuffer: Source is Video, destination is BltBuffer
  //
  for (SrcY = SourceY, DstY = DestinationY;
       DstY < (Height + DestinationY);
       SrcY++, DstY++) {

    Offset = (SrcY * Config->WidthInPixels) + SourceX;
    Offset = Config->BytesPerPixel * Offset;
    BltMemSrc = (VOID *) (Config->FrameBuffer + Offset);

    if (Config->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
      BltMemDst =
        (VOID *) (
            (UINT8 *) BltBuffer +
            (DstY * Delta) +
            (DestinationX * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL))
          );
    } else {
      BltMemDst = (VOID *) Config->LineBuffer;
    }

    CopyMem (BltMemDst, BltMemSrc, WidthInBytes);

    if (Config->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) {
      for (X = 0; X < Width; X++) {
        Blt = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)
          ((UINT8 *) BltBuffer + (DstY * Delta) +
           (DestinationX + X) * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
        Uint32 = *(UINT32*) (Config->LineBuffer + (X * Config->BytesPerPixel));
        *(UINT32*) Blt =
          (UINT32) (
              (((Uint32 & Config->PixelMasks.RedMask) >>
                Config->PixelShl[0]) << Config->PixelShr[0]) |
              (((Uint32 & Config->PixelMasks.GreenMask) >>
                Config->PixelShl[1]) << Config->PixelShr[1]) |
              (((Uint32 & Config->PixelMasks.BlueMask)  >>
                Config->PixelShl[2]) << Config->PixelShr[2])
            );
      }
    }
  }

  return RETURN_SUCCESS;
}


/**
  Performs a UEFI Graphics Output Protocol Blt Video to Buffer operation.

  @param[out] BltBuffer     Output buffer for pixel color data
  @param[in]  SourceX       X location within video
  @param[in]  SourceY       Y location within video
  @param[in]  Width         Width (in pixels)
  @param[in]  Height        Height

  @retval  RETURN_INVALID_PARAMETER - Invalid parameter passed in
  @retval  RETURN_SUCCESS - The sizes were returned

**/
EFI_STATUS
EFIAPI
FrameBufferBltVideoToBltBuffer (
  IN FRAMEBUFFER_CONFIG                     *Config,
  OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height
  )
{
  return FrameBufferBltVideoToBltBufferEx (
           Config,
           BltBuffer,
           SourceX,
           SourceY,
           0,
           0,
           Width,
           Height,
           0
           );
}


/**
  Performs a UEFI Graphics Output Protocol Blt Buffer to Video operation
  with extended parameters.

  @param[in]  BltBuffer     Output buffer for pixel color data
  @param[in]  SourceX       X location within BltBuffer
  @param[in]  SourceY       Y location within BltBuffer
  @param[in]  DestinationX  X location within video
  @param[in]  DestinationY  Y location within video
  @param[in]  Width         Width (in pixels)
  @param[in]  Height        Height
  @param[in]  Delta         Number of bytes in a row of BltBuffer

  @retval  RETURN_INVALID_PARAMETER - Invalid parameter passed in
  @retval  RETURN_SUCCESS - The sizes were returned

**/
EFI_STATUS
EFIAPI
FrameBufferBltBufferToVideoEx (
  IN  FRAMEBUFFER_CONFIG                    *Config,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height,
  IN  UINTN                                 Delta
  )
{
  UINTN                           DstY;
  UINTN                           SrcY;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Blt;
  VOID                            *BltMemSrc;
  VOID                            *BltMemDst;
  UINTN                           X;
  UINT32                          Uint32;
  UINTN                           Offset;
  UINTN                           WidthInBytes;

  //
  // BltBuffer to Video: Source is BltBuffer, destination is Video
  //
  if (DestinationY + Height > Config->Height) {
    return RETURN_INVALID_PARAMETER;
  }

  if (DestinationX + Width > Config->WidthInPixels) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Width == 0 || Height == 0) {
    return RETURN_INVALID_PARAMETER;
  }

  //
  // If Delta is zero, then the entire BltBuffer is being used, so Delta is
  // the number of bytes in each row of BltBuffer. Since BltBuffer is Width
  // pixels size, the number of bytes in each row can be computed.
  //
  if (Delta == 0) {
    Delta = Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  }

  WidthInBytes = Width * Config->BytesPerPixel;

  for (SrcY = SourceY, DstY = DestinationY;
       SrcY < (Height + SourceY);
       SrcY++, DstY++) {

    Offset = (DstY * Config->WidthInPixels) + DestinationX;
    Offset = Config->BytesPerPixel * Offset;
    BltMemDst = (VOID*) (Config->FrameBuffer + Offset);

    if (Config->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
      BltMemSrc = (VOID *) ((UINT8 *) BltBuffer + (SrcY * Delta));
    } else {
      for (X = 0; X < Width; X++) {
        Blt =
          (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) (
              (UINT8 *) BltBuffer +
              (SrcY * Delta) +
              ((SourceX + X) * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL))
            );
        Uint32 = *(UINT32*) Blt;
        *(UINT32*) (Config->LineBuffer + (X * Config->BytesPerPixel)) =
          (UINT32) (
              (((Uint32 << Config->PixelShl[0]) >> Config->PixelShr[0]) &
               Config->PixelMasks.RedMask) |
              (((Uint32 << Config->PixelShl[1]) >> Config->PixelShr[1]) &
               Config->PixelMasks.GreenMask) |
              (((Uint32 << Config->PixelShl[2]) >> Config->PixelShr[2]) &
               Config->PixelMasks.BlueMask)
            );
      }
      BltMemSrc = (VOID *) Config->LineBuffer;
    }

    CopyMem (BltMemDst, BltMemSrc, WidthInBytes);
  }

  return RETURN_SUCCESS;
}


/**
  Performs a UEFI Graphics Output Protocol Blt Buffer to Video operation.

  @param[in]  BltBuffer     Output buffer for pixel color data
  @param[in]  DestinationX  X location within video
  @param[in]  DestinationY  Y location within video
  @param[in]  Width         Width (in pixels)
  @param[in]  Height        Height

  @retval  RETURN_INVALID_PARAMETER - Invalid parameter passed in
  @retval  RETURN_SUCCESS - The sizes were returned

**/
EFI_STATUS
EFIAPI
FrameBufferBltBufferToVideo (
  IN FRAMEBUFFER_CONFIG                     *Config,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height
  )
{
  return FrameBufferBltBufferToVideoEx (
           Config,
           BltBuffer,
           0,
           0,
           DestinationX,
           DestinationY,
           Width,
           Height,
           0
           );
}


/**
  Performs a UEFI Graphics Output Protocol Blt Video to Video operation

  @param[in]  SourceX       X location within video
  @param[in]  SourceY       Y location within video
  @param[in]  DestinationX  X location within video
  @param[in]  DestinationY  Y location within video
  @param[in]  Width         Width (in pixels)
  @param[in]  Height        Height

  @retval  RETURN_INVALID_PARAMETER - Invalid parameter passed in
  @retval  RETURN_SUCCESS - The sizes were returned

**/
EFI_STATUS
EFIAPI
FrameBufferBltVideoToVideo (
  IN FRAMEBUFFER_CONFIG                     *Config,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height
  )
{
  VOID                            *BltMemSrc;
  VOID                            *BltMemDst;
  UINTN                           Offset;
  UINTN                           WidthInBytes;
  INTN                            LineStride;

  //
  // Video to Video: Source is Video, destination is Video
  //
  if (SourceY + Height > Config->Height) {
    return RETURN_INVALID_PARAMETER;
  }

  if (SourceX + Width > Config->WidthInPixels) {
    return RETURN_INVALID_PARAMETER;
  }

  if (DestinationY + Height > Config->Height) {
    return RETURN_INVALID_PARAMETER;
  }

  if (DestinationX + Width > Config->WidthInPixels) {
    return RETURN_INVALID_PARAMETER;
  }

  if (Width == 0 || Height == 0) {
    return RETURN_INVALID_PARAMETER;
  }

  WidthInBytes = Width * Config->BytesPerPixel;

  Offset = (SourceY * Config->WidthInPixels) + SourceX;
  Offset = Config->BytesPerPixel * Offset;
  BltMemSrc = (VOID *) (Config->FrameBuffer + Offset);

  Offset = (DestinationY * Config->WidthInPixels) + DestinationX;
  Offset = Config->BytesPerPixel * Offset;
  BltMemDst = (VOID *) (Config->FrameBuffer + Offset);

  LineStride = Config->WidthInBytes;
  if ((UINTN) BltMemDst > (UINTN) BltMemSrc) {
    LineStride = -LineStride;
  }

  while (Height > 0) {
    CopyMem (BltMemDst, BltMemSrc, WidthInBytes);

    BltMemSrc = (VOID*) ((UINT8*) BltMemSrc + LineStride);
    BltMemDst = (VOID*) ((UINT8*) BltMemDst + LineStride);
    Height--;
  }

  return RETURN_SUCCESS;
}


/**
  Performs a UEFI Graphics Output Protocol Blt operation.

  @param[in] Config            - Pointer to a configuration which was
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
  IN  VOID                                  *Config,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer, OPTIONAL
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION     BltOperation,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height,
  IN  UINTN                                 Delta
  )
{
  FRAMEBUFFER_CONFIG  *FbConfig = (FRAMEBUFFER_CONFIG *) Config;

  switch (BltOperation) {
  case EfiBltVideoToBltBuffer:
    return FrameBufferBltVideoToBltBufferEx (
             FbConfig,
             BltBuffer,
             SourceX,
             SourceY,
             DestinationX,
             DestinationY,
             Width,
             Height,
             Delta
             );

  case EfiBltVideoToVideo:
    return FrameBufferBltVideoToVideo (
             FbConfig,
             SourceX,
             SourceY,
             DestinationX,
             DestinationY,
             Width,
             Height
             );

  case EfiBltVideoFill:
    return FrameBufferBltVideoFill (
             FbConfig,
             BltBuffer,
             DestinationX,
             DestinationY,
             Width,
             Height
             );

  case EfiBltBufferToVideo:
    return FrameBufferBltBufferToVideoEx (
             FbConfig,
             BltBuffer,
             SourceX,
             SourceY,
             DestinationX,
             DestinationY,
             Width,
             Height,
             Delta
             );
  default:
    return RETURN_INVALID_PARAMETER;
  }
}
