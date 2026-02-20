; boot.asm - Bootloader
; Loads kernel using single-sector CHS reads in a loop (most robust).
; Switches to 32-bit protected mode, jumps to kernel.

[bits 16]
[org 0x7C00]

KERNEL_OFFSET equ 0x1000
KERNEL_SECTORS equ 48     ; ~24 KB, plenty for our kernel

mov [BOOT_DRIVE], dl
mov bp, 0x9000
mov sp, bp

mov si, MSG_BOOT
call print_16
call load_kernel
call switch_to_pm
jmp $

; ============================================================
; 16-bit routines
; ============================================================

print_16:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

load_kernel:
    mov si, MSG_LOAD
    call print_16

    ; Use INT 13h extensions (LBA) - most reliable for hard drives
    ; First check if extensions are supported
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc .no_lba          ; Extensions not supported

    ; Extensions supported - use LBA read
    mov si, DAP
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    int 0x13
    jnc .load_done      ; Success

.no_lba:
    ; Fallback: read one sector at a time using CHS
    ; Works universally regardless of drive geometry
    xor ax, ax
    mov es, ax
    mov bx, KERNEL_OFFSET
    mov cx, KERNEL_SECTORS   ; loop counter
    mov byte [cur_sect], 2   ; start from CHS sector 2

.read_loop:
    push cx
    push bx

    mov ah, 0x02         ; BIOS read
    mov al, 1            ; 1 sector
    mov ch, 0            ; cylinder 0
    mov cl, [cur_sect]   ; sector number
    mov dh, 0            ; head 0
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error

    pop bx
    pop cx

    add bx, 512          ; advance buffer
    inc byte [cur_sect]  ; next sector

    ; If sector > 63 (max for most drives), wrap to next head
    cmp byte [cur_sect], 64
    jl .no_wrap
    mov byte [cur_sect], 1
    ; Could advance head/cylinder here, but our kernel fits in <63 sectors
.no_wrap:

    loop .read_loop

.load_done:
    mov si, MSG_OK
    call print_16
    ret

disk_error:
    mov si, MSG_DISK_ERR
    call print_16
    jmp $

; Disk Address Packet for INT 13h LBA extensions
DAP:
    db 0x10              ; Size of packet
    db 0                 ; Reserved
    dw KERNEL_SECTORS    ; Sectors to read
    dw KERNEL_OFFSET     ; Offset
    dw 0x0000            ; Segment
    dd 1                 ; LBA start (sector after boot)
    dd 0                 ; Upper LBA

cur_sect: db 0

; ============================================================
; GDT + Protected Mode
; ============================================================

gdt_start:
    dd 0x0, 0x0

gdt_code:
    dw 0xFFFF, 0x0
    db 0x0, 10011010b, 11001111b, 0x0

gdt_data:
    dw 0xFFFF, 0x0
    db 0x0, 10010010b, 11001111b, 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x90000
    mov esp, ebp
    call KERNEL_OFFSET
    jmp $

; ============================================================
; Data
; ============================================================

BOOT_DRIVE:   db 0
MSG_BOOT:     db "MyOS Bootloader", 13, 10, 0
MSG_LOAD:     db "Loading kernel...", 0
MSG_OK:       db " OK", 13, 10, 0
MSG_DISK_ERR: db " DISK ERROR!", 0

times 510 - ($ - $$) db 0
dw 0xAA55
