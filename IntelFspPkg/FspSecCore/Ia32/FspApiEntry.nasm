;------------------------------------------------------------------------------
;
; Copyright (c) 2014, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Abstract:
;
;   Provide FSP API entry points.
;
;------------------------------------------------------------------------------

    SECTION .text

%include    "SaveRestoreSse.inc"
%include    "UcodeLoad.inc"

;
; Following are fixed PCDs
;
extern   ASM_PFX(PcdGet32(PcdTemporaryRamBase))
extern   ASM_PFX(PcdGet32(PcdTemporaryRamSize))
extern   ASM_PFX(PcdGet32(PcdFspTemporaryRamSize))

;
; Following functions will be provided in C
;
extern ASM_PFX(FspImageSizeOffset)
extern ASM_PFX(SecStartup)
extern ASM_PFX(FspApiCallingCheck)

;
; Following functions will be provided in PlatformSecLib
;
extern ASM_PFX(GetFspBaseAddress)
extern ASM_PFX(GetBootFirmwareVolumeOffset)
extern ASM_PFX(PlatformTempRamInit)
extern ASM_PFX(Pei2LoaderSwitchStack)
extern ASM_PFX(FspSelfCheck)
extern ASM_PFX(PlatformBasicInit)

;
; Define the data length that we saved on the stack top
;
%define DATA_LEN_OF_PER0 0x18
%define DATA_LEN_OF_MCUD 0x18
%define DATA_LEN_AT_STACK_TOP (DATA_LEN_OF_PER0 + DATA_LEN_OF_MCUD + 4)

;------------------------------------------------------------------------------
global ASM_PFX(LoadUcode)
ASM_PFX(LoadUcode):
   ; Inputs:
   ;   esp -> LOAD_UCODE_PARAMS pointer
   ; Register Usage:
   ;   esp  Preserved
   ;   All others destroyed
   ; Assumptions:
   ;   No memory available, stack is hard-coded and used for return address
   ;   Executed by SBSP and NBSP
   ;   Beginning of microcode update region starts on paragraph boundary

   ;
   ;
   ; Save return address to EBP
   mov    ebp, eax

   cmp    esp, 0
   jz     .paramerror
   mov    eax, dword [esp]    ; Parameter pointer
   cmp    eax, 0
   jz     .paramerror
   mov    esp, eax
   mov    esi, [esp + LOAD_UCODE_PARAMS.ucode_code_addr]
   cmp    esi, 0
   jnz    .check_main_header

.paramerror:
   mov    eax, 0x80000002
   jmp    .exit

   mov    esi, [esp + LOAD_UCODE_PARAMS.ucode_code_addr]

.check_main_header:
   ; Get processor signature and platform ID from the installed processor
   ; and save into registers for later use
   ; ebx = processor signature
   ; edx = platform ID
   mov   eax, 1
   cpuid
   mov   ebx, eax
   mov   ecx, MSR_IA32_PLATFORM_ID
   rdmsr
   mov   ecx, edx
   shr   ecx, 50-32
   and   ecx, 0x7
   mov   edx, 1
   shl   edx, cl

   ; Current register usage
   ; esp -> stack with paramters
   ; esi -> microcode update to check
   ; ebx = processor signature
   ; edx = platform ID

   ; Check for valid microcode header
   ; Minimal test checking for header version and loader version as 1
   mov   eax, dword 1
   cmp   [esi + ucode_hdr.version], eax
   jne   .advance_fixed_size
   cmp   [esi + ucode_hdr.loader], eax
   jne   .advance_fixed_size

   ; Check if signature and plaform ID match
   cmp   ebx, [esi + ucode_hdr.processor]
   jne   .0
   test  edx, [esi + ucode_hdr.flags]
   jnz   .load_check  ; Jif signature and platform ID match

.0:
   ; Check if extended header exists
   ; First check if total_size and data_size are valid
   xor   eax, eax
   cmp   [esi + ucode_hdr.total_size], eax
   je    .next_microcode
   cmp   [esi + ucode_hdr.data_size], eax
   je    .next_microcode

   ; Then verify total size - sizeof header > data size
   mov   ecx, [esi + ucode_hdr.total_size]
   sub   ecx, ucode_hdr.size
   cmp   ecx, [esi + ucode_hdr.data_size]
   jng   .next_microcode    ; Jif extended header does not exist

   ; Set edi -> extended header
   mov   edi, esi
   add   edi, ucode_hdr.size
   add   edi, [esi + ucode_hdr.data_size]

   ; Get count of extended structures
   mov   ecx, [edi + ext_sig_hdr.count]

   ; Move pointer to first signature structure
   add   edi, ext_sig_hdr.size

.check_ext_sig:
   ; Check if extended signature and platform ID match
   cmp   [edi + ext_sig.processor], ebx
   jne   .1
   test  [edi + ext_sig.flags], edx
   jnz   .load_check     ; Jif signature and platform ID match
.1:
   ; Check if any more extended signatures exist
   add   edi, ext_sig.size
   loop  .check_ext_sig

.next_microcode:
   ; Advance just after end of this microcode
   xor   eax, eax
   cmp   [esi + ucode_hdr.total_size], eax
   je    .2
   add   esi, [esi + ucode_hdr.total_size]
   jmp   .check_address
.2:
   add   esi, dword 2048
   jmp   .check_address

.advance_fixed_size:
   ; Advance by 4X dwords
   add   esi, dword 1024

.check_address:
   ; Is valid Microcode start point ?
   cmp   dword [esi], 0xffffffff
   jz    .done

   ; Address >= microcode region address + microcode region size?
   mov   eax, [esp + LOAD_UCODE_PARAMS.ucode_code_addr]
   add   eax, [esp + LOAD_UCODE_PARAMS.ucode_code_size]
   cmp   esi, eax
   jae   .done        ;Jif address is outside of ucode region
   jmp   .check_main_header

.load_check:
   ; Get the revision of the current microcode update loaded
   mov   ecx, MSR_IA32_BIOS_SIGN_ID
   xor   eax, eax               ; Clear EAX
   xor   edx, edx               ; Clear EDX
   wrmsr                        ; Load 0 to MSR at 8Bh

   mov   eax, 1
   cpuid
   mov   ecx, MSR_IA32_BIOS_SIGN_ID
   rdmsr                         ; Get current microcode signature

   ; Verify this microcode update is not already loaded
   cmp   [esi + ucode_hdr.revision], edx
   je    .continue

.load_microcode:
   ; EAX contains the linear address of the start of the Update Data
   ; EDX contains zero
   ; ECX contains 79h (IA32_BIOS_UPDT_TRIG)
   ; Start microcode load with wrmsr
   mov   eax, esi
   add   eax, ucode_hdr.size
   xor   edx, edx
   mov   ecx, MSR_IA32_BIOS_UPDT_TRIG
   wrmsr
   mov   eax, 1
   cpuid

.continue:
   jmp   .next_microcode

.done:
   mov   eax, 1
   cpuid
   mov   ecx, MSR_IA32_BIOS_SIGN_ID
   rdmsr                         ; Get current microcode signature
   xor   eax, eax
   cmp   edx, 0
   jnz   .exit
   mov   eax, 0x8000000E

.exit:
   jmp   ebp

;----------------------------------------------------------------------------
; TempRamInit API
;
; This FSP API will load the microcode update, enable code caching for the
; region specified by the boot loader and also setup a temporary stack to be
; used till main memory is initialized.
;
;----------------------------------------------------------------------------
global ASM_PFX(TempRamInitApi)
ASM_PFX(TempRamInitApi):
  ;
  ; Ensure SSE is enabled
  ;
  ENABLE_SSE

  ;
  ; Save EBP, EBX, ESI, EDI & ESP in XMM7 & XMM6
  ;
  SAVE_REGS

  ;
  ; Save timestamp into XMM4 & XMM5
  ;
  rdtsc
  SAVE_EAX
  SAVE_EDX

  ;
  ; Check Parameter
  ;
  mov       eax, dword [esp + 4]
  cmp       eax, 0
  mov       eax, 0x80000002
  jz        NemInitExit

  ;
  ; CPUID/DeviceID check
  ;
  mov       eax, .3
  jmp       ASM_PFX(FspSelfCheck)  ; Note: ESP can not be changed.
.3:
  cmp       eax, 0
  jnz       NemInitExit

  ;
  ; Platform Basic Init.
  ;
  mov       eax, .4
  jmp       ASM_PFX(PlatformBasicInit)
.4:
  cmp       eax, 0
  jnz       NemInitExit

  ;
  ; Load microcode
  ;
  mov       eax, .5
  add       esp, 4
  jmp       ASM_PFX(LoadUcode)
.5:
  LOAD_ESP
  cmp       eax, 0
  jnz       NemInitExit

  ;
  ; Call platform NEM init
  ;
  mov       eax, .6
  add       esp, 4
  jmp       ASM_PFX(PlatformTempRamInit)
.6:
  LOAD_ESP
  cmp       eax, 0
  jnz       NemInitExit

  ;
  ; Save parameter pointer in edx
  ;
  mov       edx, dword [esp + 4]

  ;
  ; Enable FSP STACK
  ;
  mov       esp, PcdGet32(PcdTemporaryRamBase)
  add       esp, PcdGet32(PcdTemporaryRamSize)

  push      DATA_LEN_OF_MCUD     ; Size of the data region
  push      0x4455434D            ; Signature of the  data region 'MCUD'
  push      dword [edx +  4] ; Microcode size
  push      dword [edx +  0] ; Microcode base
  push      dword [edx + 12] ; Code size
  push      dword [edx + 8]  ; Code base

  ;
  ; Save API entry/exit timestamp into stack
  ;
  push      DATA_LEN_OF_PER0     ; Size of the data region
  push      0x30524550            ; Signature of the  data region 'PER0'
  rdtsc
  push      edx
  push      eax
  LOAD_EAX
  LOAD_EDX
  push      edx
  push      eax

  ;
  ; Terminator for the data on stack
  ;
  push      0

  ;
  ; Set ECX/EDX to the bootloader temporary memory range
  ;
  mov       ecx, PcdGet32(PcdTemporaryRamBase)
  mov       edx, ecx
  add       edx, PcdGet32(PcdTemporaryRamSize)
  sub       edx, PcdGet32(PcdFspTemporaryRamSize)

  xor       eax, eax

NemInitExit:
  ;
  ; Load EBP, EBX, ESI, EDI & ESP from XMM7 & XMM6
  ;
  LOAD_REGS
  ret

;----------------------------------------------------------------------------
; FspInit API
;
; This FSP API will perform the processor and chipset initialization.
; This API will not return.  Instead, it transfers the control to the
; ContinuationFunc provided in the parameter.
;
;----------------------------------------------------------------------------
global ASM_PFX(FspInitApi)
ASM_PFX(FspInitApi):
  ;
  ; Stack must be ready
  ;
  push   0x87654321
  pop    eax
  cmp    eax, 0x87654321
  jz     .7
  mov    eax, 0x80000003
  jmp    .exit

.7:
  ;
  ; Additional check
  ;
  pushad
  push   1
  call   ASM_PFX(FspApiCallingCheck)
  add    esp, 4
  mov    dword [esp + 4 * 7],  eax
  popad
  cmp    eax, 0
  jz     .8
  jmp    .exit

.8:
  ;
  ; Store the address in FSP which will return control to the BL
  ;
  push   dword .exit

  ;
  ; Create a Task Frame in the stack for the Boot Loader
  ;
  pushfd     ; 2 pushf for 4 byte alignment
  cli
  pushad

  ; Reserve 8 bytes for IDT save/restore
  sub     esp, 8
  sidt    [esp]

  ;
  ; Setup new FSP stack
  ;
  mov     eax, esp
  mov     esp, PcdGet32(PcdTemporaryRamBase)
  add     esp, PcdGet32(PcdTemporaryRamSize)
  sub     esp, (DATA_LEN_AT_STACK_TOP + 0x40)

  ;
  ; Save the bootloader's stack pointer
  ;
  push    eax

  ;
  ; Pass entry point of the PEI core
  ;
  call    ASM_PFX(GetFspBaseAddress)
  mov     edi, ASM_PFX(FspImageSizeOffset)
  mov     edi, DWORD [eax + edi]
  add     edi, eax
  sub     edi, 0x20
  add     eax, DWORD [edi]
  push    eax

  ;
  ; Pass BFV into the PEI Core
  ; It uses relative address to calucate the actual boot FV base
  ; For FSP impleantion with single FV, PcdFlashFvRecoveryBase and
  ; PcdFspAreaBaseAddress are the same. For FSP with mulitple FVs,
  ; they are different. The code below can handle both cases.
  ;
  call    ASM_PFX(GetFspBaseAddress)
  mov     edi, eax
  call    ASM_PFX(GetBootFirmwareVolumeOffset)
  add     eax, edi
  push    eax

  ;
  ; Pass stack base and size into the PEI Core
  ;
  mov     eax,  PcdGet32(PcdTemporaryRamBase)
  add     eax,  PcdGet32(PcdTemporaryRamSize)
  sub     eax,  PcdGet32(PcdFspTemporaryRamSize)
  push    eax
  push    PcdGet32(PcdFspTemporaryRamSize)

  ;
  ; Pass Control into the PEI Core
  ;
  call    ASM_PFX(SecStartup)

.exit:
  ret

;----------------------------------------------------------------------------
; NotifyPhase API
;
; This FSP API will notify the FSP about the different phases in the boot
; process
;
;----------------------------------------------------------------------------
global ASM_PFX(NotifyPhaseApi)
ASM_PFX(NotifyPhaseApi):
  ;
  ; Stack must be ready
  ;
  push   0x87654321
  pop    eax
  cmp    eax, 0x87654321
  jz     .9
  mov    eax, 0x80000003
  jmp    .err_exit

.9:
  ;
  ; Verify the calling condition
  ;
  pushad
  push   2
  call   ASM_PFX(FspApiCallingCheck)
  add    esp, 4
  mov    dword [esp + 4 * 7],  eax
  popad

  cmp    eax, 0
  jz     .10

  ;
  ; Error return
  ;
.err_exit:
  ret

.10:
  jmp    ASM_PFX(Pei2LoaderSwitchStack)

