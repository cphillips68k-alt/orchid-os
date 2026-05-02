; kernel.asm - 32-bit Protected Mode Kernel
; Landed here at 0x10000 after bootloader jump.
; No long mode. No paging yet. Just proof-of-life.

BITS 32
ORG 0x10000

kernel_main:
    ; Clear the screen by writing spaces
    mov edi, 0xB8000        ; VGA text buffer
    mov ecx, 80 * 25
    mov ax, 0x0F20          ; White on black, space character
    rep stosw

    ; Print a welcome message
    mov esi, msg_welcome
    mov edi, 0xB8000        ; Top-left corner
    call print_32

    ; Print a second line to prove we control the cursor
    mov esi, msg_ready
    mov edi, 0xB8000 + 160  ; Row 1 (160 bytes per row)
    call print_32

    ; Hang forever
    cli
    hlt
    jmp $

; ---------------------------------------------------------------------------
; 32-bit print routine (null-terminated string at ESI)
; Destroys: EDI (advanced), ESI (advanced)
; ---------------------------------------------------------------------------
print_32:
    push eax
.loop:
    lodsb                   ; AL = [ESI++]
    test al, al
    jz .done
    mov ah, 0x0F            ; White on black
    mov [edi], ax
    add edi, 2
    jmp .loop
.done:
    pop eax
    ret

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
msg_welcome db "Orchid OS - 32-bit kernel active", 0
msg_ready   db "System ready. No GUI yet, but it's mine.", 0