/* kernel.c - Orchid OS Phase 1: Process table + SEND/RECV
 * Everything in ring 3. Kernel does scheduler + interrupts + messages.
 */

#define VGA_BUF ((volatile unsigned short *)0xB8000)
#define MAX_PROCS 16
#define MSG_SIZE 64

/* ---- basic I/O (kernel-internal only, VGA server takes over later) ---- */
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

/* ---- Message type ------------------------------------------------------ */
typedef struct {
    unsigned int from;               /* sender PID */
    unsigned int type;               /* message type (user-defined) */
    unsigned char data[MSG_SIZE];    /* payload */
} message_t;

/* ---- Process states ---------------------------------------------------- */
#define PROC_FREE    0    /* slot is unused */
#define PROC_RUNNING 1    /* currently on CPU (only one at a time) */
#define PROC_READY   2    /* ready to run */
#define PROC_SENDING 3    /* blocked trying to send */
#define PROC_RECEIVING 4  /* blocked waiting for a message */

/* ---- Process table entry ----------------------------------------------- */
#define STACK_SIZE 4096

typedef struct {
    unsigned int pid;
    unsigned int state;
    unsigned int ksp;                /* saved kernel stack pointer */
    unsigned char kstack[STACK_SIZE]; /* kernel stack */
    unsigned char ustack[STACK_SIZE]; /* user stack (ring 3) */
    
    /* Message queue (simple: one pending message) */
    int has_message;
    message_t pending_msg;
    
    /* Blocking state */
    unsigned int blocked_on_pid;     /* who are we waiting for? */
} process_t;

process_t procs[MAX_PROCS];
unsigned int current_pid = 0;
unsigned int next_pid = 1;

/* ---- GDT & TSS --------------------------------------------------------- */
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

/* ---- IDT ----------------------------------------------------------------- */
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

/* ---- Syscall numbers ----------------------------------------------------- */
#define SYS_PUTCHAR  1
#define SYS_SEND     2
#define SYS_RECV     3
#define SYS_GETPID   4

/* ---- Registers passed from syscall stub ---------------------------------- */
struct regs {
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

/* ---- Forward declarations ------------------------------------------------ */
void vga_server_main(void);
void kbd_server_main(void);
void test_task_main(void);
unsigned int schedule(unsigned int current_esp);
unsigned int timer_handler(unsigned int esp);
void keyboard_handler(void);
void syscall_handler(struct regs *r);

/* ---- Helper: find next ready process ------------------------------------- */
static int find_next_ready(void) {
    for (int i = 0; i < MAX_PROCS; i++) {
        int pid = (current_pid + 1 + i) % MAX_PROCS;
        if (procs[pid].state == PROC_READY) return pid;
    }
    /* No ready process? Return current (idle) */
    return current_pid;
}

/* ---- Kernel message delivery --------------------------------------------- */
/* Called by SEND syscall: try to deliver msg to target.
 * Returns 1 if delivered immediately (receiver was RECEIVING),
 * 0 if queued and sender should block. */
static int deliver_message(unsigned int from, unsigned int to, message_t *msg) {
    if (to >= MAX_PROCS || procs[to].state == PROC_FREE) return -1;
    
    msg->from = from;
    
    /* Is the target waiting for a message? */
    if (procs[to].state == PROC_RECEIVING) {
        /* Deliver directly, wake up target */
        procs[to].pending_msg = *msg;
        procs[to].has_message = 1;
        procs[to].state = PROC_READY;
        procs[to].blocked_on_pid = 0;
        return 1;  /* delivered, sender doesn't block */
    }
    
    /* Target is busy or sending. Queue the message, sender blocks. */
    procs[to].pending_msg = *msg;
    procs[to].has_message = 1;
    return 0;  /* sender blocks */
}

/* ---- Scheduler ----------------------------------------------------------- */
unsigned int schedule(unsigned int current_esp) {
    procs[current_pid].ksp = current_esp;
    
    /* Mark current as READY if it was RUNNING */
    if (procs[current_pid].state == PROC_RUNNING) {
        procs[current_pid].state = PROC_READY;
    }
    
    /* Pick next ready process */
    int next = find_next_ready();
    
    procs[next].state = PROC_RUNNING;
    current_pid = next;
    
    return procs[next].ksp;
}

/* ---- Timer handler ------------------------------------------------------- */
unsigned int timer_handler(unsigned int esp) {
    static int ticks = 0;
    ticks++;
    /* Status bar: show running PID and tick count */
    putchar_at('[', 70);
    putchar_at('0' + current_pid, 71);
    putchar_at(']', 72);
    putchar_at('0' + ((ticks/10) % 10), 74);
    putchar_at('0' + (ticks % 10), 75);
    putchar_at('H', 77);
    putchar_at('z', 78);
    return schedule(esp);
}

/* ---- Keyboard handler ---------------------------------------------------- */
void keyboard_handler(void) {
    unsigned char sc = inb(0x60);
    /* Show scancode in bottom-right for debugging */
    char hex[] = "0123456789ABCDEF";
    putchar_at(hex[sc >> 4], 78*2);
    putchar_at(hex[sc & 0xF], 78*2 + 1);
}

/* ---- Syscall handler ----------------------------------------------------- */
void syscall_handler(struct regs *r) {
    unsigned int syscall_num = r->eax;
    process_t *cur = &procs[current_pid];
    
    switch (syscall_num) {
        case SYS_PUTCHAR: {
            /* Direct screen write (temporary, until VGA server is ready) */
            putchar_at((char)r->ebx, r->ecx);
            break;
        }
        
        case SYS_GETPID: {
            r->eax = current_pid;
            break;
        }
        
        case SYS_SEND: {
            unsigned int target_pid = r->ebx;
            message_t *msg = (message_t *)r->ecx;
            
            /* Validate target */
            if (target_pid >= MAX_PROCS || procs[target_pid].state == PROC_FREE) {
                r->eax = (unsigned int)-1;
                break;
            }
            
            int result = deliver_message(current_pid, target_pid, msg);
            if (result == 1) {
                /* Delivered immediately */
                r->eax = 0;
            } else if (result == 0) {
                /* Target busy, sender blocks */
                cur->state = PROC_SENDING;
                cur->blocked_on_pid = target_pid;
                /* Switch to another process */
                unsigned int next = find_next_ready();
                if (next != current_pid) {
                    procs[current_pid].ksp = (unsigned int)r;  /* save state */
                    procs[current_pid].state = PROC_SENDING;
                    current_pid = next;
                    procs[next].state = PROC_RUNNING;
                    /* Return to new process's saved context */
                    /* (This is tricky - we need to switch stacks here) */
                    asm volatile("mov %0, %%esp\n\t"
                                 "popal\n\t"
                                 "iret"
                                 : : "r"(procs[next].ksp));
                }
                r->eax = 0;
            } else {
                r->eax = (unsigned int)-1;
            }
            break;
        }
        
        case SYS_RECV: {
            /* Do we have a pending message? */
            if (cur->has_message) {
                /* Copy to user buffer */
                message_t *dest = (message_t *)r->ebx;
                *dest = cur->pending_msg;
                cur->has_message = 0;
                
                /* If someone was blocked sending to us, wake them */
                /* (Simplified: just mark sender as ready) */
                unsigned int sender = cur->pending_msg.from;
                if (procs[sender].state == PROC_SENDING && 
                    procs[sender].blocked_on_pid == current_pid) {
                    procs[sender].state = PROC_READY;
                    procs[sender].blocked_on_pid = 0;
                }
                
                r->eax = 0;
            } else {
                /* No message. Block and switch. */
                cur->state = PROC_RECEIVING;
                int next = find_next_ready();
                if (next != current_pid) {
                    procs[current_pid].ksp = (unsigned int)r;
                    current_pid = next;
                    procs[next].state = PROC_RUNNING;
                    asm volatile("mov %0, %%esp\n\t"
                                 "popal\n\t"
                                 "iret"
                                 : : "r"(procs[next].ksp));
                }
                r->eax = 0;
            }
            break;
        }
        
        default:
            r->eax = (unsigned int)-1;
            break;
    }
}

/* ---- Set up a process in the table --------------------------------------- */
static int spawn_process(void (*entry)(void)) {
    int pid = next_pid++;
    if (pid >= MAX_PROCS) return -1;
    
    process_t *p = &procs[pid];
    p->pid = pid;
    p->state = PROC_READY;
    p->has_message = 0;
    p->blocked_on_pid = 0;
    
    /* Build kernel stack with iret frame to ring 3 */
    unsigned int *ksp = (unsigned int *)(p->kstack + STACK_SIZE);
    *--ksp = 0x23;                          /* SS (user data) */
    *--ksp = (unsigned int)(p->ustack + STACK_SIZE); /* user ESP */
    *--ksp = 0x202;                         /* EFLAGS (IF set) */
    *--ksp = 0x1B;                          /* CS (user code) */
    *--ksp = (unsigned int)entry;           /* EIP */
    for (int i = 0; i < 8; i++) *--ksp = 0; /* dummy pushad */
    p->ksp = (unsigned int)ksp;
    
    return pid;
}

/* ---- Kernel main --------------------------------------------------------- */
void kernel_main(void) {
    int i;
    for (i = 0; i < 80*25; i++) VGA_BUF[i] = 0x0F20;
    print_at("Orchid OS Phase 1 - Microkernel + SEND/RECV", 0);
    for (i = 0; i < 80; i++) putchar_at(0xC4, 80 + i);
    print_at("Scheduler: [ ]    Hz  Key:", 24*80);

    /* GDT */
    set_gdt_entry(0, 0, 0, 0, 0);
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xCF);
    set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);
    set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);
    set_gdt_entry(5, (unsigned int)&tss, sizeof(tss)-1, 0x89, 0x00);
    gp.limit = sizeof(gdt)-1; gp.base = (unsigned int)gdt;
    asm volatile("lgdt %0" : : "m"(gp));
    reload_segments();

    /* TSS */
    tss.esp0 = 0x90000; tss.ss0 = 0x10;
    asm volatile("mov $0x2B, %ax; ltr %ax");

    /* IDT */
    set_idt_gate(32, (unsigned int)timer_stub, 0x08, 0x8E);
    set_idt_gate(33, (unsigned int)keyboard_stub, 0x08, 0x8E);
    set_idt_gate(0x80, (unsigned int)syscall_stub, 0x08, 0xEE);
    ip.limit = sizeof(idt)-1; ip.base = (unsigned int)idt;
    asm volatile("lidt %0" : : "m"(ip));

    /* PIC */
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC); outb(0xA1, 0xFF);

    /* PIT */
    outb(0x43, 0x36);
    outb(0x40, 11931 & 0xFF);
    outb(0x40, 11931 >> 8);

    /* Clear process table */
    for (i = 0; i < MAX_PROCS; i++) {
        procs[i].state = PROC_FREE;
        procs[i].has_message = 0;
    }

    /* Spawn servers and test task */
    int vga_pid = spawn_process(vga_server_main);
    int kbd_pid = spawn_process(kbd_server_main);
    int test_pid = spawn_process(test_task_main);

    /* Set kernel as PID 0 (idle/running) */
    procs[0].pid = 0;
    procs[0].state = PROC_RUNNING;
    current_pid = 0;

    /* Jump to first real process (VGA server) */
    current_pid = vga_pid;
    procs[vga_pid].state = PROC_RUNNING;
    asm volatile("mov %0, %%esp\n\t"
                 "popal\n\t"
                 "iret"
                 : : "r"(procs[vga_pid].ksp));
    while(1);
}

/* ---- VGA server (ring 3) ------------------------------------------------- */
void vga_server_main(void) {
    /* For now, just write a message to show we're alive */
    unsigned int my_pid;
    asm volatile("mov $4, %%eax; int $0x80; mov %%eax, %0" : "=r"(my_pid));
    
    /* Write to screen directly using PUTCHAR (temporary) */
    asm volatile("mov $1, %%eax; mov $'V', %%ebx; mov $170, %%ecx; int $0x80");
    asm volatile("mov $1, %%eax; mov $'G', %%ebx; mov $172, %%ecx; int $0x80");
    asm volatile("mov $1, %%eax; mov $'A', %%ebx; mov $174, %%ecx; int $0x80");
    
    /* Now wait for messages and process them */
    while(1) {
        message_t msg;
        /* RECV from anyone (0 = wildcard for now) */
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" 
                     : : "r"(&msg) : "eax", "ebx");
        /* We'd process the message here */
    }
}

/* ---- Keyboard server (ring 3) -------------------------------------------- */
void kbd_server_main(void) {
    unsigned int my_pid;
    asm volatile("mov $4, %%eax; int $0x80; mov %%eax, %0" : "=r"(my_pid));
    
    asm volatile("mov $1, %%eax; mov $'K', %%ebx; mov $180, %%ecx; int $0x80");
    asm volatile("mov $1, %%eax; mov $'B', %%ebx; mov $182, %%ecx; int $0x80");
    asm volatile("mov $1, %%eax; mov $'D', %%ebx; mov $184, %%ecx; int $0x80");
    
    while(1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80"
                     : : "r"(&msg) : "eax", "ebx");
    }
}

/* ---- Test task (ring 3) -------------------------------------------------- */
void test_task_main(void) {
    unsigned int my_pid;
    asm volatile("mov $4, %%eax; int $0x80; mov %%eax, %0" : "=r"(my_pid));
    
    /* Print on row 3 */
    int offset = 240;
    char *msg = "Hello from ring 3 test process!";
    for (int i = 0; msg[i]; i++) {
        asm volatile("mov $1, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                     : : "r"((int)msg[i]), "r"(offset + i));
    }
    
    while(1) asm("nop");
}