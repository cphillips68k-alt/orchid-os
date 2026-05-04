#include "types.h"

#define VGA_ADDR  0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CRTC  0x3D4
#define VGA_DATA  0x3D5

static u16 *vga_buffer = (u16*)VGA_ADDR;
static u8 vga_color;
static u32 cursor_x, cursor_y;

enum vga_color_enum {
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

u8 vga_make_color(u8 fg, u8 bg) {
    return (bg << 4) | fg;
}

void vga_set_color(u8 fg, u8 bg) {
    vga_color = vga_make_color(fg, bg);
}

static void vga_update_cursor(void) {
    u16 pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(VGA_CRTC, 0x0F);              // Cursor low byte register
    outb(VGA_DATA, (u8)(pos & 0xFF));
    outb(VGA_CRTC, 0x0E);              // Cursor high byte register
    outb(VGA_DATA, (u8)((pos >> 8) & 0xFF));
}

void vga_clear(void) {
    u16 blank = (u16)' ' | ((u16)vga_color << 8);
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}

void vga_scroll(void) {
    for (u32 i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
        vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
    }
    u16 blank = (u16)' ' | ((u16)vga_color << 8);
    for (u32 i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    cursor_y = VGA_HEIGHT - 1;
    cursor_x = 0;
    vga_update_cursor();
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (u16)' ' | ((u16)vga_color << 8);
        }
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (u16)c | ((u16)vga_color << 8);
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
    vga_update_cursor();
}

void vga_write(const char *s) {
    while (*s) vga_putchar(*s++);
}

void vga_write_dec(u32 n) {
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