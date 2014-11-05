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
;   Provide macro for register save/restore using SSE registers
;
;------------------------------------------------------------------------------

;
; Define SSE instruction set
;
%ifdef USE_SSE41_FLAG
;
; Define SSE macros using SSE 4.1 instructions
;
%macro SXMMN 3 ; XMM, IDX, REG
    pinsrd  %1, %3, (%2 & 3)
%endmacro

%macro LXMMN 3 ; XMM, REG, IDX
    pextrd  %2, %1, (%3 & 3)
%endmacro

%else

;
; Define SSE macros using SSE 2 instructions
;
%macro SXMMN 3 ; XMM, IDX, REG
    pinsrw  %1, %3, ((%2 & 3) * 2)
    ror     %3, 16
    pinsrw  %1, %3, (%2 & 3) * 2 + 1
    rol     %3, 16
%endmacro

%macro LXMMN 3 ; XMM, REG, IDX
    pshufd  %1, %1,  (0xE4E4E4 >> (%3 * 2))  & 0xFF
    movd    %2, %1
    pshufd  %1, %1,  (0xE4E4E4 >> (%3 * 2 + (%3 & 1) * 4)) & 0xFF
%endmacro

%endif


%macro SAVE_REGS 0
  SXMMN      xmm7, 0, ebp
  SXMMN      xmm7, 1, ebx
  SXMMN      xmm7, 2, esi
  SXMMN      xmm7, 3, edi
  SAVE_ESP
%endmacro

%macro LOAD_REGS 0
  LXMMN      xmm7, ebp, 0
  LXMMN      xmm7, ebx, 1
  LXMMN      xmm7, esi, 2
  LXMMN      xmm7, edi, 3
  LOAD_ESP
%endmacro

%macro LOAD_EAX 0
  LXMMN      xmm6, eax, 1
%endmacro

%macro SAVE_EAX 0
  SXMMN      xmm6, 1, eax
%endmacro

%macro LOAD_EDX 0
  LXMMN      xmm6, edx, 2
%endmacro

%macro SAVE_EDX 0
  SXMMN      xmm6, 2, edx
%endmacro

%macro SAVE_ECX 0
  SXMMN      xmm6, 3, ecx
%endmacro

%macro LOAD_ECX 0
  LXMMN      xmm6, ecx, 3
%endmacro

%macro SAVE_ESP 0
  SXMMN      xmm6, 0, esp
%endmacro

%macro LOAD_ESP 0
  movd       esp,  xmm6
%endmacro

%macro ENABLE_SSE 0
  mov        eax, cr4
  or         eax, 0x00000600
  mov        cr4, eax
%endmacro
