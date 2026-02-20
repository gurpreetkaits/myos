; interrupt.asm - ISR and IRQ assembly stubs
; Each stub pushes the interrupt number onto the stack and
; jumps to a common handler that calls the C function isr_handler().

[bits 32]

; External C handler
[extern isr_handler]

; ============================================================
; Macros
; ============================================================

; ISR with no error code pushed by CPU
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0        ; dummy error code
    push dword %1       ; interrupt number
    jmp isr_common_stub
%endmacro

; ISR with error code pushed by CPU
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1       ; interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; IRQ: maps hardware IRQ N to interrupt number M
%macro IRQ 2
global irq%1
irq%1:
    push dword 0        ; dummy error code
    push dword %2       ; remapped interrupt number
    jmp isr_common_stub
%endmacro

; ============================================================
; Exception handlers (INT 0-31)
; ============================================================

ISR_NOERRCODE 0     ; #DE Division Error
ISR_NOERRCODE 1     ; #DB Debug
ISR_NOERRCODE 2     ; NMI
ISR_NOERRCODE 3     ; #BP Breakpoint
ISR_NOERRCODE 4     ; #OF Overflow
ISR_NOERRCODE 5     ; #BR Bound Range
ISR_NOERRCODE 6     ; #UD Invalid Opcode
ISR_NOERRCODE 7     ; #NM Device Not Available
ISR_ERRCODE   8     ; #DF Double Fault
ISR_NOERRCODE 9     ; Coprocessor Segment Overrun
ISR_ERRCODE   10    ; #TS Invalid TSS
ISR_ERRCODE   11    ; #NP Segment Not Present
ISR_ERRCODE   12    ; #SS Stack-Segment Fault
ISR_ERRCODE   13    ; #GP General Protection Fault
ISR_ERRCODE   14    ; #PF Page Fault
ISR_NOERRCODE 15    ; Reserved
ISR_NOERRCODE 16    ; #MF x87 FP Exception
ISR_ERRCODE   17    ; #AC Alignment Check
ISR_NOERRCODE 18    ; #MC Machine Check
ISR_NOERRCODE 19    ; #XM SIMD FP Exception
ISR_NOERRCODE 20    ; #VE Virtualization
ISR_ERRCODE   21    ; #CP Control Protection
ISR_NOERRCODE 22    ; Reserved
ISR_NOERRCODE 23    ; Reserved
ISR_NOERRCODE 24    ; Reserved
ISR_NOERRCODE 25    ; Reserved
ISR_NOERRCODE 26    ; Reserved
ISR_NOERRCODE 27    ; Reserved
ISR_NOERRCODE 28    ; Reserved
ISR_NOERRCODE 29    ; Reserved
ISR_NOERRCODE 30    ; Reserved
ISR_NOERRCODE 31    ; Reserved

; ============================================================
; Hardware IRQ handlers (IRQ 0-15 → INT 32-47)
; ============================================================

IRQ  0, 32     ; PIT Timer
IRQ  1, 33     ; Keyboard
IRQ  2, 34     ; Cascade
IRQ  3, 35     ; COM2
IRQ  4, 36     ; COM1
IRQ  5, 37     ; LPT2
IRQ  6, 38     ; Floppy
IRQ  7, 39     ; LPT1 / Spurious
IRQ  8, 40     ; CMOS RTC
IRQ  9, 41     ; Free
IRQ 10, 42     ; Free
IRQ 11, 43     ; Free
IRQ 12, 44     ; PS/2 Mouse
IRQ 13, 45     ; FPU
IRQ 14, 46     ; Primary ATA
IRQ 15, 47     ; Secondary ATA

; ============================================================
; Common ISR stub
; Saves all registers, calls C handler, restores and irets.
; ============================================================

isr_common_stub:
    pusha                   ; Push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI

    mov ax, ds
    push eax                ; Save data segment selector

    mov ax, 0x10            ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; Push pointer to registers_t struct
    call isr_handler        ; Call C handler
    add esp, 4              ; Clean up pushed pointer

    pop eax                 ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                    ; Restore all general registers
    add esp, 8              ; Remove error code and interrupt number
    iret                    ; Return from interrupt
