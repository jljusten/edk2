;------------------------------------------------------------------------------
; @file
; A minimal INT 10 stub which allows some UEFI OS's with a legacy INT 10
; dependency to boot.
;
; Copyright (c) 2013 - 2014, Intel Corporation. All rights reserved.<BR>
;
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

;
; Define DEBUG to enable debug messages
;
%define DEBUG

BITS    16

%define IMAGE_SECTORS ((EndOfStub - StartOfStub + 511) / 512)

ORG     0
StartOfStub:

        db      0x55, 0xaa, IMAGE_SECTORS

Int10Entry:
        jmp     short MainInt10Code

TIMES 0x10-($-$$) DB 0                  ; Offset 0x10

TIMES 0x18-($-$$) DB 0                  ; Offset 0x18
        dw      PcirStructure

TIMES 0x1e-($-$$) DB 0                  ; Offset 0x1e
        db      'I', 'B', 'M', 0

ALIGN   4
PcirStructure:
        db      'P', 'C', 'I', 'R'      ; UINT32  Signature
        dw      0x1013, 0x00B8          ; UINT16  VendorId, DeviceId
        dw      0                       ; UINT16  Reserved0
        dw      (PcirStructureEnd - PcirStructure) ; UINT16  Length
        db      0                       ; UINT8   Revision
        db      0, 0, 3                 ; UINT8   ClassCode[3]
        dw      IMAGE_SECTORS           ; UINT16  ImageLength
        dw      0                       ; UINT16  CodeRevision
        db      0                       ; UINT8   CodeType
        db      0x80                    ; UINT8   Indicator
        dw      0                       ; UINT16  Reserved1
PcirStructureEnd:

MainInt10Code:
%ifdef DEBUG
        call    DebugTraceOnEntry
%endif
        cmp     ah, 0x4f
        jne     MainInt10CodeDone
        cmp     al, 0x00
        je      x4f00
        cmp     al, 0x01
        je      x4f01
        cmp     al, 0x02
        je      x4f02
        cmp     al, 0x03
        je      x4f03
MainInt10Code4fDone:
        mov     al, 0x4f
MainInt10CodeDone:
        mov     ah, 0
        iret
MainInt10CodeError:
        mov     ah, 0x01
        iret

x4f00:
        push    cx
        push    bx
        mov     cx, x4f00DataEnd - x4f00Data
        lea     bx, [cs:x4f00Data]
        call    CopyMemCsBxToEsDiSizeCx
        pop     bx
        pop     cx
        jmp     MainInt10Code4fDone
x4f00Data:
        dd      'VESA'
        dw      0x0200          ; Version 2.0
        dw      OemString       ; OEM Name
        dw      0xc000
        dd      0               ; Capabilities
        dw      ModeList
        dw      0xc000
        dw      ((1024*768*3) + 0xffff) / 0x10000
x4f00DataEnd:
OemString:
        db      'Intel', 0
ModeList:
        dw      0x0115
        dw      0x0118
        dw      0xffff

x4f01:
        cmp     cx, 0x0115
        je      x4f01ModeOkay
        cmp     cx, 0x0118
        jne     MainInt10CodeError
x4f01ModeOkay:
        push    cx
        push    bx
        mov     cx, x4f01DataEnd - x4f01Data
        lea     bx, [cs:x4f01Data]
        call    CopyMemCsBxToEsDiSizeCx
        pop     bx
        pop     cx
        cmp     cx, 0x0115
        jne     MainInt10Code4fDone
        mov     word [es:di+0x12], 800
        mov     word [es:di+0x14], 600
        jmp     MainInt10Code4fDone
x4f01Data:
        dw      0x00fb          ; Supportted, Optional info, Color, Graphics,
                                ; Not VGA-compat, No bank switched mode,
                                ; Linear framebuffer
        db      0x07            ; Window A: exists, readable, writable
        db      0x00            ; Window B: doesn't exist
        dw      0x0040          ; 64KB granularity (same as 4f00 function)
        dw      0x0040
        dw      0               ; Window A segment
        dw      0
        dd      0               ; 4f05 location
        dw      (1024*3)        ; Bytes per line
        dw      1024            ; Width
        dw      768             ; Height
        db      8               ; Character width
        db      10              ; Character height
        db      1               ; Number of memory planes
        db      24              ; Bits per pixel
        db      1               ; Number of memory banks
        db      0x06            ; Memory model - direct color
        db      0               ; Bank size (KB) ??
        db      3               ; Number of image pages - 1
        db      0               ; Reserved
        db      8               ; Red mask size
        db      16              ; Red position
        db      8               ; Green mask size
        db      8               ; Green position
        db      8               ; Blue mask size
        db      0               ; Blue position
        db      0               ; Reserved mask size
        db      0               ; Reserved position
        db      0               ; Direct color mode info
        dd      0x80000000      ; Framebuffer address (FIX!!)
        dd      0               ; Offscreen pointer
        dw      0               ; Size of offscreen memory
x4f01DataEnd:

x4f02:
        push    bx
        and     bx, ~0x4000
        cmp     bx, 0x0115
        je      x4f02ModeOkay
        cmp     bx, 0x0118
x4f02ModeOkay:
        jne     x4f02DontUpdateCurrentMode
        mov     [cs:xf403CurrentMode], bx
x4f02DontUpdateCurrentMode:
        pop     bx
        jne     MainInt10CodeError
        jmp     MainInt10Code4fDone

xf403CurrentMode:
        dw      0
x4f03:
        mov     bx, [cs:xf403CurrentMode]
        jmp     MainInt10Code4fDone

CopyMemCsBxToEsDiSizeCx:
        push    bx
        push    di
CopyMemCsBxToEsDiSizeCxLoop:
        mov     al, [cs:bx]
        inc     bx
        mov     [es:di], al
        inc     di
        loop    CopyMemCsBxToEsDiSizeCxLoop
        pop     di
        pop     bx
        ret

%ifdef DEBUG

DebugTraceOnEntry:
        push    si
        push    ds
        push    ax

        mov     si, cs
        mov     ds, si
        mov     si, DebubEntryMessage
        call    PrintStringSi
        call    PrintAx

        mov     si, DebubEntryBxMessage
        call    PrintStringSi
        mov     ax, bx
        call    PrintAx

        mov     si, DebubEntryCxMessage
        call    PrintStringSi
        mov     ax, cx
        call    PrintAx

        mov     si, EndLineMessage
        call    PrintStringSi

        pop     ax
        pop     ds
        pop     si
        ret

DebubEntryMessage:
        db      'INT10 AX=', 0

DebubEntryBxMessage:
        db      ' BX=', 0

DebubEntryCxMessage:
        db      ' CX=', 0

EndLineMessage: 
        db      0xd, 0xa, 0

PrintStringSi:
        pusha
        mov     dx, 0x0402
PrintStringSiLoop:
        lodsb
        cmp     al, 0
        je      PrintStringSiDone
        out     dx, al
        jmp     PrintStringSiLoop
PrintStringSiDone:
        popa
        ret

PrintNibbleAl:
        pusha
        mov     dx, 0x0402
        and     al, 0x0f
        cmp     al, 9
        jg      PrintNibbleAlHex
        add     al, '0'
        jmp     PrintNibbleAlDone
PrintNibbleAlHex:
        add     al, 'A'-10
PrintNibbleAlDone:
        out     dx, al
        popa
        ret

PrintAx:
        push    ax
        rol     ax, 4
        call    PrintNibbleAl
        rol     ax, 4
        call    PrintNibbleAl
        rol     ax, 4
        call    PrintNibbleAl
        rol     ax, 4
        call    PrintNibbleAl
        pop     ax
        ret

%endif

EndOfStub:
