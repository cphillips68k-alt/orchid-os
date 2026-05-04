#include "paging.h"
#include "lib.h"

// Page directory (1024 entries) and first page table
// Must be page-aligned — we'll use the linker to ensure this
static u32 page_directory[1024] __attribute__((aligned(4096)));
static u32 first_page_table[1024] __attribute__((aligned(4096)));

extern void paging_load_directory(u32 dir_addr);
extern void paging_enable_paging(void);

void paging_init(void) {
    // Identity-map the first 4 MB
    u32 i;
    for (i = 0; i < 1024; i++) {
        first_page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_RW;
    }

    // Clear page directory
    memset(page_directory, 0, sizeof(page_directory));

    // First entry points to first page table
    page_directory[0] = (u32)first_page_table | PAGE_PRESENT | PAGE_RW;

    // Load page directory and enable paging
    paging_load_directory((u32)page_directory);
    paging_enable_paging();
}

void paging_map_page(u32 virt, u32 phys, u32 flags) {
    u32 pd_index = virt >> 22;
    u32 pt_index = (virt >> 12) & 0x3FF;

    // Only handle first 4 MB identity-mapped region for now
    if (pd_index == 0) {
        first_page_table[pt_index] = (phys & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;
        asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    }
}