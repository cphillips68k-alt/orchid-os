#include "printf.h"
#include "vga.h"
#include "lib.h"

static void print_dec(u32 n) {
    if (n == 0) {
        vga_putchar('0');
        return;
    }
    char buf[12];
    s32 i = 0;
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i--) vga_putchar(buf[i]);
}

static void print_hex(u32 n) {
    vga_write("0x");
    if (n == 0) {
        vga_putchar('0');
        return;
    }
    char buf[10];
    s32 i = 0;
    while (n > 0) {
        u32 d = n & 0xF;
        buf[i++] = (d < 10) ? ('0' + d) : ('A' + d - 10);
        n >>= 4;
    }
    while (i--) vga_putchar(buf[i]);
}

void printf(const char *fmt, ...) {
    u32 *args = (u32 *)&fmt + 1;
    
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 's': vga_write((const char *)*args++); break;
                case 'c': vga_putchar((char)*args++); break;
                case 'u': print_dec(*args++); break;
                case 'x': print_hex(*args++); break;
                case '%': vga_putchar('%'); break;
                default:  vga_putchar(*fmt); break;
            }
        } else {
            vga_putchar(*fmt);
        }
        fmt++;
    }
}