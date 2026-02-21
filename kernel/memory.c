#include "memory.h"
#include "string.h"
#include "vga.h"

#define PAGE_SIZE       4096
#define PMM_START       0x100000   /* Start managing from 1MB */
#define PMM_BITMAP_ADDR 0x20000    /* Bitmap stored at 128KB */
#define PMM_MAX_PAGES   4096       /* 16MB / 4KB = 4096 pages */

static uint32_t *pmm_bitmap = (uint32_t *)PMM_BITMAP_ADDR;
static uint32_t  pmm_total_pages = 0;
static uint32_t  pmm_used_pages  = 0;

static void pmm_set_bit(uint32_t page) {
    pmm_bitmap[page / 32] |= (1 << (page % 32));
}

static void pmm_clear_bit(uint32_t page) {
    pmm_bitmap[page / 32] &= ~(1 << (page % 32));
}

static bool pmm_test_bit(uint32_t page) {
    return pmm_bitmap[page / 32] & (1 << (page % 32));
}

void pmm_init(uint32_t mem_size_kb) {
    pmm_total_pages = (mem_size_kb * 1024 - PMM_START) / PAGE_SIZE;
    if (pmm_total_pages > PMM_MAX_PAGES) pmm_total_pages = PMM_MAX_PAGES;

    uint32_t bitmap_size = pmm_total_pages / 8;
    if (pmm_total_pages % 8) bitmap_size++;

    memset(pmm_bitmap, 0, bitmap_size);
    pmm_used_pages = 0;
}

void *pmm_alloc_page(void) {
    for (uint32_t i = 0; i < pmm_total_pages; i++) {
        if (!pmm_test_bit(i)) {
            pmm_set_bit(i);
            pmm_used_pages++;
            return (void *)(PMM_START + i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free_page(void *addr) {
    uint32_t phys = (uint32_t)addr;
    if (phys < PMM_START) return;
    uint32_t page = (phys - PMM_START) / PAGE_SIZE;
    if (page < pmm_total_pages && pmm_test_bit(page)) {
        pmm_clear_bit(page);
        pmm_used_pages--;
    }
}

uint32_t pmm_get_free_pages(void) {
    return pmm_total_pages - pmm_used_pages;
}

uint32_t pmm_get_total_pages(void) {
    return pmm_total_pages;
}

#define PD_ADDR  0x30000
#define PT_ADDR  0x31000

void paging_init(void) {
    uint32_t *page_dir = (uint32_t *)PD_ADDR;
    memset(page_dir, 0, PAGE_SIZE);

    for (int i = 0; i < 4; i++) {
        uint32_t *page_table = (uint32_t *)(PT_ADDR + i * PAGE_SIZE);

        for (int j = 0; j < 1024; j++) {
            uint32_t phys_addr = (i * 1024 + j) * PAGE_SIZE;
            page_table[j] = phys_addr | 0x03;
        }

        page_dir[i] = ((uint32_t)page_table) | 0x03;
    }

    __asm__ volatile(
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(PD_ADDR)
        : "eax"
    );
}

#define EXTRA_PT_BASE 0x35000
static int next_pt_slot = 0;

void paging_map_region(uint32_t virt, uint32_t phys, uint32_t size, uint32_t flags) {
    uint32_t *page_dir = (uint32_t *)PD_ADDR;

    for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
        uint32_t v = virt + offset;
        uint32_t p = phys + offset;
        uint32_t pd_index = v >> 22;
        uint32_t pt_index = (v >> 12) & 0x3FF;

        if (!(page_dir[pd_index] & 0x01)) {
            uint32_t pt_phys = EXTRA_PT_BASE + next_pt_slot * PAGE_SIZE;
            next_pt_slot++;
            memset((void *)pt_phys, 0, PAGE_SIZE);
            page_dir[pd_index] = pt_phys | flags | 0x01;
        }

        uint32_t *page_table = (uint32_t *)(page_dir[pd_index] & 0xFFFFF000);
        page_table[pt_index] = p | flags | 0x01;
    }

    __asm__ volatile("mov %%cr3, %%eax\nmov %%eax, %%cr3" ::: "eax");
}

#define HEAP_START 0x200000
#define HEAP_SIZE  0x200000

typedef struct heap_block {
    uint32_t size;
    bool is_free;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head = NULL;
static uint32_t heap_used = 0;

void heap_init(void) {
    heap_head = (heap_block_t *)HEAP_START;
    heap_head->size = HEAP_SIZE - sizeof(heap_block_t);
    heap_head->is_free = true;
    heap_head->next = NULL;
    heap_used = 0;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = (size + 3) & ~3;

    heap_block_t *curr = heap_head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (curr->size > size + sizeof(heap_block_t) + 16) {
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)curr + sizeof(heap_block_t) + size);
                new_block->size = curr->size - size - sizeof(heap_block_t);
                new_block->is_free = true;
                new_block->next = curr->next;
                curr->next = new_block;
                curr->size = size;
            }
            curr->is_free = false;
            heap_used += curr->size;
            return (void *)((uint8_t *)curr + sizeof(heap_block_t));
        }
        curr = curr->next;
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;

    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));
    block->is_free = true;
    heap_used -= block->size;

    heap_block_t *curr = heap_head;
    while (curr) {
        if (curr->is_free && curr->next && curr->next->is_free) {
            curr->size += sizeof(heap_block_t) + curr->next->size;
            curr->next = curr->next->next;
            continue;
        }
        curr = curr->next;
    }
}

uint32_t heap_get_used(void) {
    return heap_used;
}

uint32_t heap_get_free(void) {
    return HEAP_SIZE - heap_used;
}
