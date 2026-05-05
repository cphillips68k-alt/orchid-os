#include "shell.h"
#include "types.h"
#include "vga.h"
#include "printf.h"
#include "lib.h"
#include "pmm.h"
#include "fs.h"

#define MAX_CMD_LEN  64
#define MAX_ARGS     8
#define MAX_CMDS     32

typedef void (*cmd_func_t)(s32 argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    cmd_func_t  func;
} shell_cmd_t;

static shell_cmd_t cmd_table[MAX_CMDS];
static s32         cmd_count = 0;
static char        input_buf[MAX_CMD_LEN];
static s32         input_pos = 0;

static void shell_register(const char *name, const char *help, cmd_func_t func) {
    if (cmd_count >= MAX_CMDS) return;
    cmd_table[cmd_count].name = name;
    cmd_table[cmd_count].help = help;
    cmd_table[cmd_count].func = func;
    cmd_count++;
}

// ---- BUILT-IN COMMANDS ----

static void cmd_help(s32 argc, char **argv) {
    (void)argc; (void)argv;
    printf("Orchid OS Shell Commands:\n");
    for (s32 i = 0; i < cmd_count; i++) {
        printf("  %s - %s\n", cmd_table[i].name, cmd_table[i].help);
    }
}

static void cmd_clear(s32 argc, char **argv) {
    (void)argc; (void)argv;
    vga_clear();
}

static void cmd_mem(s32 argc, char **argv) {
    (void)argc; (void)argv;
    printf("Total: %u MB  Free: %u MB\n", pmm_total_mb(), pmm_free_mb());
}

static void cmd_echo(s32 argc, char **argv) {
    for (s32 i = 1; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");
}

static void cmd_halt(s32 argc, char **argv) {
    (void)argc; (void)argv;
    printf("Halting.\n");
    asm volatile("cli; hlt");
}

static void cmd_ls(s32 argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "/";
    char name[FS_MAX_NAME];
    u32 size;
    bool is_dir;
    u32 idx = 0;
    
    while (fs_list(path, name, &size, &is_dir, idx)) {
        printf("%s", name);
        if (is_dir) printf("/");
        printf("  (%u bytes)\n", size);
        idx++;
    }
}

static void cmd_cat(s32 argc, char **argv) {
    if (argc < 2) {
        printf("Usage: cat <file>\n");
        return;
    }
    
    s32 fd = fs_open(argv[1]);
    if (fd < 0) {
        printf("File not found: %s\n", argv[1]);
        return;
    }
    
    u8 buf[128];
    s32 bytes;
    while ((bytes = fs_read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes] = '\0';
        printf("%s", buf);
    }
    printf("\n");
    fs_close(fd);
}

// ---- INPUT HANDLING ----

static void shell_execute(const char *line) {
    char args_buf[MAX_CMD_LEN];
    char *argv[MAX_ARGS];
    s32 argc = 0;

    // Copy so we can tokenize
    memcpy(args_buf, line, MAX_CMD_LEN);
    args_buf[MAX_CMD_LEN - 1] = '\0';

    char *p = args_buf;
    while (*p && argc < MAX_ARGS) {
        // Skip whitespace
        while (*p == ' ') p++;
        if (!*p) break;

        argv[argc++] = p;

        // Find end of token
        while (*p && *p != ' ') p++;
        if (*p) { *p = '\0'; p++; }
    }

    if (argc == 0) return;

    // Find command
    for (s32 i = 0; i < cmd_count; i++) {
        if (strcmp(argv[0], cmd_table[i].name) == 0) {
                cmd_table[i].func(argc, argv);
            return;
        }
    }

    printf("Unknown command: %s\nType 'help' for a list.\n", argv[0]);
}

void shell_handle_key(char c) {
        if (c == '\n' || c == '\r') {
        vga_putchar('\n');
        input_buf[input_pos] = '\0';
        shell_execute(input_buf);
        input_pos = 0;
        shell_prompt();
    } else if (c == '\b') {
        if (input_pos > 0) {
            input_pos--;
            vga_putchar('\b');
            vga_putchar(' ');
            vga_putchar('\b');
        }
    } else if (c >= ' ' && c <= '~' && input_pos < MAX_CMD_LEN - 1) {
        input_buf[input_pos++] = c;
        vga_putchar(c);
    }
}

// ---- PUBLIC API ----

void shell_prompt(void) {
    printf("! ");
}

void shell_init(void) {
    shell_register("help",  "Show available commands", cmd_help);
    shell_register("clear", "Clear the screen", cmd_clear);
    shell_register("mem",   "Show memory stats", cmd_mem);
    shell_register("echo",  "Print arguments", cmd_echo);
    shell_register("halt",  "Halt the system", cmd_halt);    
    shell_register("ls",   "List directory contents", cmd_ls);
    shell_register("cat",  "Print file contents", cmd_cat);

    input_pos = 0;
}

void shell_run(void) {
    // Shell is keyboard-driven. For now, this is a placeholder.
    // Real input comes from the keyboard IRQ calling shell_handle_key().
}