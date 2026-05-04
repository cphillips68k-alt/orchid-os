#ifndef PMM_H
#define PMM_H

#include "types.h"
#include "multiboot.h"

void pmm_init(multiboot_info_t *mb);
u32  pmm_alloc_frame(void);
void pmm_free_frame(u32 frame);
u32  pmm_total_mb(void);
u32  pmm_free_mb(void);

#endif