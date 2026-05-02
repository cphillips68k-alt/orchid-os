; kernel.asm - The Actual Operating System
; 
; Three things:
;   1. Interrupting (PIT timer via 8259 PIC)
;   2. A scheduler that handles loops (round-robin preemption)
;   3. Ring 0 / Ring 3 separation (GDT + TSS + syscalls)
;
; Runs in 64-bit long mode. Jumps here from bootloader at 0x10000.

BITS 64
ORG 0x10000

; ============================================================================
; ENTRY POINT
; ============================================================================
kernel_main:
    ; We're in 32-bit protected mode from the bootloader.
    ; Step 1: Switch to 64-bit long mode.
    
    ; Clear the screen (write to VGA text buffer at 0xB8000)
    mov edi, 0xB8000
    mov ecx, 80 * 25
    mov ax, 0x0F00          ; White on black, null character
    rep stosw
    
    ; Print welcome message
    mov esi, msg_welcome
    call vga_print
    
    ; Step 2: Set up 64-bit GDT
    lgdt [gdt64_ptr]
    
    ; Step 3: Enable long mode
    mov eax, cr4
    or eax, 1 << 5          ; Set PAE bit
    mov cr4, eax
    
    mov ecx, 0xC0000080     ; EFER MSR
    rdmsr
    or eax, 1 << 8          ; Set LME (Long Mode Enable)
    wrmsr
    
    mov eax, cr0
    or eax, 1 << 31         ; Set PG (Paging)
    mov cr0, eax
    
    ; Now in compatibility mode. Far jump to 64-bit code.
    jmp 0x08:long_mode_start

; ============================================================================
; 64-BIT LONG MODE
; ============================================================================
long_mode_start:
    BITS 64
    
    ; Set up segment registers
    mov ax, 0x10            ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up kernel stack
    mov rsp, kernel_stack_top
    
    ; Print that we made it to 64-bit mode
    mov esi, msg_long_mode
    call vga_print
    
    ; Step 4: Set up interrupts
    call init_idt
    call init_pic
    call init_pit
    
    ; Step 5: Set up TSS for ring transitions
    call init_tss
    
    ; Step 6: Set up scheduler tasks
    call init_scheduler
    
    ; Step 7: Enable interrupts
    sti
    
    ; Step 8: Jump to first task
    jmp user_task_1

; ============================================================================
; IDT (Interrupt Descriptor Table)
; ============================================================================
init_idt:
    ; We'll set up 256 entries, but only use a few.
    ; Timer interrupt: vector 32 (IRQ 0)
    ; Keyboard: vector 33 (IRQ 1)
    ; Syscall: vector 0x80
    
    mov rax, timer_handler
    mov rbx, 32
    call set_idt_entry
    
    mov rax, syscall_handler
    mov rbx, 0x80
    call set_idt_entry
    
    lidt [idt_ptr]
    ret

set_idt_entry:
    ; rax = handler address
    ; rbx = interrupt number
    ; Set up 64-bit interrupt gate
    push rbx
    imul rbx, 16             ; 16 bytes per entry
    add rbx, idt
    
    mov word [rbx], ax       ; Handler low 16 bits
    mov word [rbx + 2], 0x08 ; Code segment selector
    mov byte [rbx + 4], 0    ; IST + reserved
    mov byte [rbx + 5], 0x8E ; Present, DPL=0, Interrupt Gate
    shr rax, 16
    mov word [rbx + 6], ax   ; Handler middle 16 bits
    shr rax, 16
    mov dword [rbx + 8], eax ; Handler high 32 bits
    mov dword [rbx + 12], 0  ; Reserved
    
    pop rbx
    ret

; ============================================================================
; PIC (Programmable Interrupt Controller)
; ============================================================================
init_pic:
    ; Remap 8259 PIC so IRQ 0-15 map to IDT entries 32-47
    ; This avoids conflict with CPU exceptions (0-31)
    
    mov al, 0x11            ; ICW1: Initialize, ICW4 needed
    out 0x20, al
    out 0xA0, al
    
    mov al, 32              ; ICW2: Base vector for master
    out 0x21, al
    mov al, 40              ; ICW2: Base vector for slave
    out 0xA1, al
    
    mov al, 0x04            ; ICW3: Master has slave on IRQ2
    out 0x21, al
    mov al, 0x02            ; ICW3: Slave identity
    out 0xA1, al
    
    mov al, 0x01            ; ICW4: 8086 mode
    out 0x21, al
    out 0xA1, al
    
    ; Mask all interrupts except IRQ 0 (timer)
    mov al, 0xFE            ; 1111 1110 - only IRQ0 enabled
    out 0x21, al
    mov al, 0xFF            ; Mask all slave interrupts
    out 0xA1, al
    
    ret

; ============================================================================
; PIT (Programmable Interval Timer)
; ============================================================================
init_pit:
    ; Set PIT to fire at ~100Hz
    ; Divisor = 1193180 / frequency
    ; 1193180 / 100 = 11931 (approx)
    mov al, 0x36            ; Channel 0, lobyte/hibyte, square wave
    out 0x43, al
    
    mov ax, 11931           ; Divisor for ~100Hz
    out 0x40, al            ; Low byte
    shr ax, 8
    out 0x40, al            ; High byte
    ret

; ============================================================================
; TSS (Task State Segment) - Required for ring transitions
; ============================================================================
init_tss:
    ; We need at minimum a TSS with RSP0 (kernel stack pointer for ring 0)
    mov dword [tss + 4], kernel_stack_top  ; RSP0 low 32 bits
    ; Upper 32 bits are already 0 (we're in BSS)
    
    ; Load TSS into GDT
    mov rax, tss
    mov word [gdt64 + 0x28], 103      ; Limit (size - 1)
    mov word [gdt64 + 0x28 + 2], ax   ; Base low 16
    shr rax, 16
    mov byte [gdt64 + 0x28 + 4], al   ; Base middle 8
    mov byte [gdt64 + 0x28 + 5], 0x89 ; Present, 64-bit TSS available
    mov byte [gdt64 + 0x28 + 6], 0x00 ; Flags + limit high
    mov byte [gdt64 + 0x28 + 7], ah   ; Base high 8
    shr rax, 16
    mov dword [gdt64 + 0x28 + 8], eax ; Base upper 32
    
    mov ax, 0x2B            ; TSS selector (0x28 | 0x03)
    ltr ax
    ret

; ============================================================================
; SCHEDULER
; ============================================================================
init_scheduler:
    ; Just two tasks for now. They'll print to screen.
    ; Each task has: saved RIP, saved RSP, state
    mov rax, user_task_1
    mov [task1_rip], rax
    mov rax, user_stack_1_top
    mov [task1_rsp], rax
    
    mov rax, user_task_2
    mov [task2_rip], rax
    mov rax, user_stack_2_top
    mov [task2_rsp], rax
    
    mov byte [current_task], 0    ; Start with task 1
    ret

; ============================================================================
; TIMER INTERRUPT HANDLER
; ============================================================================
timer_handler:
    ; Save all registers (they're caller-saved by interrupt gate)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Save current task's state
    mov al, [current_task]
    cmp al, 0
    je .save_task1
    
    ; Save task 2
    mov [task2_rip], rsp
    add rsp, 15 * 8          ; Restore stack to where RIP is saved
    ; Actually, let's be explicit:
    mov rax, [rsp + 15 * 8]  ; Original RIP (pushed by CPU)
    mov [task2_rip], rax
    mov [task2_rsp], rsp
    jmp .switch_task
    
.save_task1:
    mov rax, [rsp + 15 * 8]
    mov [task1_rip], rax
    mov [task1_rsp], rsp
    
.switch_task:
    ; Toggle current task
    xor byte [current_task], 1
    
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    
    ; Restore next task's state
    mov al, [current_task]
    cmp al, 0
    je .restore_task1
    
    ; Restore task 2
    mov rsp, [task2_rsp]
    jmp .done_restore
    
.restore_task1:
    mov rsp, [task1_rsp]
    
.done_restore:
    ; Pop saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    iretq

; ============================================================================
; SYSCALL HANDLER
; ============================================================================
syscall_handler:
    ; Simple syscall interface:
    ; rax = syscall number
    ; Currently: 1 = print character (rdi = character)
    
    cmp rax, 1
    je .sys_print
    
    iretq

.sys_print:
    mov rsi, rdi            ; Character to print
    call vga_putchar
    iretq

; ============================================================================
; USER TASKS (Ring 3)
; ============================================================================
user_task_1:
    ; This runs in Ring 3!
    mov esi, msg_task1
    call sys_print_string   ; Custom "syscall" to print
.user_loop_1:
    nop
    jmp .user_loop_1

user_task_2:
    mov esi, msg_task2
    call sys_print_string
.user_loop_2:
    nop
    nop
    jmp .user_loop_2

; Helper: print string via syscalls
sys_print_string:
    push rsi
.loop:
    lodsb
    test al, al
    jz .done
    mov rdi, rax
    mov rax, 1              ; Syscall 1: print char
    int 0x80
    jmp .loop
.done:
    pop rsi
    ret

; ============================================================================
; VGA TEXT OUTPUT (Ring 0 utility)
; ============================================================================
vga_print:
    ; Print string at ESI to VGA text mode buffer
    push rax
    push rdi
    mov edi, [vga_cursor]
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0F            ; White on black
    mov [0xB8000 + edi], ax
    add edi, 2
    jmp .loop
.done:
    mov [vga_cursor], edi
    pop rdi
    pop rax
    ret

vga_putchar:
    ; Print single character in RSI
    push rdi
    mov edi, [vga_cursor]
    mov eax, esi
    mov ah, 0x0F
    mov [0xB8000 + edi], ax
    add edi, 2
    mov [vga_cursor], edi
    pop rdi
    ret

; ============================================================================
; DATA
; ============================================================================
msg_welcome db 'MyOS: Booting...', 0
msg_long_mode db ' [64-bit mode]', 0
msg_task1 db ' [Task 1 running]', 0
msg_task2 db ' [Task 2 running]', 0

vga_cursor dd 0

; ============================================================================
; GDT (64-bit)
; ============================================================================
gdt64:
    dq 0                    ; Null descriptor
    dq 0x0020980000000000   ; Code: 64-bit, ring 0
    dq 0x0000920000000000   ; Data: ring 0
    dq 0x0020F80000000000   ; User code: 64-bit, ring 3
    dq 0x0000F20000000000   ; User data: ring 3
    ; TSS descriptor (filled by init_tss)
    dq 0
    dq 0

gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

; ============================================================================
; IDT (Interrupt Descriptor Table)
; ============================================================================
idt:
    times 256 dq 0, 0       ; 256 entries, 16 bytes each

idt_ptr:
    dw 256 * 16 - 1
    dq idt

; ============================================================================
; TSS (Task State Segment)
; ============================================================================
tss:
    times 104 db 0          ; Minimum TSS size

; ============================================================================
; SCHEDULER STATE
; ============================================================================
current_task db 0
task1_rip dq 0
task1_rsp dq 0
task2_rip dq 0
task2_rsp dq 0

; ============================================================================
; STACKS
; ============================================================================
; Kernel stack
align 16
kernel_stack_bottom:
    times 4096 db 0
kernel_stack_top:

; User stacks for Ring 3 tasks
align 16
user_stack_1_bottom:
    times 4096 db 0
user_stack_1_top:

user_stack_2_bottom:
    times 4096 db 0
user_stack_2_top: