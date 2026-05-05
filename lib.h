#ifndef LIB_H
#define LIB_H

#include "types.h"

void *memset(void *dst, s32 val, u32 len);
void *memcpy(void *dst, const void *src, u32 len);
u32   strlen(const char *s);
s32   strcmp(const char *a, const char *b);
s32   strcmp_ci(const char *a, const char *b);

#endif