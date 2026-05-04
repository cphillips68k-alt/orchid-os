# boot.S — AT&T syntax for Clang IAS

.section .multiboot
.align 8
header_start:
    .long 0xE85250D6                    # magic
    .long 0                             # arch (32-bit)
    .long (header_end - header_start)   # header length
    .long 0x100000000 - (0xE85250D6 + 0 + (header_end - header_start))  # checksum
    
    .align 8
    .short 0    # end tag type
    .short 0    # flags
    .long  8    # size
header_end:

.section .text
.globl _start
.extern kernel_main

_start:
    movl $stack_top, %esp
    pushl %ebx
    pushl %eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

.section .bss
.align 16
stack_bottom:
    .space 0x4000
stack_top: