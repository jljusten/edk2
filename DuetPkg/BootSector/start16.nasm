;------------------------------------------------------------------------------
;*
;*   Copyright (c) 2006 - 2011, Intel Corporation. All rights reserved.<BR>
;*   This program and the accompanying materials
;*   are licensed and made available under the terms and conditions of the BSD License
;*   which accompanies this distribution.  The full text of the license may be found at
;*   http://opensource.org/licenses/bsd-license.php
;*
;*   THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
;*   WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;*
;*    start16.nasm
;*
;*   Abstract:
;*
;------------------------------------------------------------------------------

        .stack
        SECTION .text

%define FAT_DIRECTORY_ENTRY_SIZE 0x20
%define FAT_DIRECTORY_ENTRY_SHIFT 5
%define BLOCK_SIZE 0x200
%define BLOCK_MASK 0x1ff
%define BLOCK_SHIFT 9

        org 0x0
Ia32Jump:
  jmp   BootSectorEntryPoint  ; JMP inst    - 3 bytes
  nop

OemId: db "INTEL   "    ; OemId               - 8 bytes

SectorSize: dw 0             ; Sector Size         - 16 bits
SectorsPerCluster: db 0             ; Sector Per Cluster  - 8 bits
ReservedSectors: dw 0             ; Reserved Sectors    - 16 bits
NoFats: db 0             ; Number of FATs      - 8 bits
RootEntries: dw 0             ; Root Entries        - 16 bits
Sectors: dw 0             ; Number of Sectors   - 16 bits
Media: db 0             ; Media               - 8 bits  - ignored
SectorsPerFat: dw 0             ; Sectors Per FAT     - 16 bits
SectorsPerTrack: dw 0             ; Sectors Per Track   - 16 bits - ignored
Heads: dw 0             ; Heads               - 16 bits - ignored
HiddenSectors: dd 0             ; Hidden Sectors      - 32 bits - ignored
LargeSectors: dd 0             ; Large Sectors       - 32 bits
PhysicalDrive: db 0             ; PhysicalDriveNumber - 8 bits  - ignored
CurrentHead: db 0             ; Current Head        - 8 bits
Signature: db 0             ; Signature           - 8 bits  - ignored
VolId: db "    "        ; Volume Serial Number- 4 bytes
FatLabel: db "           " ; Label               - 11 bytes
SystemId: db "FAT16   "    ; SystemId            - 8 bytes

BootSectorEntryPoint:
        ASSUME  ds:@code
        ASSUME  ss:@code
      ; ds = 1000, es = 2000 + x (size of first cluster >> 4)
      ; cx = Start Cluster of EfiLdr
      ; dx = Start Cluster of Efivar.bin

; Re use the BPB data stored in Boot Sector
        mov     bp,0x7c00

        push    cx
; Read Efivar.bin
;       1000:dx    = DirectoryEntry of Efivar.bin -> BS.com has filled already
        mov     ax,0x1900
        mov     es,ax
        test    dx,dx
        jnz     CheckVarStoreSize

        mov     al,1
NoVarStore:
        push    es
; Set the 5th byte start @ 0:19000 to non-zero indicating we should init var store header in DxeIpl
        mov     byte es:[4],al
        jmp     SaveVolumeId

CheckVarStoreSize:
        mov     di,dx
        cmp     dword ds:[di+2], 0x4000
        mov     al,2
        jne     NoVarStore

LoadVarStore:
        mov     al,0
        mov     byte es:[4],al
        mov     cx,word ptr[di]
;       ES:DI = 1500:0
        xor     di,di
        push    es
        mov     ax,0x1500
        mov     es,ax
        call    ReadFile
SaveVolumeId:
        pop     es
        mov     ax,word ptr [bp+VolId]
        mov     word es:[0],ax                  ; Save Volume Id to 0:19000. we will find the correct volume according to this VolumeId
        mov     ax,word ptr [bp+VolId+2]
        mov     word es:[2],ax

; Read Efildr
        pop     cx
;       cx    = Start Cluster of Efildr -> BS.com has filled already
;       ES:DI = 2000:0, first cluster will be read again
        xor     di,di                               ; di = 0
        mov     ax,0x2000
        mov     es,ax
        call    ReadFile
        mov     ax,cs
        mov     word cs:[JumpSegment],ax

JumpFarInstruction:
        db      0xea
JumpOffset:
        dw      0x200
JumpSegment:
        dw      0x2000

; ****************************************************************************
; ReadFile
;
; Arguments:
;   CX    = Start Cluster of File
;   ES:DI = Buffer to store file content read from disk
;
; Return:
;   (ES << 4 + DI) = end of file content Buffer
;
; ****************************************************************************
ReadFile:
; si      = NumberOfClusters
; cx      = ClusterNumber
; dx      = CachedFatSectorNumber
; ds:0000 = CacheFatSectorBuffer
; es:di   = Buffer to load file
; bx      = NextClusterNumber
        pusha
        mov     si,1                                ; NumberOfClusters = 1
        push    cx                                  ; Push Start Cluster onto stack
        mov     dx,0xfff                            ; CachedFatSectorNumber = 0xfff
FatChainLoop:
        mov     ax,cx                               ; ax = ClusterNumber
        and     ax,0xfff8                           ; ax = ax & 0xfff8
        cmp     ax,0xfff8                           ; See if this is the last cluster
        je      FoundLastCluster                    ; Jump if last cluster found
        mov     ax,cx                               ; ax = ClusterNumber
        shl     ax,1                                ; FatOffset = ClusterNumber * 2
        push    si                                  ; Save si
        mov     si,ax                               ; si = FatOffset
        shr     ax,BLOCK_SHIFT                      ; ax = FatOffset >> BLOCK_SHIFT
        add     ax,word ptr [bp+ReservedSectors]    ; ax = FatSectorNumber = ReservedSectors + (FatOffset >> BLOCK_OFFSET)
        and     si,BLOCK_MASK                       ; si = FatOffset & BLOCK_MASK
        cmp     ax,dx                               ; Compare FatSectorNumber to CachedFatSectorNumber
        je      SkipFatRead
        mov     bx,2
        push    es
        push    ds
        pop     es
        call    ReadBlocks                          ; Read 2 blocks starting at AX storing at ES:DI
        pop     es
        mov     dx,ax                               ; CachedFatSectorNumber = FatSectorNumber
SkipFatRead:
        mov     bx,word ptr [si]                    ; bx = NextClusterNumber
        mov     ax,cx                               ; ax = ClusterNumber
        pop     si                                  ; Restore si
        dec     bx                                  ; bx = NextClusterNumber - 1
        cmp     bx,cx                               ; See if (NextClusterNumber-1)==ClusterNumber
        jne     ReadClusters
        inc     bx                                  ; bx = NextClusterNumber
        inc     si                                  ; NumberOfClusters++
        mov     cx,bx                               ; ClusterNumber = NextClusterNumber
        jmp     FatChainLoop
ReadClusters:
        inc     bx
        pop     ax                                  ; ax = StartCluster
        push    bx                                  ; StartCluster = NextClusterNumber
        mov     cx,bx                               ; ClusterNumber = NextClusterNumber
        sub     ax,2                                ; ax = StartCluster - 2
        xor     bh,bh
        mov     bl,byte ptr [bp+SectorsPerCluster]  ; bx = SectorsPerCluster
        mul     bx                                  ; ax = (StartCluster - 2) * SectorsPerCluster
        add     ax, word [bp]                   ; ax = FirstClusterLBA + (StartCluster-2)*SectorsPerCluster
        push    ax                                  ; save start sector
        mov     ax,si                               ; ax = NumberOfClusters
        mul     bx                                  ; ax = NumberOfClusters * SectorsPerCluster
        mov     bx,ax                               ; bx = Number of Sectors
        pop     ax                                  ; ax = Start Sector
        call    ReadBlocks
        mov     si,1                                ; NumberOfClusters = 1
        jmp     FatChainLoop
FoundLastCluster:
        pop     cx
        popa
        ret

; ****************************************************************************
; ReadBlocks - Reads a set of blocks from a block device
;
; AX    = Start LBA
; BX    = Number of Blocks to Read
; ES:DI = Buffer to store sectors read from disk
; ****************************************************************************

; cx = Blocks
; bx = NumberOfBlocks
; si = StartLBA

ReadBlocks:
        pusha
        add     eax,dword ptr [bp+LBAOffsetForBootSector]    ; Add LBAOffsetForBootSector to Start LBA
        add     eax,dword ptr [bp+HiddenSectors]    ; Add HiddenSectors to Start LBA
        mov     esi,eax                             ; esi = Start LBA
        mov     cx,bx                               ; cx = Number of blocks to read
ReadCylinderLoop:
        mov     bp,0x7bfc                           ; bp = 0x7bfc
        mov     eax,esi                             ; eax = Start LBA
        xor     edx,edx                             ; edx = 0
        movzx   ebx,word ptr [bp]                   ; bx = MaxSector
        div     ebx                                 ; ax = StartLBA / MaxSector
        inc     dx                                  ; dx = (StartLBA % MaxSector) + 1

        mov     bx,word ptr [bp]                    ; bx = MaxSector
        sub     bx,dx                               ; bx = MaxSector - Sector
        inc     bx                                  ; bx = MaxSector - Sector + 1
        cmp     cx,bx                               ; Compare (Blocks) to (MaxSector - Sector + 1)
        jg      LimitTransfer
        mov     bx,cx                               ; bx = Blocks
LimitTransfer:
        push    ax                                  ; save ax
        mov     ax,es                               ; ax = es
        shr     ax,(BLOCK_SHIFT-4)                  ; ax = Number of blocks into mem system
        and     ax,0x7f                             ; ax = Number of blocks into current seg
        add     ax,bx                               ; ax = End Block number of transfer
        cmp     ax,0x80                             ; See if it crosses a 64K boundry
        jle     NotCrossing64KBoundry               ; Branch if not crossing 64K boundry
        sub     ax,0x80                             ; ax = Number of blocks past 64K boundry
        sub     bx,ax                               ; Decrease transfer size by block overage
NotCrossing64KBoundry:
        pop     ax                                  ; restore ax

        push    cx
        mov     cl,dl                               ; cl = (StartLBA % MaxSector) + 1 = Sector
        xor     dx,dx                               ; dx = 0
        div     word [bp+2]                     ; ax = ax / (MaxHead + 1) = Cylinder
                                                    ; dx = ax % (MaxHead + 1) = Head

        push    bx                                  ; Save number of blocks to transfer
        mov     dh,dl                               ; dh = Head
        mov     bp,0x7c00                           ; bp = 0x7c00
        mov     dl,byte ptr [bp+PhysicalDrive]      ; dl = Drive Number
        mov     ch,al                               ; ch = Cylinder
        mov     al,bl                               ; al = Blocks
        mov     ah,2                                ; ah = Function 2
        mov     bx,di                               ; es:bx = Buffer address
        int     0x13
        jc      DiskError
        pop     bx
        pop     cx
        movzx   ebx,bx
        add     esi,ebx                             ; StartLBA = StartLBA + NumberOfBlocks
        sub     cx,bx                               ; Blocks = Blocks - NumberOfBlocks
        mov     ax,es
        shl     bx,(BLOCK_SHIFT-4)
        add     ax,bx
        mov     es,ax                               ; es:di = es:di + NumberOfBlocks*BLOCK_SIZE
        cmp     cx,0
        jne     ReadCylinderLoop
        popa
        ret

DiskError:
        push cs
        pop  ds
        lea  si, [ErrorString]
        mov  cx, 7
        jmp  PrintStringAndHalt

PrintStringAndHalt:
        mov  ax,0xb800
        mov  es,ax
        mov  di,160
        rep  movsw
Halt:
        jmp   Halt

ErrorString:
        db 'S', 0xc, 'E', 0xc, 'r', 0xc, 'r', 0xc, 'o', 0xc, 'r', 0xc, '!', 0xc

        org     0x1fa
LBAOffsetForBootSector:
        dd      0x0

        org     0x1fe
        dw      0xaa55

;******************************************************************************
;******************************************************************************
;******************************************************************************

%define DELAY_PORT 0xed    ; Port to use for 1uS delay
%define KBD_CONTROL_PORT 0x60    ; 8042 control port
%define KBD_STATUS_PORT 0x64    ; 8042 status port
%define WRITE_DATA_PORT_CMD 0xd1    ; 8042 command to write the data port
%define ENABLE_A20_CMD 0xdf    ; 8042 command to enable A20

        org     0x200
        jmp start
Em64String:
        db 'E', 0xc, 'm', 0xc, '6', 0xc, '4', 0xc, 'T', 0xc, ' ', 0xc, 'U', 0xc, 'n', 0xc, 's', 0xc, 'u', 0xc, 'p', 0xc, 'p', 0xc, 'o', 0xc, 'r', 0xc, 't', 0xc, 'e', 0xc, 'd', 0xc, '!', 0xc

start:
        mov ax,cs
        mov ds,ax
        mov es,ax
        mov ss,ax
        mov sp,MyStack

;        mov ax,0b800h
;        mov es,ax
;        mov byte ptr es:[160],'a'
;        mov ax,cs
;        mov es,ax

        mov ebx,0
        lea edi, [MemoryMap]
MemMapLoop:
        mov eax,0xe820
        mov ecx,20
        mov edx,'SMAP'
        int 0x15
        jc  MemMapDone
        add edi,20
        cmp ebx,0
        je  MemMapDone
        jmp MemMapLoop
MemMapDone:
        lea eax, [MemoryMap]
        sub edi,eax                         ; Get the address of the memory map
        mov dword [MemoryMapSize],edi   ; Save the size of the memory map

        xor     ebx,ebx
        mov     bx,cs                       ; BX=segment
        shl     ebx,4                       ; BX="linear" address of segment base
        lea     eax, [GDT_BASE + ebx]        ; EAX=PHYSICAL address of gdt
        mov     dword [gdtr + 2],eax    ; Put address of gdt into the gdtr
        lea     eax, [IDT_BASE + ebx]        ; EAX=PHYSICAL address of idt
        mov     dword [idtr + 2],eax    ; Put address of idt into the idtr
        lea     edx, [MemoryMapSize + ebx]   ; Physical base address of the memory map

        add ebx,0x1000                      ; Source of EFI32
        mov dword [JUMP+2],ebx
        add ebx,0x1000
        mov esi,ebx                         ; Source of EFILDR32

;        mov ax,0b800h
;        mov es,ax
;        mov byte ptr es:[162],'b'
;        mov ax,cs
;        mov es,ax

;
; Enable A20 Gate
;

        mov ax,0x2401                        ; Enable A20 Gate
        int 0x15
        jnc A20GateEnabled                  ; Jump if it suceeded

;
; If INT 15 Function 2401 is not supported, then attempt to Enable A20 manually.
;

        call    Empty8042InputBuffer        ; Empty the Input Buffer on the 8042 controller
        jnz     Timeout8042                 ; Jump if the 8042 timed out
        out     DELAY_PORT,ax               ; Delay 1 uS
        mov     al,WRITE_DATA_PORT_CMD      ; 8042 cmd to write output port
        out     KBD_STATUS_PORT,al          ; Send command to the 8042
        call    Empty8042InputBuffer        ; Empty the Input Buffer on the 8042 controller
        jnz     Timeout8042                 ; Jump if the 8042 timed out
        mov     al,ENABLE_A20_CMD           ; gate address bit 20 on
        out     KBD_CONTROL_PORT,al         ; Send command to thre 8042
        call    Empty8042InputBuffer        ; Empty the Input Buffer on the 8042 controller
        mov     cx,25                       ; Delay 25 uS for the command to complete on the 8042
Delay25uS:
        out     DELAY_PORT,ax               ; Delay 1 uS
        loop    Delay25uS
Timeout8042:

A20GateEnabled:
        mov     bx,0x8                    ; Flat data descriptor
;
; DISABLE INTERRUPTS - Entering Protected Mode
;

        cli

;        mov ax,0b800h
;        mov es,ax
;        mov byte ptr es:[164],'c'
;        mov ax,cs
;        mov es,ax

        db      0x66
        lgdt    [gdtr]
        db      0x66
        lidt    [idtr]

        mov     eax,cr0
        or      al,1
        mov     cr0,eax
JUMP:
; jmp far 0010:00020000
        db  0x66
        db  0xea
        dd  0x20000
        dw  0x10

Empty8042InputBuffer:
        mov cx,0
Empty8042Loop:
        out     DELAY_PORT,ax               ; Delay 1us
        in      al,KBD_STATUS_PORT          ; Read the 8042 Status Port
        and     al,0x2                      ; Check the Input Buffer Full Flag
        loopnz  Empty8042Loop               ; Loop until the input buffer is empty or a timout of 65536 uS
        ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; data
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        align 0x2

gdtr: dw GDT_END - GDT_BASE - 1   ; GDT limit
        dd 0                        ; (GDT base gets set above)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;   global descriptor table (GDT)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        align 0x2

global ASM_PFX(GDT_BASE)
ASM_PFX(GDT_BASE):
; null descriptor
%define NULL_SEL $-ASM_PFX(GDT_BASE)
        dw 0            ; limit 15:0
        dw 0            ; base 15:0
        db 0            ; base 23:16
        db 0            ; type
        db 0            ; limit 19:16, flags
        db 0            ; base 31:24

; linear data segment descriptor
%define LINEAR_SEL $-ASM_PFX(GDT_BASE)
        dw 0xFFFF       ; limit 0xFFFFF
        dw 0            ; base 0
        db 0
        db 0x92         ; present, ring 0, data, expand-up, writable
        db 0xCF                 ; page-granular, 32-bit
        db 0

; linear code segment descriptor
%define LINEAR_CODE_SEL $-ASM_PFX(GDT_BASE)
        dw 0xFFFF       ; limit 0xFFFFF
        dw 0            ; base 0
        db 0
        db 0x9A         ; present, ring 0, data, expand-up, writable
        db 0xCF                 ; page-granular, 32-bit
        db 0

; system data segment descriptor
%define SYS_DATA_SEL $-ASM_PFX(GDT_BASE)
        dw 0xFFFF       ; limit 0xFFFFF
        dw 0            ; base 0
        db 0
        db 0x92         ; present, ring 0, data, expand-up, writable
        db 0xCF                 ; page-granular, 32-bit
        db 0

; system code segment descriptor
%define SYS_CODE_SEL $-ASM_PFX(GDT_BASE)
        dw 0xFFFF       ; limit 0xFFFFF
        dw 0            ; base 0
        db 0
        db 0x9A         ; present, ring 0, data, expand-up, writable
        db 0xCF                 ; page-granular, 32-bit
        db 0

; spare segment descriptor
%define SPARE3_SEL $-ASM_PFX(GDT_BASE)
        dw 0            ; limit 0xFFFFF
        dw 0            ; base 0
        db 0
        db 0            ; present, ring 0, data, expand-up, writable
        db 0            ; page-granular, 32-bit
        db 0

; spare segment descriptor
%define SPARE4_SEL $-ASM_PFX(GDT_BASE)
        dw 0            ; limit 0xFFFFF
        dw 0            ; base 0
        db 0
        db 0            ; present, ring 0, data, expand-up, writable
        db 0            ; page-granular, 32-bit
        db 0

; spare segment descriptor
%define SPARE5_SEL $-ASM_PFX(GDT_BASE)
        dw 0            ; limit 0xFFFFF
        dw 0            ; base 0
        db 0
        db 0            ; present, ring 0, data, expand-up, writable
        db 0            ; page-granular, 32-bit
        db 0

GDT_END:

        align 0x2

idtr: dw IDT_END - IDT_BASE - 1   ; IDT limit
        dd 0                        ; (IDT base gets set above)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;   interrupt descriptor table (IDT)
;
;   Note: The hardware IRQ's specified in this table are the normal PC/AT IRQ
;       mappings.  This implementation only uses the system timer and all other
;       IRQs will remain masked.  The descriptors for vectors 33+ are provided
;       for convenience.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;idt_tag db "IDT",0
        align 0x2

global ASM_PFX(IDT_BASE)
ASM_PFX(IDT_BASE):
; divide by zero (INT 0)
%define DIV_ZERO_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; debug exception (INT 1)
%define DEBUG_EXCEPT_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; NMI (INT 2)
%define NMI_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; soft breakpoint (INT 3)
%define BREAKPOINT_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; overflow (INT 4)
%define OVERFLOW_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; bounds check (INT 5)
%define BOUNDS_CHECK_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; invalid opcode (INT 6)
%define INVALID_OPCODE_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; device not available (INT 7)
%define DEV_NOT_AVAIL_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; double fault (INT 8)
%define DOUBLE_FAULT_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; Coprocessor segment overrun - reserved (INT 9)
%define RSVD_INTR_SEL1 $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; invalid TSS (INT 0ah)
%define INVALID_TSS_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; segment not present (INT 0bh)
%define SEG_NOT_PRESENT_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; stack fault (INT 0ch)
%define STACK_FAULT_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; general protection (INT 0dh)
%define GP_FAULT_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; page fault (INT 0eh)
%define PAGE_FAULT_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; Intel reserved - do not use (INT 0fh)
%define RSVD_INTR_SEL2 $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; floating point error (INT 10h)
%define FLT_POINT_ERR_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; alignment check (INT 11h)
%define ALIGNMENT_CHECK_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; machine check (INT 12h)
%define MACHINE_CHECK_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; SIMD floating-point exception (INT 13h)
%define SIMD_EXCEPTION_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; 85 unspecified descriptors, First 12 of them are reserved, the rest are avail
        db (85 * 8) dup(0)

; IRQ 0 (System timer) - (INT 68h)
%define IRQ0_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 1 (8042 Keyboard controller) - (INT 69h)
%define IRQ1_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; Reserved - IRQ 2 redirect (IRQ 2) - DO NOT USE!!! - (INT 6ah)
%define IRQ2_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 3 (COM 2) - (INT 6bh)
%define IRQ3_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 4 (COM 1) - (INT 6ch)
%define IRQ4_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 5 (LPT 2) - (INT 6dh)
%define IRQ5_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 6 (Floppy controller) - (INT 6eh)
%define IRQ6_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 7 (LPT 1) - (INT 6fh)
%define IRQ7_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 8 (RTC Alarm) - (INT 70h)
%define IRQ8_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 9 - (INT 71h)
%define IRQ9_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 10 - (INT 72h)
%define IRQ10_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 11 - (INT 73h)
%define IRQ11_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 12 (PS/2 mouse) - (INT 74h)
%define IRQ12_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 13 (Floating point error) - (INT 75h)
%define IRQ13_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 14 (Secondary IDE) - (INT 76h)
%define IRQ14_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

; IRQ 15 (Primary IDE) - (INT 77h)
%define IRQ15_SEL $-ASM_PFX(IDT_BASE)
        dw 0            ; offset 15:0
        dw SYS_CODE_SEL ; selector 15:0
        db 0            ; 0 for interrupt gate
        db 0xe | 0x80   ; (10001110)type = 386 interrupt gate, present
        dw 0            ; offset 31:16

IDT_END:

        align 0x2

MemoryMapSize: dd 0
MemoryMap: dd 0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0

        dd  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        dd  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

        org 0xfe0
MyStack:
        ; below is the pieces of the IVT that is used to redirect INT 68h - 6fh
        ;    back to INT 08h - 0fh  when in real mode...  It is 'org'ed to a
        ;    known low address (20f00) so it can be set up by PlMapIrqToVect in
        ;    8259.c

        int 8
        iret

        int 9
        iret

        int 10
        iret

        int 11
        iret

        int 12
        iret

        int 13
        iret

        int 14
        iret

        int 15
        iret

        org 0xffe
BlockSignature:
        dw  0xaa55

