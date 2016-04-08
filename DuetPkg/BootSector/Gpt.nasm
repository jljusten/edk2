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
;*    gpt.asm
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
        xor   ax, ax                    ; AX = 0x0000
        mov   bx, 0x7c00                ; BX = 0x7C00
        mov   bp, 0x600                 ; BP = 0x0600
        mov   si, OFFSET RelocatedStart ; SI = Offset(RelocatedStart)
        mov   cx, 0x200                 ; CX = 0x0200
        sub   cx, si                    ; CS = 0x0200 - Offset(RelocatedStart)
        lea   di, [bp+si]               ; DI = 0x0600 + Offset(RelocatedStart)
        lea   si, [bx+si]               ; BX = 0x7C00 + Offset(RelocatedStart)
        mov   ss, ax                    ; SS = 0x0000
        mov   sp, bx                    ; SP = 0x7C00
        mov   es,ax                     ; ES = 0x0000
        mov   ds,ax                     ; DS = 0x0000
        push  ax                        ; PUSH 0x0000
        push  di                        ; PUSH 0x0600 + Offset(RelocatedStart)
        cld                             ; Clear the direction flag
        rep   movsb                     ; Copy 0x0200 bytes from 0x7C00 to 0x0600
        retf                            ; JMP 0x0000:0x0600 + Offset(RelocatedStart)

; ****************************************************************************
; Code relocated to 0x0000:0x0600
; ****************************************************************************

RelocatedStart:
; ****************************************************************************
; Get Driver Parameters to 0x0000:0x7BFC
; ****************************************************************************
        xor   ax,ax         ; ax = 0
        mov   ss,ax         ; ss = 0
        add   ax,0x1000
        mov   ds,ax

        mov   sp,0x7c00     ; sp = 0x7c00
        mov   bp,sp         ; bp = 0x7c00

        mov   ah,8                                ; ah = 8 - Get Drive Parameters Function
        mov   byte [bp+PhysicalDrive],dl      ; BBS defines that BIOS would pass the booting driver number to the loader through DL
        int   0x13                                 ; Get Drive Parameters
        xor   ax,ax                   ; ax = 0
        mov   al,dh                   ; al = dh
        inc   al                      ; MaxHead = al + 1
        push  ax                      ; 0000:7bfe = MaxHead
        mov   al,cl                   ; al = cl
        and   al,0x3f                 ; MaxSector = al & 0x3f
        push  ax                      ; 0000:7bfc = MaxSector

; ****************************************************************************
; Read GPT Header from hard disk to 0x0000:0x0800
; ****************************************************************************
        xor     ax, ax
        mov     es, ax                            ; Read to 0x0000:0x0800
        mov     di, 0x800                         ; Read to 0x0000:0x0800
        mov     eax, 1                            ; Read LBA #1
        mov     edx, 0                            ; Read LBA #1
        mov     bx, 1                             ; Read 1 Block
        push    es
        call    ReadBlocks
        pop     es

; ****************************************************************************
; Read Target GPT Entry from hard disk to 0x0000:0x0A00
; ****************************************************************************
        cmp   dword es:[di], 0x20494645       ; Check for "EFI "
        jne   BadGpt
        cmp   dword es:[di + 4], 0x54524150   ; Check for "PART"
        jne   BadGpt
        cmp   dword es:[di + 8], 0x10000   ; Check Revision - 0x10000
        jne   BadGpt

        mov   eax, dword es:[di + 84]         ; EAX = SizeOfPartitionEntry
        mul   byte [bp+GptPartitionIndicator] ; EAX = SizeOfPartitionEntry * GptPartitionIndicator
        mov   edx, eax                            ; EDX = SizeOfPartitionEntry * GptPartitionIndicator
        shr   eax, BLOCK_SHIFT                    ; EAX = (SizeOfPartitionEntry * GptPartitionIndicator) / BLOCK_SIZE
        and   edx, BLOCK_MASK                     ; EDX = Targer PartitionEntryLBA Offset
                                                  ;     = (SizeOfPartitionEntry * GptPartitionIndicator) % BLOCK_SIZE
        push  edx
        mov   ecx, dword es:[di + 72]         ; ECX = PartitionEntryLBA (Low)
        mov   ebx, dword es:[di + 76]         ; EBX = PartitionEntryLBA (High)
        add   eax, ecx                            ; EAX = Target PartitionEntryLBA (Low)
                                                  ;     = (PartitionEntryLBA +
                                                  ;        (SizeOfPartitionEntry * GptPartitionIndicator) / BLOCK_SIZE)
        adc   edx, ebx                            ; EDX = Target PartitionEntryLBA (High)

        mov   di, 0xA00                           ; Read to 0x0000:0x0A00
        mov   bx, 1                               ; Read 1 Block
        push  es
        call  ReadBlocks
        pop   es

; ****************************************************************************
; Read Target DBR from hard disk to 0x0000:0x7C00
; ****************************************************************************
        pop   edx                                 ; EDX = (SizeOfPartitionEntry * GptPartitionIndicator) % BLOCK_SIZE
        add   di, dx                              ; DI = Targer PartitionEntryLBA Offset
        cmp   dword es:[di], 0xC12A7328       ; Check for EFI System Partition "C12A7328-F81F-11d2-BA4B-00A0C93EC93B"
        jne   BadGpt
        cmp   dword es:[di + 4], 0x11d2F81F   ;
        jne   BadGpt
        cmp   dword es:[di + 8], 0xA0004BBA   ;
        jne   BadGpt
        cmp   dword es:[di + 0xc], 0x3BC93EC9 ;
        jne   BadGpt

        mov   eax, dword es:[di + 32]         ; EAX = StartingLBA (Low)
        mov   edx, dword es:[di + 36]         ; EDX = StartingLBA (High)
        mov   di, 0x7C00                          ; Read to 0x0000:0x7C00
        mov   bx, 1                               ; Read 1 Block
        call  ReadBlocks

; ****************************************************************************
; Transfer control to BootSector - Jump to 0x0000:0x7C00
; ****************************************************************************
        xor   ax, ax
        push  ax                        ; PUSH 0x0000
        mov   di, 0x7c00
        push  di                        ; PUSH 0x7C00
        retf                            ; JMP 0x0000:0x7C00

; ****************************************************************************
; ReadBlocks - Reads a set of blocks from a block device
;
; EDX:EAX = Start LBA
; BX      = Number of Blocks to Read (must < 127)
; ES:DI   = Buffer to store sectors read from disk
; ****************************************************************************

; si = DiskAddressPacket

ReadBlocks:
        pushad
        push  ds
        xor   cx, cx
        mov   ds, cx
        mov   bp, 0x600                         ; bp = 0x600
        lea   si, [bp + OFFSET AddressPacket]   ; DS:SI = Disk Address Packet
        mov   BYTE ds:[si+2],bl             ;    02 = Number Of Block transfered
        mov   WORD ds:[si+4],di             ;    04 = Transfer Buffer Offset
        mov   WORD ds:[si+6],es             ;    06 = Transfer Buffer Segment
        mov   DWORD ds:[si+8],eax           ;    08 = Starting LBA (Low)
        mov   DWORD ds:[si+0xc],edx         ;    0C = Starting LBA (High)
        mov   ah, 0x42                           ; ah = Function 42
        mov   dl,byte ptr [bp+PhysicalDrive]    ; dl = Drive Number
        int   0x13
        jc    BadGpt
        pop   ds
        popad
        ret

; ****************************************************************************
; Address Packet used by ReadBlocks
; ****************************************************************************
AddressPacket:
        db    0x10                       ; Size of address packet
        db    0x0                       ; Reserved.  Must be 0
        db    0x1                       ; Read blocks at a time (To be fixed each times)
        db    0x0                       ; Reserved.  Must be 0
        dw    0x0                     ; Destination Address offset (To be fixed each times)
        dw    0x0                     ; Destination Address segment (To be fixed each times)
AddressPacketLba:
        dd    0x0, 0x0                    ; Start LBA (To be fixed each times)
AddressPacketEnd:

; ****************************************************************************
; ERROR Condition:
; ****************************************************************************

BadGpt:
    mov  ax,0xb800
    mov  es,ax
    mov  ax, 0x60
    mov  ds, ax
    lea  si, cs:[ErrorString]
    mov  cx, 10
    mov  di, 320
    rep  movsw
Halt:
    jmp   Halt

StartString:
    db 'G', 0xc, 'P', 0xc, 'T', 0xc, ' ', 0xc, 'S', 0xc, 't', 0xc, 'a', 0xc, 'r', 0xc, 't', 0xc, '!', 0xc
ErrorString:
    db 'G', 0xc, 'P', 0xc, 'T', 0xc, ' ', 0xc, 'E', 0xc, 'r', 0xc, 'r', 0xc, 'o', 0xc, 'r', 0xc, '!', 0xc

; ****************************************************************************
; PhysicalDrive - Used to indicate which disk to be boot
;                 Can be patched by tool
; ****************************************************************************
    org   0x1B6
PhysicalDrive: db 0x80

; ****************************************************************************
; GptPartitionIndicator - Used to indicate which GPT partition to be boot
;                         Can be patched by tool
; ****************************************************************************
    org   0x1B7
GptPartitionIndicator: db 0

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
; PMBR Entry - Can be patched by tool
; ****************************************************************************
    org   0x1BE
    db 0          ; Boot Indicator
    db 0xff       ; Start Header
    db 0xff       ; Start Sector
    db 0xff       ; Start Track
    db 0xee       ; OS Type
    db 0xff       ; End Header
    db 0xff       ; End Sector
    db 0xff       ; End Track
    dd 1          ; Starting LBA
    dd 0xFFFFFFFF ; End LBA

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

