#include "pmm.h"
#include "lib.h"
#include "printf.h"

#define PMM_STACK_SIZE 0x8000  // Can track up to 32K frames
#define FRAME_SIZE 4096

static u32 pmm_stack[PMM_STACK_SIZE];
static u32 pmm_stack_top;
static u32 total_frames;
static u32 free_frames;

void pmm_init(multiboot_info_t *mb) {
    multiboot_mmap_tag_t *mmap = &mb->mmap;
    multiboot_mmap_entry_t *entry = mboot_mmap_first(mb);

    pmm_stack_top = 0;
    total_frames = 0;
    free_frames = 0;

    while (entry) {
        if (entry->type == 1) { // Available memory
            u64 base = entry->base_addr;
            u64 length = entry->length;

            // Align base up to page boundary
            u32 start_frame = (base + FRAME_SIZE - 1) / FRAME_SIZE;
            u32 end_frame   = (base + length) / FRAME_SIZE;

            // Don't use memory below 1 MB (hardware reserved area)
            if (start_frame < 256) start_frame = 256;
            // Don't use first 4 MB (our kernel lives there)
            if (start_frame < 1024) start_frame = 1024;

            for (u32 i = start_frame; i < end_frame && pmm_stack_top < PMM_STACK_SIZE; i++) {
                pmm_stack[pmm_stack_top++] = i;
                free_frames++;
            }
            total_frames += (end_frame - start_frame);
        }
        entry = mboot_mmap_next(mmap, entry);
    }

    printf("[PMM] %u frames available (%u MB)\n", free_frames, free_frames * 4 / 1024);
}

u32 pmm_alloc_frame(void) {
    if (pmm_stack_top == 0) {
        printf("[PMM] FATAL: Out of memory!\n");
        asm volatile("cli; hlt");
        return 0;
    }
    free_frames--;
    return pmm_stack[--pmm_stack_top] * FRAME_SIZE;
}

void pmm_free_frame(u32 frame) {
    if (pmm_stack_top >= PMM_STACK_SIZE) {
        printf("[PMM] FATAL: PMM stack overflow!\n");
        return;
    }
    pmm_stack[pmm_stack_top++] = frame / FRAME_SIZE;
    free_frames++;
}

u32 pmm_total_mb(void) {
    return total_frames * 4 / 1024;
}

u32 pmm_free_mb(void) {
    return free_frames * 4 / 1024;
}