#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "types.h"

#define BOOTINFO_ADDR  0x500
#define BOOTINFO_MAGIC 0x4F594D42

typedef struct {
    uint32_t magic;
    uint32_t fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    uint32_t vesa_mode;
} __attribute__((packed)) bootinfo_t;

bootinfo_t *bootinfo_get(void);
bool bootinfo_has_framebuffer(void);

#endif
