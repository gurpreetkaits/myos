#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

void pmm_init(uint32_t mem_size_kb);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
uint32_t pmm_get_free_pages(void);
uint32_t pmm_get_total_pages(void);

void paging_init(void);
void paging_map_region(uint32_t virt, uint32_t phys, uint32_t size, uint32_t flags);

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
uint32_t heap_get_used(void);
uint32_t heap_get_free(void);

#endif
