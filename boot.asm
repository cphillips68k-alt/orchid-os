; boot.asm - Stage 1 Bootloader (DIAGNOSTIC VERSION)

BITS 16
ORG 0x7C00

start:
    ; Checkpoint A: We exist
    mov ax, 0xB800
    mov es, ax
    mov byte [es:0], 'A'
    mov byte [es:1], 0x0F
    
    cli
    
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    ; Checkpoint B: Segments set
    mov ax, 0xB800
    mov es, ax
    mov byte [es:2], 'B'
    mov byte [es:3], 0x0F
    
    ; Clear screen
    mov ax, 0x0003
    int 0x10
    
    ; Checkpoint C: Screen cleared
    mov byte [es:4], 'C'
    mov byte [es:5], 0x0F
    
    ; Print boot message
    mov si, msg_booting
    call print_string_16
    
    ; Checkpoint D: Message printed
    mov byte [es:6], 'D'
    mov byte [es:7], 0x0F

    ; Load kernel from disk
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x0000
    
    ; Checkpoint E: About to read disk
    mov ax, 0xB800
    mov es, ax
    mov byte [es:8], 'E'
    mov byte [es:9], 0x0F
    
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x0000
    
    mov ah, 0x02
    mov al, 32
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, 0x80
    int 0x13
    
    jc disk_error
    
    ; Checkpoint F: Disk read OK
    mov ax, 0xB800
    mov es, ax
    mov byte [es:10], 'F'
    mov byte [es:11], 0x0F

    ; Set up GDT
    cli
    lgdt [gdt_descriptor]
    
    ; Checkpoint G: GDT loaded
    mov byte [es:12], 'G'
    mov byte [es:13], 0x0F
    
    ; Enable protected mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    ; Checkpoint H: Protected mode enabled, about to jump
    mov byte [es:14], 'H'
    mov byte [es:15], 0x4F    ; Red background for "last bootloader checkpoint"
    
    ; Far jump to kernel
    jmp 0x08:protected_mode

disk_error:
    mov ax, 0xB800
    mov es, ax
    mov byte [es:20], 'X'
    mov byte [es:21], 0x4F
    mov si, msg_disk_err
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
msg_disk_err db ' DISK ERROR', 0

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
    ; Checkpoint I: We're in 32-bit protected mode
    mov ax, 0xB800
    mov es, ax
    mov byte [es:16], 'I'
    mov byte [es:17], 0x0F
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov esp, 0x90000
    
    ; Checkpoint J: About to jump to kernel
    mov ax, 0xB800
    mov es, ax
    mov byte [es:18], 'J'
    mov byte [es:19], 0x0F
    
    jmp 0x08:0x10000

times 510-($-$$) db 0
dw 0xAA55