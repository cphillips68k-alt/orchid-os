#ifndef VGA_H
#define VGA_H

#include "types.h"

enum vga_color {
    VGA_BLACK,
    VGA_BLUE,
    VGA_GREEN,
    VGA_CYAN,
    VGA_RED,
    VGA_MAGENTA,
    VGA_BROWN,
    VGA_LGRAY,
    VGA_DGRAY,
    VGA_LBLUE,
    VGA_LGREEN,
    VGA_LCYAN,
    VGA_LRED,
    VGA_LMAGENTA,
    VGA_YELLOW,
    VGA_WHITE,
};

void vga_clear(void);
void vga_set_color(u8 fg, u8 bg);
void vga_putchar(char c);
void vga_write(const char *s);
void vga_write_dec(u32 n);

#endif