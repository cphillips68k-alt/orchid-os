; boot.asm - 32-bit Protected Mode Bootloader
; Loads kernel from disk, switches to 32-bit PM, jumps to 0x10000.

BITS 16
ORG 0x7C00

start:
    cli                     ; No interrupts during setup

    ; Set up real mode segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Clear screen (VGA text mode 80x25)
    mov ax, 0x0003
    int 0x10

    ; Print boot message
    mov si, msg_booting
    call print_16

    ; Load kernel from disk
    ; Kernel occupies sectors 2 through 33 (32 sectors = 16KB)
    ; Load destination: 0x1000:0x0000 = physical 0x10000
    mov ah, 0x02            ; BIOS read sectors
    mov al, 32              ; Number of sectors
    mov ch, 0x00            ; Cylinder 0
    mov cl, 0x02            ; Start sector 2
    mov dh, 0x00            ; Head 0
    mov dl, 0x80            ; First hard disk
    mov bx, 0x1000
    mov es, bx
    mov bx, 0x0000          ; ES:BX = 0x10000
    int 0x13

    jc disk_error           ; Carry set = error

    mov si, msg_ok
    call print_16

    ; Load 32-bit GDT
    cli
    lgdt [gdt32_desc]

    ; Set protected mode bit in CR0
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to flush pipeline and enter 32-bit protected mode
    jmp 0x08:protected_mode

disk_error:
    mov si, msg_disk_err
    call print_16
    jmp $                   ; Hang

; ---------------------------------------------------------------------------
; 16-bit print routine (null-terminated string at DS:SI)
; ---------------------------------------------------------------------------
print_16:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E            ; BIOS teletype
    int 0x10
    jmp print_16
.done:
    ret

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
msg_booting db "Booting...", 0
msg_ok      db " OK", 13, 10, 0
msg_disk_err db " ERR:DISK", 0

; ---------------------------------------------------------------------------
; 32-bit Global Descriptor Table
; Flat memory model: code and data both span 4 GB
; ---------------------------------------------------------------------------
gdt32_start:
    ; Null descriptor (required)
    dq 0

    ; Code segment descriptor (selector 0x08)
    dw 0xFFFF               ; Limit low (0-15)
    dw 0x0000               ; Base low (0-15)
    db 0x00                 ; Base mid (16-23)
    db 10011010b            ; Access: Present, Ring 0, Code, Readable
    db 11001111b            ; Flags: 4K granularity, 32-bit, limit high (16-19)
    db 0x00                 ; Base high (24-31)

    ; Data segment descriptor (selector 0x10)
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b            ; Present, Ring 0, Data, Writable
    db 11001111b
    db 0x00

gdt32_end:

gdt32_desc:
    dw gdt32_end - gdt32_start - 1    ; GDT size - 1
    dd gdt32_start                     ; GDT address

; ---------------------------------------------------------------------------
; 32-bit Protected Mode Entry Point
; ---------------------------------------------------------------------------
BITS 32
protected_mode:
    ; Set up segment registers with data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up a known-good stack (well below where kernel will be loaded)
    mov esp, 0x90000

    ; Jump to kernel at 0x10000
    jmp 0x08:0x10000

; ---------------------------------------------------------------------------
; Boot sector signature
; ---------------------------------------------------------------------------
times 510-($-$$) db 0
dw 0xAA55