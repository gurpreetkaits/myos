#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

/* Physical memory manager (bitmap-based, 4KB pages) */
void pmm_init(uint32_t mem_size_kb);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
uint32_t pmm_get_free_pages(void);
uint32_t pmm_get_total_pages(void);

/* Paging */
void paging_init(void);

/* Heap allocator (first-fit linked list) */
void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
uint32_t heap_get_used(void);
uint32_t heap_get_free(void);

#endif
