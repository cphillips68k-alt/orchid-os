# idt.S — AT&T syntax for Clang IAS

.section .text

# Common ISR handler
isr_common:
    pushal
    pushw %ds
    pushw %es
    pushw %fs
    pushw %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movl %esp, %eax
    pushl %eax
    call isr_handler
    addl $4, %esp
    popw %gs
    popw %fs
    popw %es
    popw %ds
    popal
    addl $8, %esp     # pop error code and int number
    iret

# Common IRQ handler
irq_common:
    pushal
    pushw %ds
    pushw %es
    pushw %fs
    pushw %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movl %esp, %eax
    pushl %eax
    call irq_handler
    addl $4, %esp
    popw %gs
    popw %fs
    popw %es
    popw %ds
    popal
    addl $8, %esp
    iret

# Macro to generate ISR stubs
# ISRs without error code
.macro ISR_NOERR num
.globl isr\num
isr\num:
    pushl $0
    pushl $\num
    jmp isr_common
.endm

# ISRs with error code
.macro ISR_ERR num
.globl isr\num
isr\num:
    pushl $\num
    jmp isr_common
.endm

# Macro for IRQs
.macro IRQ num qnum
.globl irq\num
irq\num:
    pushl $0
    pushl $\qnum
    jmp irq_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47