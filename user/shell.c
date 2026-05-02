/* shell.c - Simple shell */
extern void putchar_at(char c, unsigned int off);
void shell_main(void) {
    putchar_at('S', 160);
    putchar_at('H', 162);
    putchar_at(':', 164);
    int cursor_x = 10, cursor_y = 5;
    putchar_at('>', cursor_y*80 + cursor_x);
    cursor_x++;

    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" : : "r"(&msg) : "eax","ebx");
        if (msg.type == 1) {   /* key press from keyboard */
            char c = msg.data[0];
            if (c == '\n') {
                cursor_y++;
                cursor_x = 0;
                putchar_at('>', cursor_y*80 + cursor_x);
                cursor_x = 1;
            } else if (c == '\b') {
                if (cursor_x > 1) {
                    cursor_x--;
                    putchar_at(' ', cursor_y*80 + cursor_x);
                }
            } else if (c >= ' ') {
                putchar_at(c, cursor_y*80 + cursor_x);
                cursor_x++;
                if (cursor_x >= 80) { cursor_x = 0; cursor_y++; }
            }
        }
    }
}