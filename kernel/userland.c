#include "userland.h"
#include "process.h"

static void sys_write(const char *str, uint32_t len) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(1), "b"(str), "c"(len)
        : "memory"
    );
}

static void sys_exit(void) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(0)
    );
}

static uint32_t user_strlen(const char *s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

static void user_print(const char *str) {
    sys_write(str, user_strlen(str));
}

void userland_main(void) {
    user_print("Hello from Ring 3!\n");
    user_print("User-mode process running.\n");

    for (int i = 0; i < 5; i++) {
        char msg[] = "  User tick: X\n";
        msg[13] = '0' + i;
        sys_write(msg, 15);
        for (volatile int j = 0; j < 2000000; j++);
    }

    user_print("User process exiting.\n");
    sys_exit();

    for (;;);
}

void userland_spawn(void) {
    process_create_user(userland_main, "user_demo");
}
