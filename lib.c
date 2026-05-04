#include "lib.h"

void *memset(void *dst, s32 val, u32 len) {
    u8 *d = (u8 *)dst;
    while (len--) *d++ = (u8)val;
    return dst;
}

void *memcpy(void *dst, const void *src, u32 len) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (len--) *d++ = *s++;
    return dst;
}

u32 strlen(const char *s) {
    u32 i = 0;
    while (s[i]) i++;
    return i;
}

s32 strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}