# idt_flush.S
.globl idt_flush
idt_flush:
    movl 4(%esp), %eax
    lidt (%eax)
    retl