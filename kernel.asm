; kernel.asm - The Actual Operating System (DIAGNOSTIC VERSION)

ORG 0x10000

; ============================================================================
; 32-BIT ENTRY POINT
; ============================================================================
BITS 32
kernel_main:
    ; Checkpoint K: We're in the kernel
    mov byte [0xB8000 + 20], 'K'
    mov byte [0xB8000 + 21], 0x0F
    
    ; Clear the screen
    mov edi, 0xB8000
    mov ecx, 80 * 25
    mov ax, 0x0F00
    rep stosw
    
    ; Checkpoint L: Screen cleared
    mov byte [0xB8000 + 22], 'L'
    mov byte [0xB8000 + 23], 0x0F
    
    ; Print welcome
    mov esi, msg_welcome
    call vga_print_32
    
    ; Checkpoint M: Welcome printed
    mov byte [0xB8000 + 24], 'M'
    mov byte [0xB8000 + 25], 0x0F
    
    ; Set up paging
    call setup_paging
    
    ; Checkpoint N: Paging ready
    mov byte [0xB8000 + 26], 'N'
    mov byte [0xB8000 + 27], 0x0F
    
    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    
    ; Enable LME
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    
    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    
    ; Checkpoint O: Paging enabled, about to load 64-bit GDT
    mov byte [0xB8000 + 28], 'O'
    mov byte [0xB8000 + 29], 0x4F   ; Red - last 32-bit checkpoint
    
    ; Load 64-bit GDT
    lgdt [gdt64_ptr]
    
    ; Jump to 64-bit mode
    jmp 0x08:long_mode_start

; ============================================================================
; Paging setup
; ============================================================================
setup_paging:
    mov edi, 0x9000
    mov cr3, edi
    mov ecx, 4096 * 3
    xor eax, eax
    rep stosb
    
    mov edi, 0x9000
    mov dword [edi], 0xA003
    
    mov edi, 0xA000
    mov dword [edi], 0xB003
    
    mov edi, 0xB000
    mov dword [edi], 0x00000083
    
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
    ; Checkpoint P: We made it to 64-bit mode!
    mov byte [0xB8000 + 30], 'P'
    mov byte [0xB8000 + 31], 0x2F   ; Green
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov rsp, kernel_stack_top
    
    ; Print success
    mov esi, msg_long_mode
    call vga_print_64
    
    ; We'll stop here for now - just getting to 64-bit is the goal
    ; The rest (interrupts, scheduler, ring 3) comes after we confirm this works
    
    ; Hang with visual indicator
    mov byte [0xB8000 + 32], '!'
    mov byte [0xB8000 + 33], 0x2F
    
    jmp $

; ============================================================================
; VGA 64-bit
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

; ============================================================================
; DATA
; ============================================================================
msg_welcome db 'Orchid OS: Starting...', 0
msg_long_mode db ' [64-bit mode OK]', 0

vga_cursor dd 160

; ============================================================================
; 64-bit GDT
; ============================================================================
gdt64:
    dq 0
    dq 0x0020980000000000
    dq 0x0000920000000000
    dq 0                     ; Placeholder for TSS
    dq 0

gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

; ============================================================================
; STACKS
; ============================================================================
align 16
kernel_stack_bottom:
    times 4096 db 0
kernel_stack_top: