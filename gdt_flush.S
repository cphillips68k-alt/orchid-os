global gdt_flush
gdt_flush:
    mov eax, [esp+4]    ; gdt_ptr address
    lgdt [eax]
    mov ax, 0x10        ; kernel data segment (offset 0x10 from GDT base)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush     ; far jump to kernel code segment (offset 0x08)
.flush:
    ret