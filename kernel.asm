; kernel.asm - Minimal kernel to reach 64-bit long mode

ORG 0x10000

BITS 32
kernel_main:
    ; Save a marker that we got here
    mov dword [0xB8000], 0x0F4B0F4F   ; "OK" in white
    
    ; Set up minimal paging (identity map first 2MB)
    mov edi, 0x9000
    mov cr3, edi
    mov ecx, 0x3000
    xor eax, eax
    rep stosd
    
    ; PML4[0] -> 0xA000 (PDP)
    mov dword [0x9000], 0xA003
    ; PDP[0] -> 0xB000 (PD)  
    mov dword [0xA000], 0xB003
    ; PD[0] = 2MB huge page at 0x0
    mov dword [0xB000], 0x00000083
    
    ; Enable PAE + LME + Paging
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax
    
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr
    
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax
    
    ; Load 64-bit GDT
    lgdt [gdt64_ptr]
    
    ; Jump to 64-bit mode
    jmp 0x08:long_mode_start

BITS 64
long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000
    
    ; Print success message
    mov rsi, msg_hello
    mov edi, 160              ; Second line
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0F
    mov [0xB8000 + rdi], ax
    add rdi, 2
    jmp .loop
.done:
    
    jmp $

msg_hello db 'Orchid OS - 64-bit long mode active', 0

; 64-bit GDT
gdt64:
    dq 0
    dq 0x0020980000000000    ; Ring 0 64-bit code
    dq 0x0000920000000000    ; Ring 0 data
gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64