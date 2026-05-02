/* user/keyboard.c - Keyboard server (PID 2) */
#include "common.h"

/* Scancode set 1 to ASCII (simplified US layout) */
static unsigned char scancode_to_ascii(unsigned char sc, int shift) {
    static const char normal[] = 
        "\0\x1b" "1234567890-=" "\b\t" "qwertyuiop[]" "\n"
        "\0" "asdfghjkl;'`" "\0" "\\" "zxcvbnm,./" "\0" "*" "\0" " ";
    static const char shifted[] = 
        "\0\x1b" "!@#$%^&*()_+" "\b\t" "QWERTYUIOP{}" "\n"
        "\0" "ASDFGHJKL:\"~" "\0" "|" "ZXCVBNM<>?" "\0" "*" "\0" " ";
    
    if (sc >= sizeof(normal) - 1) return 0;
    return shift ? shifted[sc] : normal[sc];
}

void kbd_server_main(void) {
    unsigned int focus_pid = PID_SHELL;  /* shell gets keyboard by default */
    int shift = 0;
    
    /* Show we're alive */
    putchar_at('K', 180);
    putchar_at('B', 182);
    putchar_at('D', 184);
    
    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80"
                     : : "r"(&msg) : "eax", "ebx");
        
        switch (msg.type) {
            case 1: {  /* scancode from kernel keyboard handler */
                unsigned char sc = msg.data[0];
                
                /* Track shift state */
                if (sc == 0x2A || sc == 0x36) {
                    shift = 1;
                } else if (sc == 0xAA || sc == 0xB6) {
                    shift = 0;
                } else if (sc & 0x80) {
                    /* Key release, ignore */
                } else {
                    unsigned char ascii = scancode_to_ascii(sc, shift);
                    if (ascii && focus_pid) {
                        message_t out;
                        out.type = 1;  /* keypress event */
                        out.data[0] = ascii;
                        out.data[1] = shift;
                        asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                                     : : "r"(focus_pid), "r"(&out) : "eax", "ebx", "ecx");
                    }
                }
                break;
            }
            
            case 2: {  /* set focus to a new PID */
                focus_pid = *(unsigned int*)msg.data;
                break;
            }
        }
    }
}