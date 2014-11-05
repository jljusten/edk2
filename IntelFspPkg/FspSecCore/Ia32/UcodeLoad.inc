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
;------------------------------------------------------------------------------

MSR_IA32_PLATFORM_ID        EQU     0x000000017
MSR_IA32_BIOS_UPDT_TRIG     EQU     0x000000079
MSR_IA32_BIOS_SIGN_ID       EQU     0x00000008b

struc ucode
    .version:                 resd     1
    .revision:                resd     1
    .date:                    resd     1
    .processor:               resd     1
    .checksum:                resd     1
    .loader:                  resd     1
    .rsvd:                    resd     6t
    .data:                    resd     500t
endstruc
;ucode_t                     TYPEDEF     ucode

struc ucode_hdr
    .version:                 resd     1
    .revision:                resd     1
    .date:                    resd     1
    .processor:               resd     1
    .checksum:                resd     1
    .loader:                  resd     1
    .flags:                   resd     1
    .data_size:               resd     1
    .total_size:              resd     1
    .rsvd:                    resd     3t
    .size:
endstruc
;ucode_hdr_t                 TYPEDEF     ucode_hdr

struc ext_sig_hdr
    .count:                   resd     1
    .checksum:                resd     1
    .rsvd:                    resd     3t
    .size:
endstruc
;ext_sig_hdr_t               TYPEDEF     ext_sig_hdr

struc ext_sig
    .processor:               resd     1
    .flags:                   resd     1
    .checksum:                resd     1
    .size:
endstruc
;ext_sig_t                   TYPEDEF     ext_sig

struc LOAD_UCODE_PARAMS
    .ucode_code_addr:         resd     1
    .ucode_code_size:         resd     1
endstruc
