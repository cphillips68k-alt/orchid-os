#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "types.h"

#define MULTIBOOT2_MAGIC 0x36D76289

typedef struct {
    u32 total_size;
    u32 reserved;
} multiboot_fixed_t;

typedef struct {
    u8  type;
    u8  reserved;
    u16 length;
} multiboot_mmap_entry_base_t;

typedef struct {
    u32 size;
    u64 base_addr;
    u64 length;
    u32 type;
} __attribute__((packed)) multiboot_mmap_entry_t;

typedef struct {
    u32 type;
    u32 size;
    u32 entry_size;
    u32 entry_version;
    multiboot_mmap_entry_t entries[];
} __attribute__((packed)) multiboot_mmap_tag_t;

typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
} __attribute__((packed)) multiboot_basic_meminfo_t;

typedef struct {
    multiboot_fixed_t fixed;
    u32 pad;
    multiboot_basic_meminfo_t mem_info;
    u32 pad2;
    multiboot_mmap_tag_t mmap;
} multiboot_info_t;

multiboot_mmap_entry_t *mboot_mmap_first(multiboot_info_t *mb);
multiboot_mmap_entry_t *mboot_mmap_next(multiboot_mmap_tag_t *tag, multiboot_mmap_entry_t *entry);
u32 mboot_get_total_memory(multiboot_info_t *mb);

#endif