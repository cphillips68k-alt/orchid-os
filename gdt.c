#include "gdt.h"

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  granularity;
    u8  base_high;
} __attribute__((packed));

struct gdt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

static struct gdt_entry gdt[5];
static struct gdt_ptr   gp;

extern void gdt_flush(u32 gdt_ptr_addr);

static void gdt_set_entry(s32 idx, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[idx].base_low    = base & 0xFFFF;
    gdt[idx].base_mid    = (base >> 16) & 0xFF;
    gdt[idx].base_high   = (base >> 24) & 0xFF;
    gdt[idx].limit_low   = limit & 0xFFFF;
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].access      = access;
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (u32)&gdt;

    gdt_set_entry(0, 0, 0,          0,    0);    // null
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // kernel code
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // kernel data
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // user code (stub)
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // user data (stub)

    gdt_flush((u32)&gp);
}