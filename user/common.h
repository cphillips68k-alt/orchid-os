/* user/common.h - Shared types and constants for ring 3 services */

#define MSG_SIZE 128

typedef struct {
    unsigned int from;
    unsigned int type;
    unsigned char data[MSG_SIZE];
} message_t;

/* syscall numbers */
#define SYS_PUTCHAR 1
#define SYS_SEND    2
#define SYS_RECV    3
#define SYS_GETPID  4
#define SYS_SPAWN   5

/* well‑known PIDs */
#define PID_VGA     1
#define PID_KBD     2
#define PID_VFS     3
#define PID_RAMDISK 4
#define PID_SHELL   5
#define PID_RUNTIME 6

/* kernel utility functions available to services */
void putchar_at(char c, unsigned int off);
void print_at(const char *s, unsigned int off);