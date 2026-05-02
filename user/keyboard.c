/* keyboard.c - Keyboard server */
extern void putchar_at(char c, unsigned int off);
static unsigned char scancode_to_ascii(unsigned char sc, int shift) {
    static const char normal[] = 
        "\0\x1b" "1234567890-=" "\b\t" "qwertyuiop[]" "\n"
        "\0" "asdfghjkl;'`" "\0" "\\" "zxcvbnm,./" "\0" "*" "\0" " ";
    if (sc < sizeof(normal)-1) return normal[sc];
    return 0;
}

void kbd_server_main(void) {
    unsigned int focus_pid = 5; /* shell has PID 5 */
    putchar_at('K', 180);
    putchar_at('B', 182);
    putchar_at('D', 184);

    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" : : "r"(&msg) : "eax","ebx");
        if (msg.type == 1) {  /* hardware interrupt forward (scancode) */
            unsigned char sc = msg.data[0];
            unsigned char ascii = scancode_to_ascii(sc, 0);
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