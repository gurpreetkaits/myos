# MyOS Makefile
CC  = x86_64-elf-gcc
LD  = x86_64-elf-ld
ASM = nasm

CFLAGS  = -m32 -ffreestanding -fno-pie -nostdlib -nostdinc -I include -Wall -Wextra -Os
LDFLAGS = -m elf_i386 -T linker.ld --oformat binary

# Source files
C_SOURCES  = $(wildcard kernel/*.c)
C_OBJECTS  = $(C_SOURCES:.c=.o)

# Kernel assembly (ELF objects, not flat binary)
ASM_KERNEL = kernel/kernel_entry.asm kernel/interrupt.asm kernel/switch.asm kernel/gdt_flush.asm
ASM_OBJECTS = $(ASM_KERNEL:.asm=.o)

# The entry object MUST be first for the linker
ENTRY_OBJ = kernel/kernel_entry.o
OTHER_ASM  = $(filter-out $(ENTRY_OBJ), $(ASM_OBJECTS))
ALL_OBJECTS = $(ENTRY_OBJ) $(OTHER_ASM) $(C_OBJECTS)

# Output
BOOT_BIN   = boot/boot.bin
KERNEL_BIN = kernel/kernel.bin
OS_IMAGE   = myos.img
FAT_IMAGE  = fat.img

all: $(OS_IMAGE)

# Final disk image: boot sector + kernel binary
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $(OS_IMAGE)
	@# Pad to 1.44MB floppy size for proper BIOS geometry detection
	dd if=/dev/null of=$(OS_IMAGE) bs=512 seek=2880 2>/dev/null

# Bootloader (flat binary)
$(BOOT_BIN): boot/boot.asm
	$(ASM) -f bin $< -o $@

# Link kernel objects into flat binary
$(KERNEL_BIN): $(ALL_OBJECTS)
	$(LD) $(LDFLAGS) $^ -o $@

# Compile C → object
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble kernel ASM → ELF object
kernel/%.o: kernel/%.asm
	$(ASM) -f elf32 $< -o $@

# ============================================================
# FAT16 test disk image (requires mtools)
# ============================================================
fat: $(FAT_IMAGE)

$(FAT_IMAGE):
	dd if=/dev/zero of=$(FAT_IMAGE) bs=512 count=2048
	mformat -i $(FAT_IMAGE) -f 1440 ::
	@echo "Hello from MyOS FAT16 disk!" > /tmp/hello.txt
	@echo "This is a test file on a FAT filesystem." >> /tmp/hello.txt
	mcopy -i $(FAT_IMAGE) /tmp/hello.txt ::HELLO.TXT
	@echo "MyOS v0.2.0" > /tmp/readme.txt
	@echo "A hobby operating system built from scratch." >> /tmp/readme.txt
	@echo "Features: IDT, keyboard, memory management," >> /tmp/readme.txt
	@echo "shell, FAT16 filesystem, multitasking." >> /tmp/readme.txt
	mcopy -i $(FAT_IMAGE) /tmp/readme.txt ::README.TXT
	@echo "FAT16 disk image created: $(FAT_IMAGE)"

# ============================================================
# Run targets
# ============================================================

# Run with just the OS disk (VESA graphics mode)
run: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE) \
		-device VGA -m 32 -monitor stdio

# Run with OS disk + FAT16 data disk
run-fat: $(OS_IMAGE) $(FAT_IMAGE)
	qemu-system-i386 \
		-drive format=raw,file=$(OS_IMAGE),index=0,if=ide \
		-drive format=raw,file=$(FAT_IMAGE),index=1,if=ide \
		-device VGA -m 32 -monitor stdio

# Run in text mode (no VESA, fallback to shell)
run-text: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE) \
		-device cirrus-vga -m 32 -monitor stdio

# Run with no graphics (serial console)
run-debug: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE) -nographic -serial mon:stdio

clean:
	rm -f $(BOOT_BIN) $(KERNEL_BIN) $(OS_IMAGE) $(C_OBJECTS) $(ASM_OBJECTS)

clean-all: clean
	rm -f $(FAT_IMAGE)

.PHONY: all fat run run-fat run-text run-debug clean clean-all
