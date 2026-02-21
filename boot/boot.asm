; boot.asm - Bootloader with VESA framebuffer support
; Loads kernel, sets VESA graphics mode (800x600x32), switches to protected mode.

[bits 16]
[org 0x7C00]

KERNEL_OFFSET  equ 0x1000
KERNEL_SECTORS equ 128       ; ~64 KB for kernel + GUI
BOOTINFO_ADDR  equ 0x500    ; Boot info struct address
BOOTINFO_MAGIC equ 0x4F594D42  ; "BMYO"
VBE_INFO_BUF   equ 0x7E00   ; VBE mode info buffer (512 bytes)

mov [BOOT_DRIVE], dl
mov bp, 0x9000
mov sp, bp

mov si, MSG_BOOT
call print_16
call load_kernel
call setup_vesa
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

    ; Try LBA first
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc .no_lba

    mov si, DAP
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    int 0x13
    jnc .load_done

.no_lba:
    ; Fallback: CHS one sector at a time
    xor ax, ax
    mov es, ax
    mov bx, KERNEL_OFFSET
    mov cx, KERNEL_SECTORS
    mov byte [cur_sect], 2
    mov byte [cur_head], 0
    mov byte [cur_cyl], 0

.read_loop:
    push cx
    push bx

    mov ah, 0x02
    mov al, 1
    mov ch, [cur_cyl]
    mov cl, [cur_sect]
    mov dh, [cur_head]
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error

    pop bx
    pop cx

    add bx, 512
    jnc .no_seg_bump
    ; BX wrapped around - advance ES segment
    mov ax, es
    add ax, 0x1000
    mov es, ax
    xor bx, bx
.no_seg_bump:
    inc byte [cur_sect]
    cmp byte [cur_sect], 64
    jl .read_loop

    ; Wrap to next head
    mov byte [cur_sect], 1
    inc byte [cur_head]
    cmp byte [cur_head], 2
    jl .read_loop
    mov byte [cur_head], 0
    inc byte [cur_cyl]

    loop .read_loop

.load_done:
    ; Reset ES to 0
    xor ax, ax
    mov es, ax
    mov si, MSG_OK
    call print_16
    ret

disk_error:
    mov si, MSG_DISK_ERR
    call print_16
    jmp $

; ============================================================
; VESA Framebuffer Setup
; ============================================================

setup_vesa:
    ; Try mode 0x115 (800x600x32bpp)
    mov cx, 0x115
    call try_vesa_mode
    jnc .vesa_ok

    ; Fallback: mode 0x112 (640x480x32bpp)
    mov cx, 0x112
    call try_vesa_mode
    jnc .vesa_ok

    ; No VESA - write text mode bootinfo
    mov dword [BOOTINFO_ADDR], BOOTINFO_MAGIC
    mov dword [BOOTINFO_ADDR + 4], 0       ; no fb
    mov dword [BOOTINFO_ADDR + 24], 0      ; vesa_mode = 0
    ret

.vesa_ok:
    ret

; Try to set VESA mode in CX. Returns CF=0 on success.
try_vesa_mode:
    push cx
    ; Get mode info into VBE_INFO_BUF
    mov ax, 0x4F01
    mov di, VBE_INFO_BUF
    int 0x10
    cmp ax, 0x004F
    jne .fail

    ; Check mode attributes (bit 0 = supported, bit 7 = LFB)
    test byte [VBE_INFO_BUF], 0x81
    jz .fail

    ; Set the mode (bit 14 = use LFB)
    pop cx
    push cx
    or cx, 0x4000
    mov ax, 0x4F02
    mov bx, cx
    int 0x10
    cmp ax, 0x004F
    jne .fail

    ; Store bootinfo
    mov dword [BOOTINFO_ADDR], BOOTINFO_MAGIC
    ; fb_addr = dword at VBE_INFO_BUF+40
    mov eax, [VBE_INFO_BUF + 40]
    mov [BOOTINFO_ADDR + 4], eax
    ; fb_width = word at VBE_INFO_BUF+18
    movzx eax, word [VBE_INFO_BUF + 18]
    mov [BOOTINFO_ADDR + 8], eax
    ; fb_height = word at VBE_INFO_BUF+20
    movzx eax, word [VBE_INFO_BUF + 20]
    mov [BOOTINFO_ADDR + 12], eax
    ; fb_pitch = word at VBE_INFO_BUF+16
    movzx eax, word [VBE_INFO_BUF + 16]
    mov [BOOTINFO_ADDR + 16], eax
    ; fb_bpp = byte at VBE_INFO_BUF+25
    movzx eax, byte [VBE_INFO_BUF + 25]
    mov [BOOTINFO_ADDR + 20], eax
    ; vesa_mode = 1
    mov dword [BOOTINFO_ADDR + 24], 1

    pop cx
    clc
    ret

.fail:
    pop cx
    stc
    ret

; ============================================================
; GDT + Protected Mode
; ============================================================

DAP:
    db 0x10
    db 0
    dw KERNEL_SECTORS
    dw KERNEL_OFFSET
    dw 0x0000
    dd 1
    dd 0

cur_sect: db 0
cur_head: db 0
cur_cyl:  db 0

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

[bits 16]
BOOT_DRIVE:   db 0
MSG_BOOT:     db "MyOS", 13, 10, 0
MSG_LOAD:     db "Loading...", 0
MSG_OK:       db " OK", 13, 10, 0
MSG_DISK_ERR: db " ERR!", 0

times 510 - ($ - $$) db 0
dw 0xAA55
