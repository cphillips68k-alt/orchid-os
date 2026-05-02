/* vga.c - VGA server */

#include "common.h"

extern void putchar_at(char c, unsigned int off);  /* kernel function, still accessible because flat memory */
extern int spawn_process(void (*entry)()); /* not needed here, but included for completeness */

void vga_server_main(void) {
    putchar_at('V', 170);
    putchar_at('G', 172);
    putchar_at('A', 174);

    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" : : "r"(&msg) : "eax","ebx");
        if (msg.type == 1) {
            char c = msg.data[0];
            unsigned int x = *(unsigned int*)(msg.data+1);
            unsigned int y = *(unsigned int*)(msg.data+5);
            putchar_at(c, y*80 + x);
        }
    }
}