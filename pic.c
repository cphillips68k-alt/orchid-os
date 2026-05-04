#include "pic.h"
#include "types.h"

#define PIC1      0x20
#define PIC2      0xA0
#define PIC1_CMD  PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_CMD  PIC2
#define PIC2_DATA (PIC2 + 1)
#define PIC_EOI   0x20

#define ICW1_ICW4        0x01
#define ICW1_INIT        0x10
#define ICW4_8086        0x01

static void io_wait(void) {
    // Brief delay on ancient hardware; harmless on modern
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

void pic_init(void) {
    u8 mask1, mask2;

    // Save masks
    mask1 = inb(PIC1_DATA);
    mask2 = inb(PIC2_DATA);

    // ICW1: start init in cascade mode
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // ICW2: vector offsets
    outb(PIC1_DATA, 0x20); // IRQ 0-7 → int 0x20-0x27
    io_wait();
    outb(PIC2_DATA, 0x28); // IRQ 8-15 → int 0x28-0x2F
    io_wait();

    // ICW3: cascade config
    outb(PIC1_DATA, 0x04); // IRQ2 has slave
    io_wait();
    outb(PIC2_DATA, 0x02); // Slave on IRQ2
    io_wait();

    // ICW4: 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Restore masks
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(u8 irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask(u8 irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  mask = inb(port) | (1 << (irq & 7));
    outb(port, mask);
}

void pic_unmask(u8 irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  mask = inb(port) & ~(1 << (irq & 7));
    outb(port, mask);
}