;------------------------------------------------------------------------------
;*
;*   Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
;*   This program and the accompanying materials
;*   are licensed and made available under the terms and conditions of the BSD License
;*   which accompanies this distribution.  The full text of the license may be found at
;*   http://opensource.org/licenses/bsd-license.php
;*
;*   THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
;*   WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;*
;*    efi64.nasm
;*
;*   Abstract:
;*
;------------------------------------------------------------------------------

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Now in 64-bit long mode.
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
    db      0xe9                        ; jmp 16 bit reletive
    dd      commonIdtEntry - $ - 4      ;  offset to jump to
%endmacro

Start:

    mov     esp,0x1fffe8 ; make final stack aligned

    ; set OSFXSR and OSXMMEXCPT because some code will use XMM register
    db 0xf
    db 0x20
    db 0xe0
;    mov rax, cr4
    bts eax, 9
    bts eax, 0xa
    db 0xf
    db 0x22
    db 0xe0
;    mov cr4, rax

    call    ClearScreen

    ; Populate IDT with meaningful offsets for exception handlers...
    mov     eax, offset Idtr
    sidt    [eax]             ; get fword address of IDT

    mov     eax, offset Halt
    mov     ebx, eax                    ; use bx to copy 15..0 to descriptors
    shr     eax, 16                     ; use ax to copy 31..16 to descriptors
                                        ; 63..32 of descriptors is 0
    mov     ecx, 0x78                    ; 78h IDT entries to initialize with unique entry points (exceptions)
    mov     esi, [offset Idtr + 2]
    mov     edi, [esi]

.0:
    mov     word [edi], bx                  ; write bits 15..0 of offset
    mov     word [edi+2], 0x38               ; SYS_CODE64_SEL from GDT
    mov     word [edi+4], 0xe00 | 0x8000    ; type = 386 interrupt gate, present
    mov     word [edi+6], ax                ; write bits 31..16 of offset
    mov     dword [edi+8], 0                ; write bits 63..32 of offset
    add     edi, 16                             ; move up to next descriptor
    add     bx, DEFAULT_HANDLER_SIZE            ; move to next entry point
    loop    .0                                  ; loop back through again until all descriptors are initialized

    ;; at this point edi contains the offset of the descriptor for INT 20
    ;; and bx contains the low 16 bits of the offset of the default handler
    ;; so initialize all the rest of the descriptors with these two values...
;    mov     ecx, 101                            ; there are 100 descriptors left (INT 20 (14h) - INT 119 (77h)
;@@:                                             ; loop through all IDT entries exception handlers and initialize to default handler
;    mov     word ptr [edi], bx                  ; write bits 15..0 of offset
;    mov     word ptr [edi+2], 38h               ; SYS_CODE64_SEL from GDT
;    mov     word ptr [edi+4], 0e00h OR 8000h    ; type = 386 interrupt gate, present
;    mov     word ptr [edi+6], ax                ; write bits 31..16 of offset
;    mov     dword ptr [edi+8], 0                ; write bits 63..32 of offset
;    add     edi, 16                             ; move up to next descriptor
;    loop    @b                                  ; loop back through again until all descriptors are initialized

;;  DUMP    location of IDT and several of the descriptors
;    mov     ecx, 8
;    mov     eax, [offset Idtr + 2]
;    mov     eax, [eax]
;    mov     edi, 0b8000h
;    call    PrintQword
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
    mov     edi,[ebp+0x30]              ; edi = [[22000 + [22014] + 3c] + 2c] = ImageBase (63..32 is zero, ignore)
    mov     eax,[ebp+0x28]              ; eax = [[22000 + [22014] + 3c] + 24] = EntryPoint
    add     eax,edi                     ; eax = ImageBase + EntryPoint
    mov     ebx, offset EfiLdrOffset
    mov     dword [ebx],eax         ; Modify far jump instruction for correct entry point

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
    db 0x66
    db 0xff
    db 0xcb
;    dec     bx
    cmp     bx,0
    jne     SectionLoop

    mov     edx, offset Idtr
    movzx   eax, word [edx]          ; get size of IDT
    db 0xff
    db 0xc0
;    inc     eax
    add     eax, dword [edx + 2]     ; add to base of IDT to get location of memory map...
    xor     ecx, ecx
    mov     ecx, eax                     ; put argument to RCX

    db 0x48
    db 0xc7
    db 0xc0
EfiLdrOffset:
    dd      0x401000                  ; Offset of EFILDR
;   mov rax, 401000h
    db 0x50
;   push rax

; ret
    db 0xc3

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
    push    eax
    push    ecx
    push    edx
    push    ebx
    push    esp
    push    ebp
    push    esi
    push    edi
    db 0x41
    db 0x50
;    push    r8
    db 0x41
    db 0x51
;    push    r9
    db 0x41
    db 0x52
;    push    r10
    db 0x41
    db 0x53
;    push    r11
    db 0x41
    db 0x54
;    push    r12
    db 0x41
    db 0x55
;    push    r13
    db 0x41
    db 0x56
;    push    r14
    db 0x41
    db 0x57
;    push    r15
    db 0x48
    mov     ebp, esp
;    mov     rbp, rsp

;;
;;  At this point the stack looks like this:
;;
;;      Calling SS
;;      Calling RSP
;;      rflags
;;      Calling CS
;;      Calling RIP
;;      Error code or 0
;;      Int num or 0ffh for unknown int num
;;      rax
;;      rcx
;;      rdx
;;      rbx
;;      rsp
;;      rbp
;;      rsi
;;      rdi
;;      r8
;;      r9
;;      r10
;;      r11
;;      r12
;;      r13
;;      r14
;;      r15 <------- RSP, RBP
;;

    call    ClearScreen
    mov     esi, offset String1
    call    PrintString
    db 0x48
    mov     eax, [ebp + 16*8]     ;; move Int number into RAX
    db 0x48
    cmp     eax, 18
    ja      PrintDefaultString
PrintExceptionString:
    shl     eax, 3              ;; multiply by 8 to get offset from StringTable to actual string address
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
    db 0x48
    mov     eax, [ebp+19*8]    ; CS
    call    PrintQword
    mov     al, ':'
    mov     byte [edi], al
    add     edi, 2
    db 0x48
    mov     eax, [ebp+18*8]    ; RIP
    call    PrintQword
    mov     esi, offset String3
    call    PrintString

    mov     edi, 0xb8140

    mov     esi, offset StringRax     ; rax
    call    PrintString
    db 0x48
    mov     eax, [ebp+15*8]
    call    PrintQword

    mov     esi, offset StringRcx     ; rcx
    call    PrintString
    db 0x48
    mov     eax, [ebp+14*8]
    call    PrintQword

    mov     esi, offset StringRdx     ; rdx
    call    PrintString
    db 0x48
    mov     eax, [ebp+13*8]
    call    PrintQword

    mov     edi, 0xb81e0

    mov     esi, offset StringRbx     ; rbx
    call    PrintString
    db 0x48
    mov     eax, [ebp+12*8]
    call    PrintQword

    mov     esi, offset StringRsp     ; rsp
    call    PrintString
    db 0x48
    mov     eax, [ebp+21*8]
    call    PrintQword

    mov     esi, offset StringRbp     ; rbp
    call    PrintString
    db 0x48
    mov     eax, [ebp+10*8]
    call    PrintQword

    mov     edi, 0xb8280

    mov     esi, offset StringRsi     ; rsi
    call    PrintString
    db 0x48
    mov     eax, [ebp+9*8]
    call    PrintQword

    mov     esi, offset StringRdi     ; rdi
    call    PrintString
    db 0x48
    mov     eax, [ebp+8*8]
    call    PrintQword

    mov     esi, offset StringEcode   ; error code
    call    PrintString
    db 0x48
    mov     eax, [ebp+17*8]
    call    PrintQword

    mov     edi, 0xb8320

    mov     esi, offset StringR8      ; r8
    call    PrintString
    db 0x48
    mov     eax, [ebp+7*8]
    call    PrintQword

    mov     esi, offset StringR9      ; r9
    call    PrintString
    db 0x48
    mov     eax, [ebp+6*8]
    call    PrintQword

    mov     esi, offset StringR10     ; r10
    call    PrintString
    db 0x48
    mov     eax, [ebp+5*8]
    call    PrintQword

    mov     edi, 0xb83c0

    mov     esi, offset StringR11     ; r11
    call    PrintString
    db 0x48
    mov     eax, [ebp+4*8]
    call    PrintQword

    mov     esi, offset StringR12     ; r12
    call    PrintString
    db 0x48
    mov     eax, [ebp+3*8]
    call    PrintQword

    mov     esi, offset StringR13     ; r13
    call    PrintString
    db 0x48
    mov     eax, [ebp+2*8]
    call    PrintQword

    mov     edi, 0xb8460

    mov     esi, offset StringR14     ; r14
    call    PrintString
    db 0x48
    mov     eax, [ebp+1*8]
    call    PrintQword

    mov     esi, offset StringR15     ; r15
    call    PrintString
    db 0x48
    mov     eax, [ebp+0*8]
    call    PrintQword

    mov     esi, offset StringSs      ; ss
    call    PrintString
    db 0x48
    mov     eax, [ebp+22*8]
    call    PrintQword

    mov     edi, 0xb8500

    mov     esi, offset StringRflags  ; rflags
    call    PrintString
    db 0x48
    mov     eax, [ebp+20*8]
    call    PrintQword

    mov     edi, 0xb8640

    mov     esi, ebp
    add     esi, 23*8
    mov     ecx, 4

OuterLoop:
    push    ecx
    mov     ecx, 4
    db 0x48
    mov     edx, edi

InnerLoop:
    db 0x48
    mov     eax, [esi]
    call    PrintQword
    add     esi, 8
    mov     al, ' '
    mov     [edi], al
    add     edi, 2
    loop    InnerLoop

    pop     ecx
    add     edx, 0xa0
    mov     edi, edx
    loop    OuterLoop

    mov     edi, 0xb8960

    db 0x48
    mov     eax, [ebp+18*8]  ; RIP
    sub     eax, 8 * 8
    db 0x48
    mov     esi, eax        ; esi = rip - 8 QWORD linear (total 16 QWORD)

    mov     ecx, 4

OuterLoop1:
    push    ecx
    mov     ecx, 4
    mov     edx, edi

InnerLoop1:
    db 0x48
    mov     eax, [esi]
    call    PrintQword
    add     esi, 8
    mov     al, ' '
    mov     [edi], al
    add     edi, 2
    loop    InnerLoop1

    pop     ecx
    add     edx, 0xa0
    mov     edi, edx
    loop    OuterLoop1

    ;wbinvd
.1:
    jmp     .1

;
; return
;
    mov     esp, ebp
;    mov     rsp, rbp
    db 0x41
    db 0x5f
;    pop    r15
    db 0x41
    db 0x5e
;    pop    r14
    db 0x41
    db 0x5d
;    pop    r13
    db 0x41
    db 0x5c
;    pop    r12
    db 0x41
    db 0x5b
;    pop    r11
    db 0x41
    db 0x5a
;    pop    r10
    db 0x41
    db 0x59
;    pop    r9
    db 0x41
    db 0x58
;    pop    r8
    pop    edi
    pop    esi
    pop    ebp
    pop    eax ; esp
    pop    ebx
    pop    edx
    pop    ecx
    pop    eax

    db 0x48
    db 0x83
    db 0xc4
    db 0x10
;    add    esp, 16 ; error code and INT number

    db 0x48
    db 0xcf
;    iretq

PrintString:
    push    eax
.2:
    mov     al, byte [esi]
    cmp     al, 0
    je      .3
    mov     byte [edi], al
    db 0xff
    db 0xc6
;    inc     esi
    add     edi, 2
    jmp     .2
.3:
    pop     eax
    ret

;; RAX contains qword to print
;; RDI contains memory location (screen location) to print it to
PrintQword:
    push    ecx
    push    ebx
    push    eax

    db 0x48
    db 0xc7
    db 0xc1
    dd 16
;    mov     rcx, 16
looptop:
    db 0x48
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

StringTable: dq offset Int0String, offset Int1String, offset Int2String, offset Int3String,
                      offset Int4String, offset Int5String, offset Int6String, offset Int7String,
                      offset Int8String, offset Int9String, offset Int10String, offset Int11String,
                      offset Int12String, offset Int13String, offset Int14String, offset Int15String,
                      offset Int16String, offset Int17String, offset Int18String, offset Int19String

String2: db " HALT!! *** (",0
String3: db ")",0
StringRax: db "RAX=",0
StringRcx: db " RCX=",0
StringRdx: db " RDX=",0
StringRbx: db "RBX=",0
StringRsp: db " RSP=",0
StringRbp: db " RBP=",0
StringRsi: db "RSI=",0
StringRdi: db " RDI=",0
StringEcode: db " ECODE=",0
StringR8: db "R8 =",0
StringR9: db " R9 =",0
StringR10: db " R10=",0
StringR11: db "R11=",0
StringR12: db " R12=",0
StringR13: db " R13=",0
StringR14: db "R14=",0
StringR15: db " R15=",0
StringSs: db " SS =",0
StringRflags: db "RFLAGS=",0

Idtr        df  0
            df  0

    org 0x21ffe
BlockSignature:
    dw      0xaa55

