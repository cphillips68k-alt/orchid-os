/* kernel.c - Orchid OS kernel core */
#define VGA_BUF ((volatile unsigned short *)0xB8000)
#define MAX_PROCS 16
#define MSG_SIZE 128

/* ---- basic I/O (kernel) ---- */
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static void putchar_at(char c, unsigned int off) {
    VGA_BUF[off] = 0x0F00 | (unsigned char)c;
}
static void print_at(const char *s, unsigned int off) {
    while (*s) VGA_BUF[off++] = 0x0F00 | (unsigned char)(*s++);
}

/* ---- message type (exported to services) ---- */
typedef struct {
    unsigned int from;
    unsigned int type;
    unsigned char data[MSG_SIZE];
} message_t;

/* ---- process states ---- */
#define PROC_FREE      0
#define PROC_RUNNING   1
#define PROC_READY     2
#define PROC_SENDING   3
#define PROC_RECEIVING 4
#define STACK_SIZE 4096

typedef struct {
    unsigned int pid, state, ksp;
    unsigned char kstack[STACK_SIZE];
    unsigned char ustack[STACK_SIZE];
    int has_message;
    message_t pending_msg;
    unsigned int blocked_on_pid;
} process_t;

process_t procs[MAX_PROCS];
unsigned int current_pid = 0, next_pid = 1;
static unsigned int code_heap = 0x200000;

/* ---- GDT, TSS, IDT ---- */
struct gdt_entry {
    unsigned short limit_low, base_low;
    unsigned char base_mid, access, granularity, base_high;
} __attribute__((packed));
struct gdt_ptr { unsigned short limit; unsigned int base; } __attribute__((packed));
struct gdt_entry gdt[6];
struct gdt_ptr gp;

struct tss {
    unsigned short link, __unused0;
    unsigned int esp0; unsigned short ss0, __unused1;
    unsigned int esp1, ss1, esp2, ss2;
    unsigned int cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    unsigned short es, __unused2, cs, __unused3, ss, __unused4, ds, __unused5,
                   fs, __unused6, gs, __unused7;
    unsigned short ldt, __unused8, trap, iomap_base;
} __attribute__((packed)) tss;

static void set_gdt_entry(int n, unsigned int base, unsigned int limit,
                           unsigned char access, unsigned char gran) {
    gdt[n].base_low = base & 0xFFFF;
    gdt[n].base_mid = (base >> 16) & 0xFF;
    gdt[n].base_high = (base >> 24) & 0xFF;
    gdt[n].limit_low = limit & 0xFFFF;
    gdt[n].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[n].access = access;
}
extern void reload_segments(void);

struct idt_entry {
    unsigned short base_low, sel;
    unsigned char zero, flags;
    unsigned short base_high;
} __attribute__((packed));
struct idt_ptr { unsigned short limit; unsigned int base; } __attribute__((packed));
struct idt_entry idt[256];
struct idt_ptr ip;

extern void timer_stub(void), keyboard_stub(void), syscall_stub(void);
static void set_idt_gate(int n, unsigned int handler, unsigned short sel, unsigned char flags) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].sel = sel; idt[n].zero = 0; idt[n].flags = flags;
}

/* ---- syscalls ---- */
#define SYS_PUTCHAR 1
#define SYS_SEND    2
#define SYS_RECV    3
#define SYS_GETPID  4
#define SYS_SPAWN   5

struct regs {
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

/* well‑known PIDs (shared across files) */
#define PID_VGA     1
#define PID_KBD     2
#define PID_VFS     3
#define PID_RAMDISK 4
#define PID_SHELL   5
#define PID_RUNTIME 6

/* ---- service entry points (defined in separate .c files) ---- */
extern void vga_server_main(void);
extern void kbd_server_main(void);
extern void vfs_server_main(void);
extern void ramdisk_server_main(void);
extern void shell_main(void);
extern void runtime_service_main(void);

/* ---- scheduler helper ---- */
static int find_next_ready(void) {
    for (int i = 0; i < MAX_PROCS; i++) {
        int pid = (current_pid + 1 + i) % MAX_PROCS;
        if (procs[pid].state == PROC_READY) return pid;
    }
    return current_pid;
}

/* ---- message delivery ---- */
static int deliver_message(unsigned int from, unsigned int to, message_t *msg) {
    if (to >= MAX_PROCS || procs[to].state == PROC_FREE) return -1;
    msg->from = from;
    if (procs[to].state == PROC_RECEIVING) {
        procs[to].pending_msg = *msg;
        procs[to].has_message = 1;
        procs[to].state = PROC_READY;
        procs[to].blocked_on_pid = 0;
        return 1;
    }
    procs[to].pending_msg = *msg;
    procs[to].has_message = 1;
    return 0;
}

/* ---- scheduler ---- */
unsigned int schedule(unsigned int current_esp) {
    procs[current_pid].ksp = current_esp;
    if (procs[current_pid].state == PROC_RUNNING)
        procs[current_pid].state = PROC_READY;
    int next = find_next_ready();
    procs[next].state = PROC_RUNNING;
    current_pid = next;
    return procs[next].ksp;
}

/* ---- timer handler ---- */
unsigned int timer_handler(unsigned int esp) {
    static int ticks = 0;
    ticks++;
    putchar_at('[', 70);
    putchar_at('0' + current_pid, 71);
    putchar_at(']', 72);
    return schedule(esp);
}

/* ---- keyboard handler ---- */
void keyboard_handler(void) {
    unsigned char sc = inb(0x60);
    char hex[] = "0123456789ABCDEF";
    putchar_at(hex[sc >> 4], 78*2);
    putchar_at(hex[sc & 0xF], 78*2 + 1);
}

/* ---- syscall handler ---- */
void syscall_handler(struct regs *r) {
    unsigned int num = r->eax;
    process_t *cur = &procs[current_pid];
    switch (num) {
    case SYS_PUTCHAR:
        putchar_at((char)r->ebx, r->ecx);
        break;
    case SYS_GETPID:
        r->eax = current_pid;
        break;
    case SYS_SEND:
    case SYS_RECV: {
        if (num == SYS_SEND) {
            unsigned int target = r->ebx;
            message_t *msg = (message_t *)r->ecx;
            int result = deliver_message(current_pid, target, msg);
            if (result == 0) {
                cur->state = PROC_SENDING;
                cur->blocked_on_pid = target;
                int next = find_next_ready();
                if (next != current_pid) {
                    procs[current_pid].ksp = (unsigned int)r;
                    current_pid = next;
                    procs[next].state = PROC_RUNNING;
                    asm volatile("mov %0, %%esp\n\tpopal\n\tiret" : : "r"(procs[next].ksp));
                }
            } else if (result < 0) r->eax = -1;
            else r->eax = 0;
        } else {
            message_t *buf = (message_t *)r->ebx;
            if (cur->has_message) {
                *buf = cur->pending_msg;
                cur->has_message = 0;
                unsigned int sender = cur->pending_msg.from;
                if (procs[sender].state == PROC_SENDING &&
                    procs[sender].blocked_on_pid == current_pid) {
                    procs[sender].state = PROC_READY;
                    procs[sender].blocked_on_pid = 0;
                }
                r->eax = 0;
            } else {
                cur->state = PROC_RECEIVING;
                int next = find_next_ready();
                if (next != current_pid) {
                    procs[current_pid].ksp = (unsigned int)r;
                    current_pid = next;
                    procs[next].state = PROC_RUNNING;
                    asm volatile("mov %0, %%esp\n\tpopal\n\tiret" : : "r"(procs[next].ksp));
                }
            }
        }
        break;
    }
    case SYS_SPAWN: {
        unsigned int code_ptr = r->ebx, code_size = r->ecx, stack_sz = r->edx;
        if (!stack_sz) stack_sz = STACK_SIZE;
        int pid = next_pid++;
        if (pid >= MAX_PROCS) { r->eax = 0; break; }
        process_t *p = &procs[pid];
        p->pid = pid; p->state = PROC_READY; p->has_message = 0;
        p->blocked_on_pid = 0;
        unsigned int caddr = code_heap; code_heap += code_size;
        for (unsigned int i = 0; i < code_size; i++)
            ((unsigned char*)caddr)[i] = ((unsigned char*)code_ptr)[i];
        code_heap += stack_sz;
        unsigned int ustack_top = code_heap;
        code_heap -= stack_sz;
        unsigned int *ksp = (unsigned int *)(p->kstack + STACK_SIZE);
        *--ksp = 0x23; *--ksp = ustack_top;
        *--ksp = 0x202; *--ksp = 0x1B;
        *--ksp = caddr;
        for (int i = 0; i < 8; i++) *--ksp = 0;
        p->ksp = (unsigned int)ksp;
        r->eax = pid;
        break;
    }
    default: r->eax = -1;
    }
}

/* ---- spawn a built‑in function ---- */
int spawn_process(void (*entry)(void)) {
    int pid = next_pid++;
    if (pid >= MAX_PROCS) return -1;
    process_t *p = &procs[pid];
    p->pid = pid; p->state = PROC_READY; p->has_message = 0;
    unsigned int *ksp = (unsigned int *)(p->kstack + STACK_SIZE);
    *--ksp = 0x23;
    *--ksp = (unsigned int)(p->ustack + STACK_SIZE);
    *--ksp = 0x202;
    *--ksp = 0x1B;
    *--ksp = (unsigned int)entry;
    for (int i = 0; i < 8; i++) *--ksp = 0;
    p->ksp = (unsigned int)ksp;
    return pid;
}

/* ---- Kernel main ---- */
void kernel_main(void) {
    int i;
    for (i = 0; i < 80*25; i++) VGA_BUF[i] = 0x0F20;
    print_at("Orchid OS", 0);
    for (i = 0; i < 80; i++) putchar_at(0xC4, 80 + i);

    /* GDT */
    set_gdt_entry(0,0,0,0,0);
    set_gdt_entry(1,0,0xFFFFF,0x9A,0xCF);
    set_gdt_entry(2,0,0xFFFFF,0x92,0xCF);
    set_gdt_entry(3,0,0xFFFFF,0xFA,0xCF);
    set_gdt_entry(4,0,0xFFFFF,0xF2,0xCF);
    set_gdt_entry(5,(unsigned int)&tss,sizeof(tss)-1,0x89,0x00);
    gp.limit=sizeof(gdt)-1; gp.base=(unsigned int)gdt;
    asm volatile("lgdt %0" : : "m"(gp));
    reload_segments();
    tss.esp0=0x90000; tss.ss0=0x10;
    asm volatile("mov $0x2B,%ax; ltr %ax");

    /* IDT */
    set_idt_gate(32,(unsigned int)timer_stub,0x08,0x8E);
    set_idt_gate(33,(unsigned int)keyboard_stub,0x08,0x8E);
    set_idt_gate(0x80,(unsigned int)syscall_stub,0x08,0xEE);
    ip.limit=sizeof(idt)-1; ip.base=(unsigned int)idt;
    asm volatile("lidt %0" : : "m"(ip));

    /* PIC */
    outb(0x20,0x11); outb(0xA0,0x11);
    outb(0x21,0x20); outb(0xA1,0x28);
    outb(0x21,0x04); outb(0xA1,0x02);
    outb(0x21,0x01); outb(0xA1,0x01);
    outb(0x21,0xFC); outb(0xA1,0xFF);

    /* PIT */
    outb(0x43,0x36); outb(0x40,11931&0xFF); outb(0x40,11931>>8);

    for(i=0;i<MAX_PROCS;i++) procs[i].state=PROC_FREE;
    procs[0].pid=0; procs[0].state=PROC_RUNNING; current_pid=0;

    /* Spawn ring‑3 services in order of their PID */
    spawn_process(vga_server_main);      /* becomes PID 1 */
    spawn_process(kbd_server_main);      /* becomes PID 2 */
    spawn_process(vfs_server_main);      /* becomes PID 3 */
    spawn_process(ramdisk_server_main);  /* becomes PID 4 */
    spawn_process(shell_main);           /* becomes PID 5 */
    spawn_process(runtime_service_main); /* becomes PID 6 */

    /* Start the first service */
    current_pid = 1;
    procs[1].state = PROC_RUNNING;
    asm volatile("mov %0, %%esp\n\tpopal\n\tiret" : : "r"(procs[1].ksp));
    while(1);
}