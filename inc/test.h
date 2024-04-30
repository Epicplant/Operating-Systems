#include <user.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_BLUE    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"
#define ANSI_ITALIC        "\x1b[3m"
#define ANSI_FG            "\x1b[38;5;11m"

#define KERNBASE 0xFFFFFFFF80000000
#define PGSIZE 0x1000

int stdin = 0;
int stdout = 1;
int stderr = 2;

#define shell_prompt(lab)                                                          \
    do {                                                                           \
        printf(stdout, ANSI_FG "(%s) > " ANSI_COLOR_RESET, lab);                  \
    } while (0)

#define test(name)                                                                 \
    do {                                                                           \
        printf(stdout, "%s -> ", name);                                            \
    } while (0)

#define error(msg, ...)                                                            \
    do {                                                                           \
        printf(stdout, ANSI_COLOR_RED "ERROR: " ANSI_COLOR_RESET);                 \
        printf(stdout, "(line %d) ", __LINE__);                                    \
        printf(stdout, msg, ##__VA_ARGS__);                                        \
        printf(stdout, "\n");                                                      \
        while (1)  {}                                                              \
    } while (0)

#define assert(a)                                                                  \
    do {                                                                           \
        if (!(a)) {                                                                \
            printf(stdout, "Assertion failed (line %d): %s\n", __LINE__, #a);      \
            while (1)  {}                                                          \
        }                                                                          \
    } while (0)

#define pass(msg, ...)                                                             \
    do {                                                                           \
        printf(stdout, ANSI_COLOR_BLUE ANSI_ITALIC "passed " ANSI_COLOR_RESET);    \
        printf(stdout, msg);                                                       \
        printf(stdout, "\n");                                                      \
    } while (0)

