/* kernel.c - Orchid OS 32-bit C kernel
 * Three things: interrupts, scheduler, ring 0/3 separation */
#define VGA_BUF ((volatile unsigned short *)0xB8000)
static unsigned int cursor = 0;

/* ---- basic I/O --------------------------------------------------------- */
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void print(const char *s) {
    while (*s) VGA_BUF[cursor++] = 0x0F00 | (unsigned char)(*s++);
}
static void putchar_at(char c, unsigned int off) {
    VGA_BUF[off] = 0x0F00 | (unsigned char)c;
}

/* ---- GDT & TSS --------------------------------------------------------- */
struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_mid;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct gdt_entry gdt[6];
struct gdt_ptr gp;

struct tss {
    unsigned short link, __unused0;
    unsigned int esp0;
    unsigned short ss0, __unused1;
    unsigned int esp1, ss1, esp2, ss2;
    unsigned int cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    unsigned short es, __unused2, cs, __unused3, ss, __unused4, ds, __unused5,
                   fs, __unused6, gs, __unused7;
    unsigned short ldt, __unused8;
    unsigned short trap, iomap_base;
} __attribute__((packed)) tss;

static void set_gdt_entry(int num, unsigned int base, unsigned int limit,
                           unsigned char access, unsigned char gran) {
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_mid = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

extern void reload_segments(void);

/* ---- IDT & interrupt stubs --------------------------------------------- */
struct idt_entry {
    unsigned short base_low;
    unsigned short sel;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr ip;

extern void timer_stub(void);
extern void keyboard_stub(void);
extern void syscall_stub(void);

static void set_idt_gate(int num, unsigned int handler,
                          unsigned short sel, unsigned char flags) {
    idt[num].base_low = handler & 0xFFFF;
    idt[num].base_high = (handler >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

/* ---- Tasks ------------------------------------------------------------- */
#define STACK_SIZE 4096
unsigned char task1_kstack[STACK_SIZE] __attribute__((aligned(16)));
unsigned char task2_kstack[STACK_SIZE] __attribute__((aligned(16)));
unsigned char task1_ustack[STACK_SIZE] __attribute__((aligned(16)));
unsigned char task2_ustack[STACK_SIZE] __attribute__((aligned(16)));

unsigned int task1_ksp, task2_ksp;
unsigned int current_task = 0;

void task1_main(void);
void task2_main(void);

/* ---- Syscall handler --------------------------------------------------- */
struct regs {
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

void syscall_handler(struct regs *r) {
    if (r->eax == 1) {                    // SYS_WRITE
        putchar_at((char)r->ebx, r->ecx);
    }
}

/* ---- Scheduler --------------------------------------------------------- */
unsigned int schedule(unsigned int current_esp) {
    if (current_task == 0)
        task1_ksp = current_esp;
    else
        task2_ksp = current_esp;

    current_task = !current_task;

    return (current_task == 0) ? task1_ksp : task2_ksp;
}

/* ---- Timer handler ----------------------------------------------------- */
unsigned int timer_handler(unsigned int esp) {
    static int ticks = 0;
    ticks++;
    putchar_at('0' + (ticks % 10), 158);   // visible heartbeat
    return schedule(esp);
}

/* ---- Keyboard handler -------------------------------------------------- */
void keyboard_handler(void) {
    unsigned char sc = inb(0x60);
    putchar_at(sc, 156);                    // raw scancode
}

/* ---- Kernel main ------------------------------------------------------- */
void kernel_main(void) {
    int i;
    for (i = 0; i < 80*25; i++) VGA_BUF[i] = 0x0F20;   // clear
    cursor = 0;
    print("Orchid OS v0.3 - C kernel");
    cursor = 160;
    print("Interrupts, scheduler, ring 3");

    /* ----- GDT (ring0 + ring3 + TSS) ----- */
    set_gdt_entry(0, 0, 0, 0, 0);
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);  // ring0 code
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xCF);  // ring0 data
    set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);  // ring3 code
    set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);  // ring3 data
    set_gdt_entry(5, (unsigned int)&tss, sizeof(tss)-1, 0x89, 0x00); // TSS
    gp.limit = sizeof(gdt)-1;
    gp.base = (unsigned int)gdt;
    asm volatile("lgdt %0" : : "m"(gp));
    reload_segments();

    /* ----- TSS ----- */
    tss.esp0 = 0x90000;
    tss.ss0  = 0x10;
    asm volatile("mov $0x2B, %ax; ltr %ax");

    /* ----- IDT ----- */
    set_idt_gate(32, (unsigned int)timer_stub, 0x08, 0x8E);
    set_idt_gate(33, (unsigned int)keyboard_stub, 0x08, 0x8E);
    set_idt_gate(0x80, (unsigned int)syscall_stub, 0x08, 0xEE);
    ip.limit = sizeof(idt)-1;
    ip.base = (unsigned int)idt;
    asm volatile("lidt %0" : : "m"(ip));

    /* ----- PIC remap & unmask IRQ0, IRQ1 ----- */
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC); // enable timer & keyboard
    outb(0xA1, 0xFF);

    /* ----- PIT 100 Hz ----- */
    outb(0x43, 0x36);
    outb(0x40, 11931 & 0xFF);
    outb(0x40, 11931 >> 8);

    /* ----- Prepare task contexts (kernel stacks + iret frames) ----- */
    unsigned int *ksp;

    ksp = (unsigned int *)(task1_kstack + STACK_SIZE);
    *--ksp = 0x23;                           // user SS
    *--ksp = (unsigned int)(task1_ustack + STACK_SIZE); // user ESP
    *--ksp = 0x202;                          // EFLAGS
    *--ksp = 0x1B;                           // user CS
    *--ksp = (unsigned int)task1_main;       // user EIP
    for (i = 0; i < 8; i++) *--ksp = 0;     // dummy pushad
    task1_ksp = (unsigned int)ksp;

    ksp = (unsigned int *)(task2_kstack + STACK_SIZE);
    *--ksp = 0x23;
    *--ksp = (unsigned int)(task2_ustack + STACK_SIZE);
    *--ksp = 0x202;
    *--ksp = 0x1B;
    *--ksp = (unsigned int)task2_main;
    for (i = 0; i < 8; i++) *--ksp = 0;
    task2_ksp = (unsigned int)ksp;

    current_task = 0;

    /* Jump to ring 3 (task1) */
    asm volatile("mov %0, %%esp\n\t"
                 "popal\n\t"
                 "iret"
                 : : "r"(task1_ksp));
    while(1);
}

/* ---- User tasks (ring 3) ----------------------------------------------- */
void task1_main(void) {
    asm("mov $1, %eax; mov $'>', %ebx; mov $320, %ecx; int $0x80");
    asm("mov $1, %eax; mov $'T', %ebx; mov $322, %ecx; int $0x80");
    asm("mov $1, %eax; mov $'1', %ebx; mov $324, %ecx; int $0x80");
    while(1) asm("nop");
}
void task2_main(void) {
    asm("mov $1, %eax; mov $'>', %ebx; mov $480, %ecx; int $0x80");
    asm("mov $1, %eax; mov $'T', %ebx; mov $482, %ecx; int $0x80");
    asm("mov $1, %eax; mov $'2', %ebx; mov $484, %ecx; int $0x80");
    while(1) asm("nop");
}