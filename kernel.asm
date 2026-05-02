; kernel.asm - Orchid OS Kernel (FIXED)
; 32-bit protected mode with interrupts, scheduler, ring separation.

BITS 32
ORG 0x10000

; ============================================================================
; ENTRY POINT
; ============================================================================
kernel_main:
    ; Load our comprehensive GDT (ring 0, ring 3, TSS)
    lgdt [gdt_ptr]

    ; Reload segment registers to use new GDT
    jmp 0x08:.reload_cs     ; Far jump to reload CS
.reload_cs:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up kernel stack (below 0x10000, safe area)
    mov esp, 0x90000

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

    ; Print status
    mov esi, msg_ready
    mov edi, 0xB8000 + 160
    call print_32

    ; Enable interrupts
    sti

    ; Jump to task 1 in ring 3
    ; Build iret stack frame manually
    push dword 0x23          ; SS = user data
    push dword task1_stack_top ; ESP
    push dword 0x202         ; EFLAGS (IF set, IOPL=0, always 1)
    push dword 0x1B          ; CS = user code
    push dword task1_main    ; EIP
    iret

; ============================================================================
; IDT
; ============================================================================
init_idt:
    mov eax, timer_handler
    mov ebx, 32
    call set_idt_gate

    mov eax, keyboard_handler
    mov ebx, 33
    call set_idt_gate

    mov eax, syscall_handler
    mov ebx, 0x80
    call set_idt_gate

    lidt [idt_ptr]
    ret

set_idt_gate:
    push ebx
    shl ebx, 3
    add ebx, idt
    mov word [ebx], ax
    mov word [ebx + 2], 0x08
    mov byte [ebx + 4], 0x00
    mov byte [ebx + 5], 0x8E       ; Present, ring 0, interrupt gate
    shr eax, 16
    mov word [ebx + 6], ax
    pop ebx
    ret

; ============================================================================
; PIC
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

    mov al, 0xFC              ; enable IRQ0 and IRQ1
    out 0x21, al
    mov al, 0xFF
    out 0xA1, al
    ret

; ============================================================================
; PIT
; ============================================================================
init_pit:
    mov al, 0x36
    out 0x43, al
    mov ax, 11931
    out 0x40, al
    mov al, ah
    out 0x40, al
    ret

; ============================================================================
; TSS
; ============================================================================
init_tss:
    ; Kernel stack for ring 0 transitions
    mov dword [tss + 4], 0x90000   ; ESP0

    ; Fill TSS descriptor in GDT (slot 5, offset 0x28)
    mov eax, tss
    mov word [gdt + 0x28], 103     ; limit = 104-1
    mov word [gdt + 0x28 + 2], ax
    shr eax, 16
    mov byte [gdt + 0x28 + 4], al
    mov byte [gdt + 0x28 + 5], 0x89 ; Present, 32-bit available TSS
    mov byte [gdt + 0x28 + 6], 0x00
    mov byte [gdt + 0x28 + 7], ah

    ; Load TSS selector (0x28 | 0x03)
    mov ax, 0x2B
    ltr ax
    ret

; ============================================================================
; TIMER HANDLER
; ============================================================================
timer_handler:
    pushad
    mov eax, [current_task]
    xor eax, 1
    mov [current_task], eax

    test eax, eax
    jz .show1
    mov word [0xB8000 + 158], 0x0F32  ; '2'
    jmp .done
.show1:
    mov word [0xB8000 + 158], 0x0F31  ; '1'
.done:
    mov al, 0x20
    out 0x20, al
    popad
    iret

; ============================================================================
; KEYBOARD HANDLER
; ============================================================================
keyboard_handler:
    pushad
    in al, 0x60
    mov byte [0xB8000 + 156], al
    mov al, 0x20
    out 0x20, al
    popad
    iret

; ============================================================================
; SYSCALL HANDLER
; ============================================================================
syscall_handler:
    cmp eax, 1
    je .print_char
    iret
.print_char:
    pushad
    mov ah, 0x0F
    mov al, bl
    mov [0xB8000 + ecx], ax
    popad
    iret

; ============================================================================
; UTILITY
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

current_task dd 0

; ============================================================================
; GDT (loaded by kernel)
; ============================================================================
gdt:
    dq 0                           ; null
    dq 0x00CF9A000000FFFF           ; ring 0 code, 32-bit
    dq 0x00CF92000000FFFF           ; ring 0 data
    dq 0x00CFFA000000FFFF           ; ring 3 code
    dq 0x00CFF2000000FFFF           ; ring 3 data
    dq 0                             ; TSS low
    dq 0                             ; TSS high
gdt_ptr:
    dw $ - gdt - 1
    dd gdt

; ============================================================================
; IDT
; ============================================================================
idt:
    times 256 dq 0
idt_ptr:
    dw 256*8 - 1
    dd idt

; ============================================================================
; TSS
; ============================================================================
tss:
    times 104 db 0

; ============================================================================
; TASK 1
; ============================================================================
task1_main:
    mov ebx, '>'
    mov ecx, 320
    mov eax, 1
    int 0x80
    mov ebx, 'T'
    mov ecx, 322
    int 0x80
    mov ebx, '1'
    mov ecx, 324
    int 0x80
.loop:
    nop
    jmp .loop

; ============================================================================
; TASK 2
; ============================================================================
task2_main:
    mov ebx, '>'
    mov ecx, 480
    mov eax, 1
    int 0x80
    mov ebx, 'T'
    mov ecx, 482
    int 0x80
    mov ebx, '2'
    mov ecx, 484
    int 0x80
.loop:
    nop
    nop
    jmp .loop

; ============================================================================
; STACKS
; ============================================================================
align 16
task1_stack:
    times 4096 db 0
task1_stack_top:

align 16
task2_stack:
    times 4096 db 0
task2_stack_top: