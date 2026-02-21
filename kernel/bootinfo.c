#include "bootinfo.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"

bootinfo_t *bootinfo_get(void) {
    bootinfo_t *binfo = (bootinfo_t *)(uint32_t)BOOTINFO_ADDR;
    if (binfo->magic == BOOTINFO_MAGIC) return binfo;
    return NULL;
}

bool bootinfo_has_framebuffer(void) {
    bootinfo_t *binfo = (bootinfo_t *)(uint32_t)BOOTINFO_ADDR;
    return binfo->magic == BOOTINFO_MAGIC && binfo->vesa_mode == 1;
}

#pragma GCC diagnostic pop
