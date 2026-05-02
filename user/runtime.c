/* runtime.c - Nectar language runtime */

#include "common.h"

void runtime_service_main(void) {
    /* placeholder, will JIT later */
    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" : : "r"(&msg) : "eax","ebx");
        /* will process source code requests */
    }
}