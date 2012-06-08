/** @file
  CPU DXE AP Startup

  Copyright (c) 2008 - 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "CpuDxe.h"
#include "CpuGdt.h"
#include "CpuMp.h"

#pragma pack(1)

typedef struct {
  UINT8  JmpToCli[2];

  UINT16 GdtLimit;
  UINT32 GdtBase;

  UINT8  Cli;

  UINT8  MovAxRealSegment; UINT16 RealSegment;
  UINT8  MovDsAx[2];

  UINT8  MovBxGdtr[3];
  UINT8  LoadGdt[5];

  UINT8  MovEaxCr0[2];
  UINT32 MovEaxCr0Value;
  UINT8  MovCr0Eax[3];

  UINT8  FarJmp32Flat[2]; UINT32 FlatJmpOffset; UINT16 FlatJmpSelector;

  //
  // Now in IA32
  //
  UINT8  MovEaxCr4;
  UINT32 MovEaxCr4Value;
  UINT8  MovCr4Eax[3];

  UINT8  MoveDataSelectorIntoAx[2]; UINT16 FlatDataSelector;
  UINT8  MoveFlatDataSelectorFromAxToDs[2];
  UINT8  MoveFlatDataSelectorFromAxToEs[2];
  UINT8  MoveFlatDataSelectorFromAxToFs[2];
  UINT8  MoveFlatDataSelectorFromAxToGs[2];
  UINT8  MoveFlatDataSelectorFromAxToSs[2];

#if defined (MDE_CPU_X64)
  //
  // Transition to X64
  //
  UINT8  MovEaxCr3;
  UINT32 Cr3Value;
  UINT8  MovCr3Eax[3];

  UINT8  MoveCr4ToEax[3];
  UINT8  SetCr4Bit5[4];
  UINT8  MoveEaxToCr4[3];

  UINT8  MoveLongModeEnableMsrToEcx[5];
  UINT8  ReadLmeMsr[2];
  UINT8  SetLongModeEnableBit[4];
  UINT8  WriteLmeMsr[2];

  UINT8  MoveCr0ToEax[3];
  UINT8  SetCr0PagingBit[4];
  UINT8  MoveEaxToCr0[3];
  //UINT8  DeadLoop[2];

  UINT8  FarJmp32LongMode; UINT32 LongJmpOffset; UINT16 LongJmpSelector;
#endif // defined (MDE_CPU_X64)

#if defined (MDE_CPU_X64)
  UINT8  MovEaxOrRaxCpuDxeEntry[2]; UINTN CpuDxeEntryValue;
#else
  UINT8  MovEaxOrRaxCpuDxeEntry; UINTN CpuDxeEntryValue;
#endif
  UINT8  JmpToCpuDxeEntry[2];

} STARTUP_CODE;

#pragma pack()

STARTUP_CODE mStartupCodeTemplate = {
  { 0xeb, 0x06 },                     // Jump to cli
  0,                                  // GDT Limit
  0,                                  // GDT Base
  0xfa,                               // cli (Clear Interrupts)
  0xb8, 0x0000,                       // mov ax, RealSegment
  { 0x8e, 0xd8 },                     // mov ds, ax
  { 0xBB, 0x02, 0x00 },               // mov bx, Gdtr
  { 0x3e, 0x66, 0x0f, 0x01, 0x17 },   // lgdt [ds:bx]
  { 0x66, 0xB8 }, 0x00000023,         // mov eax, cr0 value
  { 0x0F, 0x22, 0xC0 },               // mov cr0, eax
  { 0x66, 0xEA },                     // far jmp to 32-bit flat
        OFFSET_OF(STARTUP_CODE, MovEaxCr4),
        LINEAR_CODE_SEL,
  0xB8, 0x00000640,                   // mov eax, cr4 value
  { 0x0F, 0x22, 0xe0 },               // mov cr4, eax
  { 0x66, 0xb8 }, CPU_DATA_SEL,       // mov ax, FlatDataSelector
  { 0x8e, 0xd8 },                     // mov ds, ax
  { 0x8e, 0xc0 },                     // mov es, ax
  { 0x8e, 0xe0 },                     // mov fs, ax
  { 0x8e, 0xe8 },                     // mov gs, ax
  { 0x8e, 0xd0 },                     // mov ss, ax

#if defined (MDE_CPU_X64)
  0xB8, 0x00000000,                   // mov eax, cr3 value
  { 0x0F, 0x22, 0xd8 },               // mov cr3, eax

  { 0x0F, 0x20, 0xE0 },               // mov eax, cr4
  { 0x0F, 0xBA, 0xE8, 0x05 },         // bts eax, 5
  { 0x0F, 0x22, 0xE0 },               // mov cr4, eax

  { 0xB9, 0x80, 0x00, 0x00, 0xC0 },   // mov ecx, 0xc0000080
  { 0x0F, 0x32 },                     // rdmsr
  { 0x0F, 0xBA, 0xE8, 0x08 },         // bts eax, 8
  { 0x0F, 0x30 },                     // wrmsr

  { 0x0F, 0x20, 0xC0 },               // mov eax, cr0
  { 0x0F, 0xBA, 0xE8, 0x1F },         // bts eax, 31
  { 0x0F, 0x22, 0xC0 },               // mov cr0, eax

  0xEA,                               // FarJmp32LongMode
        OFFSET_OF(STARTUP_CODE, MovEaxOrRaxCpuDxeEntry),
        LINEAR_CODE64_SEL,
#endif // defined (MDE_CPU_X64)

  //0xeb, 0xfe,       // jmp $
#if defined (MDE_CPU_X64)
  { 0x48, 0xb8 }, 0x0,                // mov rax, X64 CpuDxe MP Entry Point
#else
  0xB8, 0x0,                          // mov eax, IA32 CpuDxe MP Entry Point
#endif
  { 0xff, 0xe0 },                     // jmp to eax/rax (CpuDxe MP Entry Point)

};


/**
  Starts the Application Processors and directs them to jump to the
  specified routine.

  The processor jumps to this code in flat mode, but the processor's
  stack is not initialized.

  @param ApEntryPoint    Pointer to the Entry Point routine

  @retval EFI_SUCCESS           The APs were started
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate memory to start APs

**/
EFI_STATUS
StartApsStackless (
  IN STACKLESS_AP_ENTRY_POINT ApEntryPoint
  )
{
  EFI_STATUS            Status;
  volatile STARTUP_CODE *StartupCode;
  IA32_DESCRIPTOR       Gdtr;
  EFI_PHYSICAL_ADDRESS  StartAddress;

  StartAddress = BASE_1MB;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  EfiACPIMemoryNVS,
                  EFI_SIZE_TO_PAGES (sizeof (*StartupCode)),
                  &StartAddress
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  StartupCode = (STARTUP_CODE*)(VOID*)(UINTN) StartAddress;
  CopyMem ((VOID*) StartupCode, &mStartupCodeTemplate, sizeof (*StartupCode));
  StartupCode->RealSegment = (UINT16) (((UINTN) StartAddress) >> 4);

  AsmReadGdtr (&Gdtr);
  StartupCode->GdtLimit = Gdtr.Limit;
  StartupCode->GdtBase = (UINT32) Gdtr.Base;

  StartupCode->CpuDxeEntryValue = (UINTN) ApEntryPoint;

  StartupCode->FlatJmpOffset += (UINT32) StartAddress;

#if defined (MDE_CPU_X64)
  StartupCode->Cr3Value = (UINT32) AsmReadCr3 ();
  StartupCode->LongJmpOffset += (UINT32) StartAddress;
#endif

  SendInitSipiSipiAllExcludingSelf ((UINT32)(UINTN)(VOID*) StartupCode);

  //
  // Wait 100 milliseconds for APs to arrive at the ApEntryPoint routine
  //
  MicroSecondDelay (100 * 1000);

  gBS->FreePages (StartAddress, EFI_SIZE_TO_PAGES (sizeof (*StartupCode)));

  return EFI_SUCCESS;
}

