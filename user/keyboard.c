/* user/keyboard.c - Keyboard server (PID 2) with diagnostic */
#include "common.h"

void kbd_server_main(void) {
    unsigned int focus_pid = PID_SHELL;

    /* Boot markers */
    putchar_at('K', 10);
    putchar_at('B', 12);
    putchar_at('D', 14);

    /* Diagnostic counter — we'll place a star each time we wake up */
    int wake_count = 0;

    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80"
                     : : "r"(&msg) : "eax","ebx");

        /* DIAGNOSTIC: draw a star to prove we woke up */
        putchar_at('*', 20 + wake_count);
        wake_count++;
        if (wake_count > 10) wake_count = 0;   /* wrap */

        /* Now handle the message */
        if (msg.type == 1) {
            unsigned char sc = msg.data[0];
            /* translate scancode -> ASCII (simplified table) */
            unsigned char ascii = 0;
            static const unsigned char table[] =
                "\0\x1b" "1234567890-=" "\b\t" "qwertyuiop[]" "\r"
                "\0" "asdfghjkl;'`" "\0" "\\" "zxcvbnm,./" "\0" "*" "\0" " ";
            if (sc < sizeof(table)) ascii = table[sc];

            if (ascii && focus_pid) {
                message_t out;
                out.type = 1;
                out.data[0] = ascii;
                asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                             : : "r"(focus_pid), "r"(&out) : "eax","ebx","ecx");
            }
        }
    }
}