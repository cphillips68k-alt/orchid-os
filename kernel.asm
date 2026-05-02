; kernel.asm - The Actual Operating System
; 
; Three things:
;   1. Interrupting (PIT timer via 8259 PIC)
;   2. A scheduler that handles loops (round-robin preemption)
;   3. Ring 0 / Ring 3 separation (GDT + TSS + syscalls)
;
; Enters from bootloader in 32-bit protected mode at 0x10000.

ORG 0x10000

; ============================================================================
; 32-BIT ENTRY POINT
; ============================================================================
BITS 32
kernel_main:
    ; We arrive from the bootloader in 32-bit protected mode.
    ; Our job: set up paging, switch to 64-bit long mode.
    
    ; Clear the screen (VGA text buffer at 0xB8000)
    mov edi, 0xB8000
    mov ecx, 80 * 25
    mov ax, 0x0F00          ; White on black, null character
    rep stosw
    
    ; Print welcome
    mov esi, msg_welcome
    call vga_print_32
    
    ; Set up basic paging for long mode
    call setup_paging
    
    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    ; Set LME (Long Mode Enable) in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    ; Load 64-bit GDT
    lgdt [gdt64_ptr]
    
    ; Far jump to 64-bit code
    jmp 0x08:long_mode_start

; ============================================================================
; Minimal Paging Setup
; Identity map first 2MB for simplicity
; ============================================================================
setup_paging:
    ; PML4 at 0x9000
    mov edi, 0x9000
    mov cr3, edi
    
    ; Zero out page tables
    mov ecx, 4096 * 3
    xor eax, eax
    rep stosb
    
    ; PML4[0] -> PDP at 0xA000
    mov edi, 0x9000
    mov dword [edi], 0xA003    ; Present + Write
    
    ; PDP[0] -> Page Directory at 0xB000  
    mov edi, 0xA000
    mov dword [edi], 0xB003    ; Present + Write
    
    ; PD[0] -> 2MB page at 0x0
    mov edi, 0xB000
    mov dword [edi], 0x00000083 ; Huge page, Present + Write
    
    ret

; ============================================================================
; 32-bit VGA print
; ============================================================================
vga_print_32:
    push eax
    push edi
    mov edi, [vga_cursor]
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0F
    mov [0xB8000 + edi], ax
    add edi, 2
    jmp .loop
.done:
    mov [vga_cursor], edi
    pop edi
    pop eax
    ret

; ============================================================================
; 64-BIT LONG MODE
; ============================================================================
BITS 64
long_mode_start:
    ; Set up segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up kernel stack
    mov rsp, kernel_stack_top
    
    ; Print success
    mov esi, msg_long_mode
    call vga_print_64
    
    ; Set up interrupts
    call init_idt
    call init_pic
    call init_pit
    
    ; Set up TSS for ring transitions
    call init_tss
    
    ; Set up scheduler
    call init_scheduler
    
    ; Enable interrupts
    sti
    
    ; Jump to first user task
    ; Need to set up stack for iretq to ring 3
    push 0x23              ; SS (user data segment)
    push user_stack_1_top   ; RSP
    push 0x200             ; RFLAGS (interrupts enabled)
    push 0x1B              ; CS (user code segment)
    push user_task_1        ; RIP
    iretq

; ============================================================================
; IDT (Interrupt Descriptor Table)
; ============================================================================
init_idt:
    mov rax, timer_handler
    mov rbx, 32
    call set_idt_entry
    
    mov rax, syscall_handler
    mov rbx, 0x80
    call set_idt_entry
    
    lidt [idt_ptr]
    ret

set_idt_entry:
    push rbx
    imul rbx, 16
    add rbx, idt
    
    mov word [rbx], ax
    mov word [rbx + 2], 0x08
    mov byte [rbx + 4], 0
    mov byte [rbx + 5], 0x8E
    shr rax, 16
    mov word [rbx + 6], ax
    shr rax, 16
    mov dword [rbx + 8], eax
    mov dword [rbx + 12], 0
    
    pop rbx
    ret

; ============================================================================
; PIC (Programmable Interrupt Controller)
; ============================================================================
init_pic:
    mov al, 0x11
    out 0x20, al
    out 0xA0, al
    
    mov al, 32
    out 0x21, al
    mov al, 40
    out 0xA1, al
    
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al
    
    mov al, 0x01
    out 0x21, al
    out 0xA1, al
    
    mov al, 0xFE
    out 0x21, al
    mov al, 0xFF
    out 0xA1, al
    
    ret

; ============================================================================
; PIT (Programmable Interval Timer)
; ============================================================================
init_pit:
    mov al, 0x36
    out 0x43, al
    
    mov ax, 11931
    out 0x40, al
    shr ax, 8
    out 0x40, al
    ret

; ============================================================================
; TSS (Task State Segment)
; ============================================================================
init_tss:
    mov qword [tss + 4], kernel_stack_top
    
    mov rax, tss
    mov word [gdt64 + 0x28], 103
    mov word [gdt64 + 0x28 + 2], ax
    shr rax, 16
    mov byte [gdt64 + 0x28 + 4], al
    mov byte [gdt64 + 0x28 + 5], 0x89
    mov byte [gdt64 + 0x28 + 6], 0x00
    mov byte [gdt64 + 0x28 + 7], ah
    shr rax, 16
    mov dword [gdt64 + 0x28 + 8], eax
    
    mov ax, 0x2B
    ltr ax
    ret

; ============================================================================
; SCHEDULER
; ============================================================================
init_scheduler:
    mov byte [current_task], 0
    ret

; ============================================================================
; TIMER INTERRUPT HANDLER
; ============================================================================
timer_handler:
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
    
    ; Toggle task indicator
    xor byte [current_task], 1
    
    ; Simple visual feedback - change character at position
    mov al, [current_task]
    test al, al
    jz .show1
    mov byte [0xB8000 + 160], '2'
    jmp .done_display
.show1:
    mov byte [0xB8000 + 160], '1'
.done_display:
    
    ; Send EOI
    mov al, 0x20
    out 0x20, al
    
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
    cmp rax, 1
    je .sys_print
    iretq

.sys_print:
    mov rsi, rdi
    call vga_putchar_64
    iretq

; ============================================================================
; USER TASKS (Ring 3)
; ============================================================================
user_task_1:
    mov esi, msg_task1
    call sys_print_string
.loop:
    nop
    jmp .loop

user_task_2:
    mov esi, msg_task2
    call sys_print_string
.loop:
    nop
    jmp .loop

sys_print_string:
    push rsi
.print_loop:
    lodsb
    test al, al
    jz .done
    mov rdi, rax
    mov rax, 1
    int 0x80
    jmp .print_loop
.done:
    pop rsi
    ret

; ============================================================================
; VGA TEXT OUTPUT (64-bit)
; ============================================================================
vga_print_64:
    push rax
    push rdi
    mov edi, [vga_cursor]
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0F
    mov [0xB8000 + rdi], ax
    add rdi, 2
    jmp .loop
.done:
    mov [vga_cursor], edi
    pop rdi
    pop rax
    ret

vga_putchar_64:
    push rdi
    mov edi, [vga_cursor]
    mov eax, esi
    mov ah, 0x0F
    mov [0xB8000 + rdi], ax
    add edi, 2
    mov [vga_cursor], edi
    pop rdi
    ret

; ============================================================================
; DATA
; ============================================================================
msg_welcome db 'MyOS: Booting...', 0
msg_long_mode db ' [64-bit mode]', 0
msg_task1 db ' [Task 1]', 0
msg_task2 db ' [Task 2]', 0

vga_cursor dd 160            ; Start on second line

; ============================================================================
; GDT (64-bit)
; ============================================================================
gdt64:
    dq 0
    dq 0x0020980000000000   ; Ring 0 code, 64-bit
    dq 0x0000920000000000   ; Ring 0 data
    dq 0x0020F80000000000   ; Ring 3 code, 64-bit
    dq 0x0000F20000000000   ; Ring 3 data
    dq 0                     ; TSS low
    dq 0                     ; TSS high

gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

; ============================================================================
; IDT
; ============================================================================
idt:
    times 256 dq 0, 0

idt_ptr:
    dw 256 * 16 - 1
    dq idt

; ============================================================================
; TSS
; ============================================================================
tss:
    times 104 db 0

; ============================================================================
; SCHEDULER STATE
; ============================================================================
current_task db 0

; ============================================================================
; STACKS
; ============================================================================
align 16
kernel_stack_bottom:
    times 4096 db 0
kernel_stack_top:

align 16
user_stack_1_bottom:
    times 4096 db 0
user_stack_1_top:

align 16
user_stack_2_bottom:
    times 4096 db 0
user_stack_2_top: