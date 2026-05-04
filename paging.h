#ifndef PAGING_H
#define PAGING_H

#include "types.h"

#define PAGE_SIZE 4096

void paging_init(void);
void paging_map_page(u32 virt, u32 phys, u32 flags);
void paging_enable(void);

// Page flags
#define PAGE_PRESENT  0x1
#define PAGE_RW       0x2
#define PAGE_USER     0x4
#define PAGE_WRITETHROUGH 0x8
#define PAGE_CACHE_DISABLE 0x10

#endif