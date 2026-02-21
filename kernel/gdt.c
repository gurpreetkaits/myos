#include "gdt.h"
#include "string.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_ptr   gdtp;
static struct tss_entry tss;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt[num].base_low    = base & 0xFFFF;
    gdt[num].base_mid    = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access      = access;
}

void gdt_init(void) {
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                    // null
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);     // kernel code
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);     // kernel data
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);     // user code
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);     // user data

    memset(&tss, 0, sizeof(tss));
    tss.ss0 = GDT_KERNEL_DATA;
    tss.esp0 = 0x90000;
    tss.iomap_base = sizeof(tss);

    gdt_set_gate(5, (uint32_t)&tss, sizeof(tss) - 1, 0xE9, 0x00);

    gdt_flush((uint32_t)&gdtp);
    tss_flush();
}

void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}
