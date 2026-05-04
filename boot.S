; Multiboot2 header
section .multiboot
align 8
header_start:
    dd 0xE85250D6                ; magic
    dd 0                         ; architecture (32-bit protected mode)
    dd header_end - header_start ; header length
    dd 0x100000000 - (0xE85250D6 + 0 + (header_end - header_start)) ; checksum
    
    ; End tag (required)
    align 8
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

; Entry point
section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top   ; Set up stack
    push ebx             ; Multiboot info pointer (argument to kernel_main)
    push eax             ; Multiboot magic
    call kernel_main
    
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 0x4000    ; 16 KB stack
stack_top: