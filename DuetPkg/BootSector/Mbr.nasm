;------------------------------------------------------------------------------
;*
;*   Copyright (c) 2006 - 2007, Intel Corporation. All rights reserved.<BR>
;*   This program and the accompanying materials
;*   are licensed and made available under the terms and conditions of the BSD License
;*   which accompanies this distribution.  The full text of the license may be found at
;*   http://opensource.org/licenses/bsd-license.php
;*
;*   THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
;*   WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;*
;*    Mbr.nasm
;*
;*   Abstract:
;*
;------------------------------------------------------------------------------

;   .dosseg
    .stack
    SECTION .text

%define BLOCK_SIZE 0x200
%define BLOCK_MASK 0x1ff
%define BLOCK_SHIFT 9

; ****************************************************************************
; Code loaded by BIOS at 0x0000:0x7C00
; ****************************************************************************

        org 0x0
Start:

; ****************************************************************************
; Start Print
; ****************************************************************************

        mov  ax,0xb800
        mov  es,ax
        mov  ax, 0x7c0
        mov  ds, ax
        lea  si, cs:[StartString]
        mov  cx, 10
        mov  di, 160
        rep  movsw

; ****************************************************************************
; Print over
; ****************************************************************************

; ****************************************************************************
; Initialize segment registers and copy code at 0x0000:0x7c00 to 0x0000:0x0600
; ****************************************************************************
        xor   ax, ax                              ; AX = 0x0000
        mov   bx, 0x7c00                          ; BX = 0x7C00
        mov   bp, 0x600                           ; BP = 0x0600
        mov   si, OFFSET RelocatedStart           ; SI = Offset(RelocatedStart)
        mov   cx, 0x200                           ; CX = 0x0200
        sub   cx, si                              ; CS = 0x0200 - Offset(RelocatedStart)
        lea   di, [bp+si]                         ; DI = 0x0600 + Offset(RelocatedStart)
        lea   si, [bx+si]                         ; BX = 0x7C00 + Offset(RelocatedStart)
        mov   ss, ax                              ; SS = 0x0000
        mov   sp, bx                              ; SP = 0x7C00
        mov   es,ax                               ; ES = 0x0000
        mov   ds,ax                               ; DS = 0x0000
        push  ax                                  ; PUSH 0x0000
        push  di                                  ; PUSH 0x0600 + Offset(RelocatedStart)
        cld                                       ; Clear the direction flag
        rep   movsb                               ; Copy 0x0200 bytes from 0x7C00 to 0x0600
        retf                                      ; JMP 0x0000:0x0600 + Offset(RelocatedStart)

; ****************************************************************************
; Code relocated to 0x0000:0x0600
; ****************************************************************************

RelocatedStart:
; ****************************************************************************
; Get Driver Parameters to 0x0000:0x7BFC
; ****************************************************************************

        xor   ax,ax                               ; AX = 0
        mov   ss,ax                               ; SS = 0
        add   ax,0x1000
        mov   ds,ax

        mov   sp,0x7c00                           ; SP = 0x7c00
        mov   bp,sp                               ; BP = 0x7c00

        mov   ah,8                                ; AH = 8 - Get Drive Parameters Function
        mov   byte [bp+PhysicalDrive],dl      ; BBS defines that BIOS would pass the booting driver number to the loader through DL
        int   0x13                                 ; Get Drive Parameters
        xor   ax,ax                               ; AX = 0
        mov   al,dh                               ; AL = DH
        inc   al                                  ; MaxHead = AL + 1
        push  ax                                  ; 0000:7bfe = MaxHead
        mov   al,cl                               ; AL = CL
        and   al,0x3f                             ; MaxSector = AL & 0x3f
        push  ax                                  ; 0000:7bfc = MaxSector

; ****************************************************************************
; Read Target DBR from hard disk to 0x0000:0x7C00
; ****************************************************************************

        xor   ax, ax
        mov   al, byte [bp+MbrPartitionIndicator]  ; AX = MbrPartitionIndex
        cmp   al, 0xff                                 ; 0xFF means do legacy MBR boot
        jnz   EfiDbr
LegacyMbr:
        mov   eax, 0x600                      ; Assume LegacyMBR is backuped in Sector 6
        jmp   StartReadTo7C00                     ; EAX = Header/Sector/Tracker/Zero

EfiDbr:
        cmp   al, 4                               ; MbrPartitionIndex should < 4
        jae   BadDbr
        shl   ax, 4                               ; AX  = MBREntrySize * Index
        add   ax, 0x1be                            ; AX  = MBREntryOffset
        mov   di, ax                              ; DI  = MBREntryOffset

        ; Here we don't use the C/H/S information provided by Partition table
        ;  but calculate C/H/S from LBA ourselves
        ;       Ci: Cylinder number
        ;       Hi: Header number
        ;       Si: Sector number
        mov   eax, dword es:[bp + di + 8]     ; Start LBA
        mov   edx, eax
        shr   edx, 16                             ; DX:AX = Start LBA
                                                  ;       = Ci * (H * S) + Hi * S + (Si - 1)

        ; Calculate C/H/S according to LBA
        mov   bp, 0x7bfa
        div   word [bp+2]                     ; AX = Hi + H*Ci
                                                  ; DX = Si - 1
        inc   dx                                  ; DX = Si
        push  dx                                  ; 0000:7bfa = Si  <----
        xor   dx, dx                              ; DX:AX = Hi + H*Ci
        div   word [bp+4]                     ; AX = Ci         <----
                                                  ; DX = Hi         <----

StartReadTo7C00:

        mov   cl, byte [bp]                   ; Si
        mov   ch, al                              ; Ci[0-7]
        or    cl, ah                              ; Ci[8,9]
        mov   bx, 0x7c00                           ; ES:BX = 0000:7C00h
        mov   ah, 0x2                              ; Function 02h
        mov   al, 1                               ; 1 Sector
        mov   dh, dl                              ; Hi
        mov   bp, 0x600
        mov   dl, byte [bp + PhysicalDrive]   ; Drive number
        int   0x13
        jc    BadDbr

; ****************************************************************************
; Transfer control to BootSector - Jump to 0x0000:0x7C00
; ****************************************************************************
        xor   ax, ax
        push  ax                                  ; PUSH 0x0000 - Segment
        mov   di, 0x7c00
        push  di                                  ; PUSH 0x7C00 - Offset
        retf                                      ; JMP 0x0000:0x7C00

; ****************************************************************************
; ERROR Condition:
; ****************************************************************************

BadDbr:
    push ax
    mov  ax, 0xb800
    mov  es, ax
    mov  ax, 0x60
    mov  ds, ax
    lea  si, cs:[ErrorString]
    mov  di, 320
    pop  ax
    call A2C
    mov  [si+16], ah
    mov  [si+18], al
    mov  cx, 10
    rep  movsw
Halt:
    jmp   Halt

StartString:
    db 'M', 0xc, 'B', 0xc, 'R', 0xc, ' ', 0xc, 'S', 0xc, 't', 0xc, 'a', 0xc, 'r', 0xc, 't', 0xc, '!', 0xc
ErrorString:
    db 'M', 0xc, 'B', 0xc, 'R', 0xc, ' ', 0xc, 'E', 0xc, 'r', 0xc, 'r', 0xc, ':', 0xc, '?', 0xc, '?', 0xc

; ****************************************************************************
; A2C - convert Ascii code stored in AH to character stored in AX
; ****************************************************************************
A2C:
    mov  al, ah
    shr  ah, 4
    and  al, 0xF
    add  ah, '0'
    add  al, '0'

    cmp  ah, '9'
    jle  .0
    add  ah, 7
.0:

    cmp al, '9'
    jle .1
    add al, 7
.1:
    ret

; ****************************************************************************
; PhysicalDrive - Used to indicate which disk to be boot
;                 Can be patched by tool
; ****************************************************************************
    org   0x1B6
PhysicalDrive: db 0x80

; ****************************************************************************
; MbrPartitionIndicator - Used to indicate which MBR partition to be boot
;                         Can be patched by tool
;                         OxFF means boot to legacy MBR. (LBA OFFSET 6)
; ****************************************************************************
    org   0x1B7
MbrPartitionIndicator: db 0

; ****************************************************************************
; Unique MBR signature
; ****************************************************************************
    org   0x1B8
    db 'DUET'

; ****************************************************************************
; Unknown
; ****************************************************************************
    org   0x1BC
    dw 0

; ****************************************************************************
; MBR Entry - To be patched
; ****************************************************************************
    org   0x1BE
    dd 0, 0, 0, 0
    org   0x1CE
    dd 0, 0, 0, 0
    org   0x1DE
    dd 0, 0, 0, 0
    org   0x1EE
    dd 0, 0, 0, 0

; ****************************************************************************
; Sector Signature
; ****************************************************************************

  org 0x1FE
SectorSignature:
  dw        0xaa55      ; Boot Sector Signature

