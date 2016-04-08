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
;*    efi32.nasm
;*
;*   Abstract:
;*
;------------------------------------------------------------------------------

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Now in 32-bit protected mode.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        .stack
        SECTION .text
        org 0x21000

%define DEFAULT_HANDLER_SIZE INT1 - INT0

%macro JmpCommonIdtEntry 0
    ; jmp     commonIdtEntry - this must be hand coded to keep the assembler from
    ;                          using a 8 bit reletive jump when the entries are
    ;                          within 255 bytes of the common entry.  This must
    ;                          be done to maintain the consistency of the size
    ;                          of entry points...
    db      0xe9                        ; jmp 16 bit relative
    dd      commonIdtEntry - $ - 4      ;  offset to jump to
%endmacro

Start:
    mov     ax,bx                      ; flat data descriptor in BX
    mov     ds,ax
    mov     es,ax
    mov     fs,ax
    mov     gs,ax
    mov     ss,ax
    mov     esp,0x1ffff0

    call    ClearScreen

    ; Populate IDT with meaningful offsets for exception handlers...
    sidt    [Idtr]             ; get fword address of IDT

    mov     eax, offset Halt
    mov     ebx, eax                    ; use bx to copy 15..0 to descriptors
    shr     eax, 16                     ; use ax to copy 31..16 to descriptors
    mov     ecx, 0x78                    ; 78h IDT entries to initialize with unique entry points (exceptions)
    mov     esi, [offset Idtr + 2]
    mov     edi, [esi]

.0:
    mov     word [edi], bx                  ; write bits 15..0 of offset
    mov     word [edi+2], 0x20               ; SYS_CODE_SEL from GDT
    mov     word [edi+4], 0xe00 | 0x8000    ; type = 386 interrupt gate, present
    mov     word [edi+6], ax                ; write bits 31..16 of offset
    add     edi, 8                              ; move up to next descriptor
    add     bx, DEFAULT_HANDLER_SIZE            ; move to next entry point
    loop    .0                                  ; loop back through again until all descriptors are initialized

    ;; at this point edi contains the offset of the descriptor for INT 20
    ;; and bx contains the low 16 bits of the offset of the default handler
    ;; so initialize all the rest of the descriptors with these two values...
;    mov     ecx, 101                            ; there are 100 descriptors left (INT 20 (14h) - INT 119 (77h)
;@@:                                             ; loop through all IDT entries exception handlers and initialize to default handler
;    mov     word ptr [edi], bx                  ; write bits 15..0 of offset
;    mov     word ptr [edi+2], 20h               ; SYS_CODE_SEL from GDT
;    mov     word ptr [edi+4], 0e00h OR 8000h    ; type = 386 interrupt gate, present
;    mov     word ptr [edi+6], ax                ; write bits 31..16 of offset
;    add     edi, 8                              ; move up to next descriptor
;    loop    @b                                  ; loop back through again until all descriptors are initialized

;;  DUMP    location of IDT and several of the descriptors
;    mov     ecx, 8
;    mov     eax, [offset Idtr + 2]
;    mov     eax, [eax]
;    mov     edi, 0b8000h
;    call    PrintDword
;    mov     esi, eax
;    mov     edi, 0b80a0h
;    jmp     OuterLoop

;;
;; just for fun, let's do a software interrupt to see if we correctly land in the exception handler...
;    mov     eax, 011111111h
;    mov     ebx, 022222222h
;    mov     ecx, 033333333h
;    mov     edx, 044444444h
;    mov     ebp, 055555555h
;    mov     esi, 066666666h
;    mov     edi, 077777777h
;    push    011111111h
;    push    022222222h
;    push    033333333h
;    int     119

    mov     esi,0x22000                 ; esi = 22000
    mov     eax,[esi+0x14]              ; eax = [22014]
    add     esi,eax                     ; esi = 22000 + [22014] = Base of EFILDR.C
    mov     ebp,[esi+0x3c]              ; ebp = [22000 + [22014] + 3c] = NT Image Header for EFILDR.C
    add     ebp,esi
    mov     edi,[ebp+0x34]              ; edi = [[22000 + [22014] + 3c] + 30] = ImageBase
    mov     eax,[ebp+0x28]              ; eax = [[22000 + [22014] + 3c] + 24] = EntryPoint
    add     eax,edi                     ; eax = ImageBase + EntryPoint
    mov     dword [EfiLdrOffset],eax   ; Modify far jump instruction for correct entry point

    mov     bx,word ptr[ebp+6]          ; bx = Number of sections
    xor     eax,eax
    mov     ax,word ptr[ebp+0x14]       ; ax = Optional Header Size
    add     ebp,eax
    add     ebp,0x18                    ; ebp = Start of 1st Section

SectionLoop:
    push    esi                         ; Save Base of EFILDR.C
    push    edi                         ; Save ImageBase
    add     esi,[ebp+0x14]              ; esi = Base of EFILDR.C + PointerToRawData
    add     edi,[ebp+0xc]              ; edi = ImageBase + VirtualAddress
    mov     ecx,[ebp+0x10]              ; ecs = SizeOfRawData

    cld
    shr     ecx,2
    rep     movsd

    pop     edi                         ; Restore ImageBase
    pop     esi                         ; Restore Base of EFILDR.C

    add     bp,0x28                     ; ebp = ebp + 028h = Pointer to next section record
    dec     bx
    cmp     bx,0
    jne     SectionLoop

    movzx   eax, word [Idtr]         ; get size of IDT
    inc     eax
    add     eax, dword [Idtr + 2]    ; add to base of IDT to get location of memory map...
    push    eax                         ; push memory map location on stack for call to EFILDR...

    push    eax                         ; push return address (useless, just for stack balance)
    db      0xb8
EfiLdrOffset:
    dd      0x401000                  ; Offset of EFILDR
; mov eax, 401000h
    push    eax
    ret

;    db      "**** DEFAULT IDT ENTRY ***",0
    align 0x2
Halt:
INT0:
    push    0x0      ; push error code place holder on the stack
    push    0x0
    JmpCommonIdtEntry
;    db      0e9h                        ; jmp 16 bit reletive
;    dd      commonIdtEntry - $ - 4      ;  offset to jump to

INT1:
    push    0x0      ; push error code place holder on the stack
    push    0x1
    JmpCommonIdtEntry

INT2:
    push    0x0      ; push error code place holder on the stack
    push    0x2
    JmpCommonIdtEntry

INT3:
    push    0x0      ; push error code place holder on the stack
    push    0x3
    JmpCommonIdtEntry

INT4:
    push    0x0      ; push error code place holder on the stack
    push    0x4
    JmpCommonIdtEntry

INT5:
    push    0x0      ; push error code place holder on the stack
    push    0x5
    JmpCommonIdtEntry

INT6:
    push    0x0      ; push error code place holder on the stack
    push    0x6
    JmpCommonIdtEntry

INT7:
    push    0x0      ; push error code place holder on the stack
    push    0x7
    JmpCommonIdtEntry

INT8:
;   Double fault causes an error code to be pushed so no phony push necessary
    nop
    nop
    push    0x8
    JmpCommonIdtEntry

INT9:
    push    0x0      ; push error code place holder on the stack
    push    0x9
    JmpCommonIdtEntry

INT10:
;   Invalid TSS causes an error code to be pushed so no phony push necessary
    nop
    nop
    push    10
    JmpCommonIdtEntry

INT11:
;   Segment Not Present causes an error code to be pushed so no phony push necessary
    nop
    nop
    push    11
    JmpCommonIdtEntry

INT12:
;   Stack fault causes an error code to be pushed so no phony push necessary
    nop
    nop
    push    12
    JmpCommonIdtEntry

INT13:
;   GP fault causes an error code to be pushed so no phony push necessary
    nop
    nop
    push    13
    JmpCommonIdtEntry

INT14:
;   Page fault causes an error code to be pushed so no phony push necessary
    nop
    nop
    push    14
    JmpCommonIdtEntry

INT15:
    push    0x0      ; push error code place holder on the stack
    push    15
    JmpCommonIdtEntry

INT16:
    push    0x0      ; push error code place holder on the stack
    push    16
    JmpCommonIdtEntry

INT17:
;   Alignment check causes an error code to be pushed so no phony push necessary
    nop
    nop
    push    17
    JmpCommonIdtEntry

INT18:
    push    0x0      ; push error code place holder on the stack
    push    18
    JmpCommonIdtEntry

INT19:
    push    0x0      ; push error code place holder on the stack
    push    19
    JmpCommonIdtEntry

INTUnknown:
REPEAT  (0x78 - 20)
    push    0x0      ; push error code place holder on the stack
;    push    xxh     ; push vector number
    db      0x6a
    db      ( $ - INTUnknown - 3 ) / 9 + 20 ; vector number
    JmpCommonIdtEntry
%endmacro

commonIdtEntry:
    pushad
    mov     ebp, esp
;;
;;  At this point the stack looks like this:
;;
;;      eflags
;;      Calling CS
;;      Calling EIP
;;      Error code or 0
;;      Int num or 0ffh for unknown int num
;;      eax
;;      ecx
;;      edx
;;      ebx
;;      esp
;;      ebp
;;      esi
;;      edi <------- ESP, EBP
;;

    call    ClearScreen
    mov     esi, offset String1
    call    PrintString
    mov     eax, [ebp + 32]     ;; move Int number into EAX
    cmp     eax, 19
    ja      PrintDefaultString
PrintExceptionString:
    shl     eax, 2              ;; multiply by 4 to get offset from StringTable to actual string address
    add     eax, offset StringTable
    mov     esi, [eax]
    jmp     PrintTheString
PrintDefaultString:
    mov     esi, offset IntUnknownString
    ; patch Int number
    mov     edx, eax
    call    A2C
    mov     [esi + 1], al
    mov     eax, edx
    shr     eax, 4
    call    A2C
    mov     [esi], al
PrintTheString:
    call    PrintString
    mov     esi, offset String2
    call    PrintString
    mov     eax, [ebp+44]          ; CS
    call    PrintDword
    mov     al, ':'
    mov     byte [edi], al
    add     edi, 2
    mov     eax, [ebp+40]          ; EIP
    call    PrintDword
    mov     esi, offset String3
    call    PrintString

    mov     edi, 0xb8140

    mov     esi, offset StringEax     ; eax
    call    PrintString
    mov     eax, [ebp+28]
    call    PrintDword

    mov     esi, offset StringEbx     ; ebx
    call    PrintString
    mov     eax, [ebp+16]
    call    PrintDword

    mov     esi, offset StringEcx     ; ecx
    call    PrintString
    mov     eax, [ebp+24]
    call    PrintDword

    mov     esi, offset StringEdx     ; edx
    call    PrintString
    mov     eax, [ebp+20]
    call    PrintDword

    mov     esi, offset StringEcode   ; error code
    call    PrintString
    mov     eax, [ebp+36]
    call    PrintDword

    mov     edi, 0xb81e0

    mov     esi, offset StringEsp     ; esp
    call    PrintString
    mov     eax, [ebp+12]
    call    PrintDword

    mov     esi, offset StringEbp     ; ebp
    call    PrintString
    mov     eax, [ebp+8]
    call    PrintDword

    mov     esi, offset StringEsi     ; esi
    call    PrintString
    mov     eax, [ebp+4]
    call    PrintDword

    mov     esi, offset StringEdi    ; edi
    call    PrintString
    mov     eax, [ebp]
    call    PrintDword

    mov     esi, offset StringEflags ; eflags
    call    PrintString
    mov     eax, [ebp+48]
    call    PrintDword

    mov     edi, 0xb8320

    mov     esi, ebp
    add     esi, 52
    mov     ecx, 8

OuterLoop:
    push    ecx
    mov     ecx, 8
    mov     edx, edi

InnerLoop:
    mov     eax, [esi]
    call    PrintDword
    add     esi, 4
    mov     al, ' '
    mov     [edi], al
    add     edi, 2
    loop    InnerLoop

    pop     ecx
    add     edx, 0xa0
    mov     edi, edx
    loop    OuterLoop

    mov     edi, 0xb8960

    mov     eax, [ebp+40]  ; EIP
    sub     eax, 32 * 4
    mov     esi, eax        ; esi = eip - 32 DWORD linear (total 64 DWORD)

    mov     ecx, 8

OuterLoop1:
    push    ecx
    mov     ecx, 8
    mov     edx, edi

InnerLoop1:
    mov     eax, [esi]
    call    PrintDword
    add     esi, 4
    mov     al, ' '
    mov     [edi], al
    add     edi, 2
    loop    InnerLoop1

    pop     ecx
    add     edx, 0xa0
    mov     edi, edx
    loop    OuterLoop1

;    wbinvd ; Ken: this intruction does not support in early than 486 arch
.1:
    jmp     .1
;
; return
;
    mov     esp, ebp
    popad
    add     esp, 8 ; error code and INT number

    iretd

PrintString:
    push    eax
.2:
    mov     al, byte [esi]
    cmp     al, 0
    je      .3
    mov     byte [edi], al
    inc     esi
    add     edi, 2
    jmp     .2
.3:
    pop     eax
    ret

;; EAX contains dword to print
;; EDI contains memory location (screen location) to print it to
PrintDword:
    push    ecx
    push    ebx
    push    eax

    mov     ecx, 8
looptop:
    rol     eax, 4
    mov     bl, al
    and     bl, 0xf
    add     bl, '0'
    cmp     bl, '9'
    jle     .4
    add     bl, 7
.4:
    mov     byte [edi], bl
    add     edi, 2
    loop    looptop
    ;wbinvd

    pop     eax
    pop     ebx
    pop     ecx
    ret

ClearScreen:
    push    eax
    push    ecx

    mov     al, ' '
    mov     ah, 0xc
    mov     edi, 0xb8000
    mov     ecx, 80 * 24
.5:
    mov     word [edi], ax
    add     edi, 2
    loop    .5
    mov     edi, 0xb8000

    pop     ecx
    pop     eax

    ret

A2C:
    and     al, 0xf
    add     al, '0'
    cmp     al, '9'
    jle     .6
    add     al, 7
.6:
    ret

String1: db "*** INT ",0

Int0String: db "0x0 Divide by 0 -",0
Int1String: db "0x1 Debug exception -",0
Int2String: db "0x2 NMI -",0
Int3String: db "0x3 Breakpoint -",0
Int4String: db "0x4 Overflow -",0
Int5String: db "0x5 Bound -",0
Int6String: db "0x6 Invalid opcode -",0
Int7String: db "0x7 Device ~ available -",0
Int8String: db "0x8 Double fault -",0
Int9String: db "0x9 Coprocessor seg overrun (reserved) -",0
Int10String: db "0xA Invalid TSS -",0
Int11String: db "0xB Segment ~ present -",0
Int12String: db "0xC Stack fault -",0
Int13String: db "0xD General protection fault -",0
Int14String: db "0xE Page fault -",0
Int15String: db "0xF (Intel reserved) -",0
Int16String: db "0x10 Floating point error -",0
Int17String: db "0x11 Alignment check -",0
Int18String: db "0x12 Machine check -",0
Int19String: db "0x13 SIMD Floating-Point Exception -",0
IntUnknownString: db "??h Unknown interrupt -",0

StringTable: dd offset Int0String, offset Int1String, offset Int2String, offset Int3String,
                      offset Int4String, offset Int5String, offset Int6String, offset Int7String,
                      offset Int8String, offset Int9String, offset Int10String, offset Int11String,
                      offset Int12String, offset Int13String, offset Int14String, offset Int15String,
                      offset Int16String, offset Int17String, offset Int18String, offset Int19String

String2: db " HALT!! *** (",0
String3: db ")",0
StringEax: db "EAX=",0
StringEbx: db " EBX=",0
StringEcx: db " ECX=",0
StringEdx: db " EDX=",0
StringEcode: db " ECODE=",0
StringEsp: db "ESP=",0
StringEbp: db " EBP=",0
StringEsi: db " ESI=",0
StringEdi: db " EDI=",0
StringEflags: db " EFLAGS=",0

Idtr        df  0

    org 0x21ffe
BlockSignature:
    dw      0xaa55

