/* Linker script for MyOS kernel
 * Loaded at 0x1000 by the bootloader.
 * kernel_entry.o MUST be linked first (it's the entry point).
 */

ENTRY(_start)

SECTIONS {
    . = 0x1000;

    .text : {
        /* kernel_entry.o must come first */
        kernel/kernel_entry.o(.text)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}
