; boot.asm - Stage 1 Bootloader

BITS 16
ORG 0x7C00

start:
    cli
    
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    ; Clear screen
    mov ax, 0x0003
    int 0x10
    
    mov si, msg_booting
    call print_string_16

    ; Load kernel from disk (sector 2, 32 sectors -> 0x10000)
    mov ah, 0x02
    mov al, 32
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, 0x80
    mov bx, 0x1000
    mov es, bx
    mov bx, 0x0000
    int 0x13
    
    jc disk_error
    
    ; Verify we loaded something real
    ; Check that first byte of kernel is not zero
    mov ax, 0x1000
    mov es, ax
    cmp byte [es:0x0000], 0
    je blank_error
    
    mov si, msg_ok
    call print_string_16

    ; Switch to protected mode
    cli
    lgdt [gdt_descriptor]
    
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    jmp 0x08:protected_mode

disk_error:
    mov si, msg_disk_err
    call print_string_16
    jmp $

blank_error:
    mov si, msg_blank
    call print_string_16
    jmp $

print_string_16:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string_16
.done:
    ret

msg_booting db 'Booting...', 0
msg_ok db ' OK', 13, 10, 0
msg_disk_err db ' ERR:DISK', 0
msg_blank db ' ERR:BLANK', 0

; GDT
gdt_start:
    dq 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

BITS 32
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    
    jmp 0x08:0x10000

times 510-($-$$) db 0
dw 0xAA55