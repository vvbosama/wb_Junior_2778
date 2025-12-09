// kernel/priority.c - 辅助调度工具与测试任务
#include "priority.h"
#include "printf.h"
#include "proc.h"

extern void exit_process(int status);

static inline int clamp_priority_value(int value) {
    if (value < PRIORITY_MIN) return PRIORITY_MIN;
    if (value > PRIORITY_MAX) return PRIORITY_MAX;
    return value;
}

static void busy_delay(int loops) {
    for (int i = 0; i < loops; i++) {
        for (volatile int j = 0; j < 50000; j++);
        }
    }
    
void priority_init(void) {
    printf("Priority: 优先级调度支持已启用，默认优先级=%d\n", PRIORITY_DEFAULT);
}

int set_priority(int pid, int priority) {
    return proc_set_priority(pid, priority);
}

int get_priority(int pid) {
    return proc_get_priority(pid);
}

int get_dynamic_priority(int pid) {
    // 当前实现静态优先级与动态优先级一致
    return proc_get_priority(pid);
}

int set_nice(int pid, int nice) {
    if (nice < -20 || nice > 19) {
        return -1;
    }
    int mapped = PRIORITY_DEFAULT + (nice / 2);
    mapped = clamp_priority_value(mapped);
    return proc_set_priority(pid, mapped);
    }
    
static int has_runnable_process(void) {
    int runnable = 0;
    spin_lock(&proc_lock);
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].state == RUNNABLE) {
            runnable = 1;
                    break;
                }
            }
    spin_unlock(&proc_lock);
    return runnable;
}

void priority_scheduler(void) {
    printf("Priority scheduler shim: delegating to 核心 scheduler\n");
    while (has_runnable_process()) {
        scheduler();
        }
    }

void show_priority_info(void) {
    printf("\n=== Priority Scheduling Snapshot ===\n");
    spin_lock(&proc_lock);
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc[i];
        if (p->state == UNUSED) {
            continue;
        }
        printf("PID=%d state=%d priority=%d level=%d slice=%d ticks=%d wait=%d\n",
               p->pid, p->state, p->priority, p->queue_level, p->queue_ticks,
               p->ticks, p->wait_time);
        }
    spin_unlock(&proc_lock);
    printf("====================================\n");
}

void high_priority_task(void) {
    if (!curr_proc) {
        return;
    }
    int pid = curr_proc->pid;
    printf("[HIGH] Process %d start\n", pid);
    busy_delay(5);
    printf("[HIGH] Process %d end\n", pid);
    exit_process(0);
}

void medium_priority_task(void) {
    if (!curr_proc) {
        return;
    }
    int pid = curr_proc->pid;
    printf("[MED] Process %d start\n", pid);
    busy_delay(10);
    printf("[MED] Process %d end\n", pid);
    exit_process(0);
}

void low_priority_task(void) {
    if (!curr_proc) {
        return;
    }
    int pid = curr_proc->pid;
    printf("[LOW] Process %d start\n", pid);
    busy_delay(15);
    printf("[LOW] Process %d end\n", pid);
    exit_process(0);
}

void cpu_intensive_priority_task(void) {
    if (!curr_proc) {
        return;
    }
    int pid = curr_proc->pid;
    printf("[CPU] Process %d start\n", pid);
    for (volatile uint64_t i = 0; i < 800000; i++);
    printf("[CPU] Process %d end\n", pid);
    exit_process(0);
}