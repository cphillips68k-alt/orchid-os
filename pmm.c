#include "pmm.h"
#include "lib.h"
#include "printf.h"

#define PMM_STACK_SIZE 0x8000
#define FRAME_SIZE 4096

static u32 pmm_stack[PMM_STACK_SIZE];
static u32 pmm_stack_top;
static u32 total_frames;
static u32 free_frames;

void pmm_init(multiboot_info_t *mb) {
    // Use basic meminfo (always reliable, no mmap parsing)
    u32 mem_upper_kb = mb->mem_info.mem_upper;
    u32 mem_kb = mem_upper_kb + 1024; // + first 1MB (mem_lower is below 1MB, we count from 0)
    u32 total = mem_kb;
    
    total_frames = total / 4; // 4 KB per frame
    free_frames  = 0;
    pmm_stack_top = 0;
    
    // Mark first 8 MB as used (kernel space)
    u32 kernel_end_frame = (8 * 1024 * 1024) / FRAME_SIZE;
    
    // Stack free frames (from kernel end to top of memory)
    u32 max_frame = total_frames;
    if (max_frame > PMM_STACK_SIZE + kernel_end_frame) {
        max_frame = PMM_STACK_SIZE + kernel_end_frame;
    }
    
    for (u32 i = max_frame - 1; i >= kernel_end_frame; i--) {
        if (pmm_stack_top < PMM_STACK_SIZE) {
            pmm_stack[pmm_stack_top++] = i;
            free_frames++;
        }
    }
    
    printf("[PMM] %u frames available (%u KB)\n", free_frames, free_frames * 4);
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