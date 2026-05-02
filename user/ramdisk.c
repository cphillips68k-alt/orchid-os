/* ramdisk.c - Ramdisk server */
typedef struct {
    char name[32];
    unsigned int size;
    const char *data;
} ramdisk_entry;

/* Embedded files – you can add more here */
static const char file1_content[] = "Hello from ramdisk!\n";
static const char file2_content[] = "VGA.putchar('H', 30, 10)";

static const ramdisk_entry files[] = {
    { "README.TXT", sizeof(file1_content)-1, file1_content },
    { "HELLO.NCT", sizeof(file2_content)-1, file2_content },
    { "", 0, 0 }  /* sentinel */
};

void ramdisk_server_main(void) {
    /* no initial screen output */
    while (1) {
        message_t msg;
        asm volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" : : "r"(&msg) : "eax","ebx");
        switch (msg.type) {
        case 1: { /* open(name) -> size, data ptr in reply */
            const char *name = (const char*)msg.data;
            for (int i = 0; files[i].size != 0; i++) {
                int match = 1;
                for (int j = 0; name[j] || files[i].name[j]; j++) {
                    if (name[j] != files[i].name[j]) { match = 0; break; }
                }
                if (match) {
                    message_t reply;
                    reply.type = 1;
                    *(unsigned int*)(reply.data) = files[i].size;
                    asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                                 : : "r"(msg.from), "r"(&reply) : "eax","ebx","ecx");
                    break;
                }
            }
            break;
        }
        case 2: { /* read(name, offset, length) -> data */
            /* for simplicity, just return the whole file content */
            const char *name = (const char*)msg.data;
            for (int i = 0; files[i].size != 0; i++) {
                int match = 1;
                for (int j = 0; name[j] || files[i].name[j]; j++) {
                    if (name[j] != files[i].name[j]) { match = 0; break; }
                }
                if (match) {
                    message_t reply;
                    reply.type = 2;
                    unsigned int len = files[i].size;
                    if (len > MSG_SIZE-2) len = MSG_SIZE-2;
                    for (unsigned int k = 0; k < len; k++)
                        reply.data[k] = files[i].data[k];
                    asm volatile("mov $2, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
                                 : : "r"(msg.from), "r"(&reply) : "eax","ebx","ecx");
                    break;
                }
            }
            break;
        }
        /* other operations (write, create) not implemented */
        }
    }
}