ENTRY(_start)

SECTIONS {
    . = 1M;   /* Load at 1MB (standard for GRUB) */
    
    .multiboot : ALIGN(4) {
        KEEP(*(.multiboot))
    }
    
    .text : ALIGN(4K) {
        *(.text)
    }
    
    .rodata : ALIGN(4K) {
        *(.rodata)
    }
    
    .data : ALIGN(4K) {
        *(.data)
    }
    
    .bss : ALIGN(4K) {
        *(.bss)
    }
}