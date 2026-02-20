; kernel_entry.asm - First code in the kernel binary
; Placed at 0x1000 by the linker, called by the bootloader.

[bits 32]
[extern main]

global _start
_start:
    call main
    ; If main returns, halt forever
.hang:
    cli
    hlt
    jmp .hang
