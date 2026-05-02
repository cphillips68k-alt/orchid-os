; boot.asm - Stage 1 Bootloader
; 
; This is the first 512 bytes of our OS.
; BIOS loads us at 0x7C00. We load the kernel from disk,
; switch to 32-bit protected mode, and jump to it.
;
; The kernel is at sector 2 onwards, loaded to 0x10000.

BITS 16
ORG 0x7C00

start:
    ; Clear interrupts during setup
    cli
    
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack grows down from bootloader
    
    ; Clear screen - set video mode 80x25 text
    mov ax, 0x0003
    int 0x10
    
    ; Print boot message
    mov si, msg_booting
    call print_string_16

    ; Load kernel from disk
    ; Kernel is at sector 2 (right after this bootloader)
    ; We'll load 32 sectors (16KB) to address 0x10000
    mov ax, 0x1000          ; ES = 0x1000
    mov es, ax
    mov bx, 0x0000          ; BX = 0x0000  => ES:BX = 0x10000
    
    mov ah, 0x02            ; BIOS: Read sectors
    mov al, 32              ; Read 32 sectors
    mov ch, 0x00            ; Cylinder 0
    mov cl, 0x02            ; Start at sector 2
    mov dh, 0x00            ; Head 0
    mov dl, 0x80            ; First hard disk
    int 0x13
    
    jc disk_error           ; CF set on error
    
    mov si, msg_ok
    call print_string_16

    ; Set up GDT for 32-bit protected mode
    cli
    lgdt [gdt_descriptor]
    
    ; Enable protected mode
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    ; Far jump to flush prefetch queue and enter protected mode
    jmp 0x08:protected_mode

disk_error:
    mov si, msg_disk_err
    call print_string_16
    jmp $                   ; Hang

; ============================================================================
; 16-bit print string routine
; Input: DS:SI -> null-terminated string
; ============================================================================
print_string_16:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E            ; BIOS teletype
    int 0x10
    jmp print_string_16
.done:
    ret

; ============================================================================
; Data
; ============================================================================
msg_booting db 'Booting...', 0
msg_ok db ' OK', 13, 10, 0
msg_disk_err db ' DISK ERROR', 0

; ============================================================================
; GDT - 32-bit protected mode
; Flat memory model: 4GB code, 4GB data
; ============================================================================
gdt_start:
    ; Null descriptor (required)
    dq 0
    
    ; 32-bit code descriptor
    dw 0xFFFF               ; Limit (0-15)
    dw 0x0000               ; Base (0-15)
    db 0x00                 ; Base (16-23)
    db 10011010b            ; Access: Present, Ring 0, Code, Readable
    db 11001111b            ; Flags: 4K granularity, 32-bit + Limit (16-19)
    db 0x00                 ; Base (24-31)
    
    ; 32-bit data descriptor
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b            ; Access: Present, Ring 0, Data, Writable
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ============================================================================
; 32-bit Protected Mode Entry
; ============================================================================
BITS 32
protected_mode:
    ; Set up data segments
    mov ax, 0x10            ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up a temporary stack
    mov esp, 0x90000
    
    ; Jump to the kernel at 0x10000
    ; The kernel will set up 64-bit mode from here
    jmp 0x08:0x10000

; ============================================================================
; Boot sector signature
; ============================================================================
times 510-($-$$) db 0
dw 0xAA55