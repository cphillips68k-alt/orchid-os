#include "types.h"
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "paging.h"
#include "pmm.h"
#include "printf.h"
#include "multiboot.h"
#include "shell.h"
#include "ata.h"
#include "fs.h"

static void hang(void) {
    asm volatile("cli; hlt");
}

void kernel_main(u32 magic, multiboot_info_t *mb_info) {
    vga_clear();
    vga_set_color(VGA_WHITE, VGA_BLACK);
    
    printf("Orchid OS 0.0.1\n\n");
    
    if (magic != MULTIBOOT2_MAGIC) {
        printf("[FATAL] Invalid bootloader magic: %x\n", magic);
        hang();
    }
    
    printf("[INIT] Setting up GDT... ");
    gdt_init();
    printf("ok\n");
    
    printf("[INIT] Setting up IDT... ");
    idt_init();
    printf("ok\n");
    
    printf("[INIT] Remapping PIC... ");
    pic_init();
    printf("ok\n");
    
    printf("[INIT] Initializing paging... ");
    paging_init();
    printf("ok\n");
    
    printf("[INIT] Initializing PMM... ");
    pmm_init(mb_info);
        printf("[INIT] Initializing ATA... ");
    if (ata_init()) {
        printf("ok\n");
        printf("[INIT] Initializing FAT32... ");
        if (fs_init()) {
            printf("ok\n");
        } else {
            printf("no filesystem\n");
        }
    } else {
        printf("no disk\n");
    }
    printf("ok\n");
    
    printf("\n[READY] Orchid OS booted successfully\n");
    printf("[MEM]  Total: %u MB  Free: %u MB\n",
           pmm_total_mb(), pmm_free_mb());
    
    printf("\nEnabling interrupts... ");
    asm volatile("sti");
    printf("done\n");
    
    printf("\nStarting shell...\n\n");
    shell_init();
    shell_prompt();
    asm volatile("sti");

    // Idle loop — shell runs off keyboard interrupts
    while (1) {
        asm volatile("hlt");
    }
}