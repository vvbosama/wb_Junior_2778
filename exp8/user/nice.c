#include "types.h"
#include "user.h"

static int str2int(const char *s) {
    int neg = 0;
    int val = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (*s) {
        if (*s < '0' || *s > '9') {
            break;
        }
        val = val * 10 + (*s - '0');
        s++;
    }
    return neg ? -val : val;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf(2, "Usage: nice pid priority\n");
        exit(-1);
    }

    int pid = str2int(argv[1]);
    int prio = str2int(argv[2]);
    if (setpriority(pid, prio) < 0) {
        printf(2, "setpriority failed\n");
        exit(-1);
    }

    exit(0);
}

