#include "idt.h"
#include "pic.h"
#include "printf.h"
#include "types.h"
#include "shell.h"
#include "types.h"   // for inb

struct idt_entry {
    u16 base_low;
    u16 sel;
    u8  zero;
    u8  flags;
    u16 base_high;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

extern void idt_flush(u32 idt_ptr_addr);

// Exception handler stubs defined in assembly
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

// IRQ handlers
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

void idt_set_gate(u8 idx, u32 handler, u16 sel, u8 flags) {
    idt[idx].base_low  = handler & 0xFFFF;
    idt[idx].base_high = (handler >> 16) & 0xFFFF;
    idt[idx].sel       = sel;
    idt[idx].zero      = 0;
    idt[idx].flags     = flags;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (u32)&idt;

    // ISRs (CPU exceptions)
    idt_set_gate(0,  (u32)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (u32)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (u32)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (u32)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (u32)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (u32)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (u32)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (u32)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (u32)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (u32)isr9,  0x08, 0x8E);
    idt_set_gate(10, (u32)isr10, 0x08, 0x8E);
    idt_set_gate(11, (u32)isr11, 0x08, 0x8E);
    idt_set_gate(12, (u32)isr12, 0x08, 0x8E);
    idt_set_gate(13, (u32)isr13, 0x08, 0x8E);
    idt_set_gate(14, (u32)isr14, 0x08, 0x8E);
    idt_set_gate(15, (u32)isr15, 0x08, 0x8E);
    idt_set_gate(16, (u32)isr16, 0x08, 0x8E);
    idt_set_gate(17, (u32)isr17, 0x08, 0x8E);
    idt_set_gate(18, (u32)isr18, 0x08, 0x8E);
    idt_set_gate(19, (u32)isr19, 0x08, 0x8E);
    idt_set_gate(20, (u32)isr20, 0x08, 0x8E);
    idt_set_gate(21, (u32)isr21, 0x08, 0x8E);
    idt_set_gate(22, (u32)isr22, 0x08, 0x8E);
    idt_set_gate(23, (u32)isr23, 0x08, 0x8E);
    idt_set_gate(24, (u32)isr24, 0x08, 0x8E);
    idt_set_gate(25, (u32)isr25, 0x08, 0x8E);
    idt_set_gate(26, (u32)isr26, 0x08, 0x8E);
    idt_set_gate(27, (u32)isr27, 0x08, 0x8E);
    idt_set_gate(28, (u32)isr28, 0x08, 0x8E);
    idt_set_gate(29, (u32)isr29, 0x08, 0x8E);
    idt_set_gate(30, (u32)isr30, 0x08, 0x8E);
    idt_set_gate(31, (u32)isr31, 0x08, 0x8E);

    // IRQs
    idt_set_gate(32, (u32)irq0,  0x08, 0x8E);
    idt_set_gate(33, (u32)irq1,  0x08, 0x8E);
    idt_set_gate(34, (u32)irq2,  0x08, 0x8E);
    idt_set_gate(35, (u32)irq3,  0x08, 0x8E);
    idt_set_gate(36, (u32)irq4,  0x08, 0x8E);
    idt_set_gate(37, (u32)irq5,  0x08, 0x8E);
    idt_set_gate(38, (u32)irq6,  0x08, 0x8E);
    idt_set_gate(39, (u32)irq7,  0x08, 0x8E);
    idt_set_gate(40, (u32)irq8,  0x08, 0x8E);
    idt_set_gate(41, (u32)irq9,  0x08, 0x8E);
    idt_set_gate(42, (u32)irq10, 0x08, 0x8E);
    idt_set_gate(43, (u32)irq11, 0x08, 0x8E);
    idt_set_gate(44, (u32)irq12, 0x08, 0x8E);
    idt_set_gate(45, (u32)irq13, 0x08, 0x8E);
    idt_set_gate(46, (u32)irq14, 0x08, 0x8E);
    idt_set_gate(47, (u32)irq15, 0x08, 0x8E);

    idt_flush((u32)&idtp);
}

void isr_handler(interrupt_frame_t *frame) {
    // Handle CPU exceptions 0-31
    u32 int_no = frame->eip; // Placeholder — real handler passes int num
    (void)frame;
    
    printf("\n[EXCEPTION] Exception occurred\n");
    printf("[EXCEPTION] EIP: %x  CS: %x  EFLAGS: %x\n", frame->eip, frame->cs, frame->eflags);
    
    asm volatile("cli; hlt");
}

void irq_handler(u32 irq) {
    if (irq == 0) { // PIT timer
        g_ticks++;
        pic_send_eoi(0);
        return;
    }
    
    if (irq == 1) { // Keyboard
        u8 scancode = inb(0x60);
        u8 released = scancode & 0x80;
        scancode &= 0x7F;
        
        static u8 last_scancode = 0;
        
        static const char scancode_to_ascii[] = {
            0,   0,   '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
            '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
            0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
            0,   '\\','z','x','c','v','b','n','m',',','.','/', 0,
            '*', 0,   ' ', 0,
        };

        if (!released && scancode < sizeof(scancode_to_ascii)) {
            char c = scancode_to_ascii[scancode];
            if (c && scancode != last_scancode) {
                shell_handle_key(c);
            }
        }
        last_scancode = (released) ? 0 : scancode;
        
        pic_send_eoi(1);
        return;
    }
    
    // Other IRQs — just EOI
    if (irq < 16) pic_send_eoi(irq);
}