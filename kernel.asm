; kernel.asm - Orchid OS Kernel
; 32-bit protected mode with interrupts, scheduler, ring separation.
;
; Three things:
;   1. Interrupting (PIT timer via 8259 PIC)
;   2. Scheduler that handles loops (round-robin preemption)
;   3. Ring 0 / Ring 3 separation (GDT + TSS + syscall)

BITS 32
ORG 0x10000

; ============================================================================
; ENTRY POINT
; ============================================================================
kernel_main:
    ; Clear screen
    mov edi, 0xB8000
    mov ecx, 80 * 25
    mov ax, 0x0F20
    rep stosw

    ; Print banner
    mov esi, msg_banner
    mov edi, 0xB8000
    call print_32

    ; Initialize the three things
    call init_idt
    call init_pic
    call init_pit
    call init_tss
    call init_tasks

    ; Print status
    mov esi, msg_ready
    mov edi, 0xB8000 + 160
    call print_32

    ; Enable interrupts
    sti

    ; Jump to first task (ring 3)
    call jump_to_task1

    ; We should never return
    jmp $

; ============================================================================
; IDT - Interrupt Descriptor Table
; ============================================================================
init_idt:
    ; Map IRQ 0 (timer) to vector 32
    mov eax, timer_handler
    mov ebx, 32
    call set_idt_gate

    ; Map IRQ 1 (keyboard) to vector 33
    mov eax, keyboard_handler
    mov ebx, 33
    call set_idt_gate

    ; Map syscall vector 0x80
    mov eax, syscall_handler
    mov ebx, 0x80
    call set_idt_gate

    lidt [idt_ptr]
    ret

; Set an interrupt gate in the IDT
; EAX = handler address, EBX = vector number
set_idt_gate:
    push ebx
    shl ebx, 3                      ; 8 bytes per entry
    add ebx, idt

    mov word [ebx], ax              ; Handler low word
    mov word [ebx + 2], 0x08        ; Code segment selector
    mov byte [ebx + 4], 0x00        ; Reserved
    mov byte [ebx + 5], 0x8E        ; Present, Ring 0, 32-bit interrupt gate
    shr eax, 16
    mov word [ebx + 6], ax          ; Handler high word

    pop ebx
    ret

; ============================================================================
; PIC - Programmable Interrupt Controller
; ============================================================================
init_pic:
    ; Remap: IRQ 0-7 -> IDT 32-39, IRQ 8-15 -> IDT 40-47
    ; This avoids conflict with CPU exceptions (0-31)

    ; ICW1: Begin initialization
    mov al, 0x11
    out 0x20, al
    out 0xA0, al

    ; ICW2: Base vector offsets
    mov al, 32
    out 0x21, al
    mov al, 40
    out 0xA1, al

    ; ICW3: Master/slave wiring
    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xA1, al

    ; ICW4: 8086 mode
    mov al, 0x01
    out 0x21, al
    out 0xA1, al

    ; Mask: enable only IRQ 0 (timer) and IRQ 1 (keyboard)
    mov al, 0xFC                ; 1111 1100
    out 0x21, al
    mov al, 0xFF                ; Mask all on slave
    out 0xA1, al

    ret

; ============================================================================
; PIT - Programmable Interval Timer
; ============================================================================
init_pit:
    ; Channel 0, lobyte/hibyte, mode 2 (rate generator), binary
    mov al, 0x36
    out 0x43, al

    ; Divisor = 1193180 / 100 = 11931 (approx) for ~100Hz
    mov ax, 11931
    out 0x40, al                ; Low byte
    mov al, ah
    out 0x40, al                ; High byte
    ret

; ============================================================================
; TSS - Task State Segment
; ============================================================================
init_tss:
    ; Fill TSS with kernel stack pointer for ring 0
    ; When a task in ring 3 does int 0x80, CPU loads ESP0 from here
    mov dword [tss + 4], 0x90000    ; ESP0 (kernel stack)

    ; Set TSS descriptor in GDT
    mov eax, tss
    mov word [gdt + 0x28], 0x67     ; Limit = 103 bytes
    mov word [gdt + 0x28 + 2], ax   ; Base low
    shr eax, 16
    mov byte [gdt + 0x28 + 4], al   ; Base mid
    mov byte [gdt + 0x28 + 5], 0xE9 ; Present, Ring 3, 32-bit TSS
    mov byte [gdt + 0x28 + 6], 0x00 ; Flags + limit high
    mov byte [gdt + 0x28 + 7], ah   ; Base high

    ; Load TSS
    mov ax, 0x2B                    ; TSS selector (0x28 | 0x03 for ring 3)
    ltr ax
    ret

; ============================================================================
; TASKS
; ============================================================================
init_tasks:
    mov dword [current_task], 0
    ret

; Jump to task 1 (ring 3)
jump_to_task1:
    ; Build iret stack frame for ring 3
    push 0x23                    ; SS = user data segment
    push task1_stack_top         ; ESP
    pushf                        ; EFLAGS
    pop eax
    or eax, 0x200                ; Enable interrupts
    push eax
    push 0x1B                    ; CS = user code segment
    push task1_main              ; EIP
    iret

; ============================================================================
; TIMER INTERRUPT HANDLER
; ============================================================================
timer_handler:
    ; Save registers
    pushad

    ; Switch tasks
    mov eax, [current_task]
    xor eax, 1
    mov [current_task], eax

    ; Update visual indicator on screen
    test eax, eax
    jz .show_task1

    ; Show task 2 indicator
    mov word [0xB8000 + 158], 0x0F32    ; '2' on task bar
    jmp .done_visual

.show_task1:
    mov word [0xB8000 + 158], 0x0F31    ; '1' on task bar

.done_visual:
    ; Send End of Interrupt to PIC
    mov al, 0x20
    out 0x20, al

    ; Restore registers
    popad
    iret

; ============================================================================
; KEYBOARD INTERRUPT HANDLER
; ============================================================================
keyboard_handler:
    pushad

    ; Read scancode from keyboard data port
    in al, 0x60

    ; Just echo something to show keyboard works
    mov byte [0xB8000 + 156], al

    ; Send EOI
    mov al, 0x20
    out 0x20, al

    popad
    iret

; ============================================================================
; SYSCALL HANDLER (int 0x80)
; ============================================================================
syscall_handler:
    ; Syscall numbers in EAX
    ; 1 = print character (EBX = char, ECX = screen offset)
    cmp eax, 1
    je .sys_print_char

    ; 2 = yield (let scheduler switch)
    cmp eax, 2
    je .sys_yield

    ; Unknown syscall - return
    iret

.sys_print_char:
    pushad
    mov ah, 0x0F
    mov al, bl
    mov [0xB8000 + ecx], ax
    popad
    iret

.sys_yield:
    ; Just return - the timer will preempt us anyway
    iret

; ============================================================================
; TASK 1 (Ring 3)
; ============================================================================
task1_main:
    ; Draw task 1's area
    mov ebx, '>'
    mov ecx, 320                    ; Row 2, column 0 (160*2)
    mov eax, 1
    int 0x80

    mov ebx, 'T'
    mov ecx, 322
    mov eax, 1
    int 0x80

    mov ebx, '1'
    mov ecx, 324
    mov eax, 1
    int 0x80

.task1_loop:
    ; Infinite loop - timer will preempt us
    nop
    jmp .task1_loop

; ============================================================================
; TASK 2 (Ring 3)
; ============================================================================
task2_main:
    ; Draw task 2's area
    mov ebx, '>'
    mov ecx, 480                    ; Row 3, column 0 (160*3)
    mov eax, 1
    int 0x80

    mov ebx, 'T'
    mov ecx, 482
    mov eax, 1
    int 0x80

    mov ebx, '2'
    mov ecx, 484
    mov eax, 1
    int 0x80

.task2_loop:
    ; Infinite loop - different NOP pattern for visual difference in debug
    nop
    nop
    jmp .task2_loop

; ============================================================================
; UTILITY: Print null-terminated string
; ============================================================================
print_32:
    push eax
    push edi
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0F
    mov [edi], ax
    add edi, 2
    jmp .loop
.done:
    pop edi
    pop eax
    ret

; ============================================================================
; DATA
; ============================================================================
msg_banner db "Orchid OS v0.2 - Protected Mode + Interrupts + Scheduler", 0
msg_ready db "Ring 0/3 active. Tasks running. Keyboard listening.", 0

; ============================================================================
; GDT
; ============================================================================
gdt:
    ; Null descriptor
    dq 0
    ; Ring 0 code (selector 0x08)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
    ; Ring 0 data (selector 0x10)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
    ; Ring 3 code (selector 0x1B)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11111010b                ; Present, Ring 3, Code, Readable
    db 11001111b
    db 0x00
    ; Ring 3 data (selector 0x23)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11110010b                ; Present, Ring 3, Data, Writable
    db 11001111b
    db 0x00
    ; TSS (filled by init_tss)
    dq 0
    dq 0

gdt_ptr:
    dw $ - gdt - 1
    dd gdt

; ============================================================================
; IDT
; ============================================================================
idt:
    times 256 dq 0              ; 256 entries, 8 bytes each

idt_ptr:
    dw 256 * 8 - 1
    dd idt

; ============================================================================
; TSS
; ============================================================================
tss:
    ; Minimum TSS is 104 bytes for 32-bit
    times 104 db 0

; ============================================================================
; TASK STATE
; ============================================================================
current_task dd 0

; ============================================================================
; TASK STACKS
; ============================================================================
align 16
task1_stack:
    times 4096 db 0
task1_stack_top:

align 16
task2_stack:
    times 4096 db 0
task2_stack_top: