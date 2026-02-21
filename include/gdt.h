#ifndef GDT_H
#define GDT_H

#include "types.h"

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

void gdt_init(void);
void tss_set_kernel_stack(uint32_t esp0);

extern void gdt_flush(uint32_t gdt_ptr);
extern void tss_flush(void);

#endif
