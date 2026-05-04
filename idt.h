#ifndef IDT_H
#define IDT_H

#include "types.h"

typedef struct {
    u32 eip;
    u32 cs;
    u32 eflags;
    u32 esp;
    u32 ss;
} __attribute__((packed)) interrupt_frame_t;

void idt_init(void);
void idt_set_gate(u8 idx, u32 handler, u16 sel, u8 flags);
void isr_handler(interrupt_frame_t *frame);

#endif