/* user/keyboard.c - Keyboard server (PID 2) */
#include "common.h"

/* Scancode set 1 to ASCII (US layout) */
static unsigned char sc_to_ascii(unsigned char sc) {
    static const unsigned char table[] = 
        "\0\x1b" "1234567890-=" "\b\t" "qwertyuiop[]" "\r"
        "\0" "asdfghjkl;'`" "\0" "\\" "zxcvbnm,./" "\0" "*" "\0" " ";
    if (sc < 58) return table[sc];
    return 0;
}

void kbd_server_main(void) {
    unsigned int focus_pid = PID_SHELL;

    /* Boot message via VGA */
    message_t out;
    out.type = 1;
    out.data[0] = 'K';
    *(unsigned int*)(out.data+1) = 10;
    *(unsigned int*)(out.data+5) = 1;
    asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                 : : "r"(PID_VGA), "r"(&out) : "eax","ebx","ecx");

    out.data[0] = 'B';
    *(unsigned int*)(out.data+1) = 11;
    asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                 : : "r"(PID_VGA), "r"(&out) : "eax","ebx","ecx");

    out.data[0] = 'D';
    *(unsigned int*)(out.data+1) = 12;
    asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                 : : "r"(PID_VGA), "r"(&out) : "eax","ebx","ecx");

    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80"
                     : : "r"(&msg) : "eax","ebx");

        if (msg.type == 1) {
            unsigned char sc = msg.data[0];
            unsigned char ascii = sc_to_ascii(sc);

            if (ascii && focus_pid) {
                /* Forward ASCII character to the focused process */
                out.type = 1;
                out.data[0] = ascii;
                asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                             : : "r"(focus_pid), "r"(&out) : "eax","ebx","ecx");
            }
        }
    }
}