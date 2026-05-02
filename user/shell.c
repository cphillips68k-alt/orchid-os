/* user/shell.c - Simple shell (PID 5) */
#include "common.h"

void shell_main(void) {
    int cursor_x = 0;
    int cursor_y = 2;

    /* Display prompt */
    putchar_at('>', cursor_y * 80 + cursor_x);
    cursor_x++;

    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80"
                     : : "r"(&msg) : "eax","ebx");

        if (msg.type == 1) {
            char c = msg.data[0];

            if (c == '\r' || c == '\n') {
                /* New line */
                cursor_y++;
                cursor_x = 0;
                putchar_at('>', cursor_y * 80 + cursor_x);
                cursor_x = 1;
            } else if (c == '\b') {
                /* Backspace */
                if (cursor_x > 1) {
                    cursor_x--;
                    putchar_at(' ', cursor_y * 80 + cursor_x);
                }
            } else if (c >= ' ') {
                /* Printable character */
                putchar_at(c, cursor_y * 80 + cursor_x);
                cursor_x++;
                if (cursor_x >= 80) {
                    cursor_x = 0;
                    cursor_y++;
                }
            }
        }
    }
}