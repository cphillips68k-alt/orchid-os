#include "multiboot.h"

multiboot_mmap_entry_t *mboot_mmap_first(multiboot_info_t *mb) {
    return mb->mmap.entries;
}

multiboot_mmap_entry_t *mboot_mmap_next(multiboot_mmap_tag_t *tag, multiboot_mmap_entry_t *entry) {
    multiboot_mmap_entry_t *next = (multiboot_mmap_entry_t *)((u8 *)entry + tag->entry_size);
    u8 *end = (u8 *)tag + tag->size;
    if ((u8 *)next >= end) return 0;
    return next;
}

u32 mboot_get_total_memory(multiboot_info_t *mb) {
    return mb->mem_info.mem_upper + 1024; // mem_upper is in KB above 1MB
}