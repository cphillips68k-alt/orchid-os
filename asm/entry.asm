; entry.asm - Bootstrap and interrupt stubs for Orchid OS
BITS 32
section .text

global start
global reload_segments
global timer_stub, keyboard_stub, syscall_stub

extern kernel_main
extern timer_handler, keyboard_handler, syscall_handler

start:
    mov esp, 0x90000        ; initial kernel stack
    call kernel_main
    cli
    hlt
    jmp $

; Reload segment registers after loading a new GDT
reload_segments:
    jmp 0x08:flush
flush:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

; Timer interrupt (IRQ0) - scheduler entry
timer_stub:
    pushad
    push esp                ; argument: current kernel stack pointer
    call timer_handler      ; returns new kernel stack pointer in eax
    add esp, 4
    mov esp, eax            ; switch to the other task's kernel stack
    ; send EOI
    push eax
    mov al, 0x20
    out 0x20, al
    pop eax
    popad
    iret

; Keyboard interrupt (IRQ1)
keyboard_stub:
    pushad
    call keyboard_handler
    mov al, 0x20
    out 0x20, al
    popad
    iret

; Syscall (int 0x80)
syscall_stub:
    pushad
    push esp                ; pointer to saved registers
    call syscall_handler
    add esp, 4
    popad
    iret