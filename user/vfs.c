/* vfs.c - Virtual File System server */

#include "common.h"

#define MAX_FD 8
typedef struct {
    unsigned int file_pid;   /* PID of the storage server (ramdisk) */
    unsigned int offset;
    unsigned int size;
} fd_t;

static fd_t fds[MAX_FD];
static int fd_count = 0;

void vfs_server_main(void) {
    /* no initial screen output */
    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" : : "r"(&msg) : "eax","ebx");
        switch (msg.type) {
        case 1: { /* open(name) */
            /* forward to ramdisk */
            message_t req;
            req.type = 1;
            const char *name = (const char*)msg.data;
            for (int i = 0; name[i]; i++) req.data[i] = name[i];
            req.data[31] = 0;
            asm volatile("mov $2, %%eax; mov %1, %%ebx; mov %0, %%ecx; int $0x80"
                         : : "r"(&req), "r"(4) : "eax","ebx","ecx");
            /* wait for reply */
            message_t reply;
            asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" : : "r"(&reply) : "eax","ebx");
            if (reply.type == 1 && fd_count < MAX_FD) {
                fds[fd_count].file_pid = 4;
                fds[fd_count].offset = 0;
                fds[fd_count].size = *(unsigned int*)(reply.data);
                message_t out;
                out.type = 1;
                out.data[0] = fd_count++;
                asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                             : : "r"(msg.from), "r"(&out) : "eax","ebx","ecx");
            }
            break;
        }
        case 2: { /* read(fd, buf, len) */
            unsigned int fd = msg.data[0];
            if (fd < fd_count) {
                /* read from ramdisk */
                message_t req;
                req.type = 2;
                /* copy filename from our stored info? We need to store name too.
                   For simplicity, we'll just request the whole file again. */
                /* This is a cheat – a real VFS would cache the handle */
            }
            break;
        }
        /* other operations */
        }
    }
}