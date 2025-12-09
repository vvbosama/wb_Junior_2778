// kernel/proc.h - 确保包含以下内容
#ifndef _PROC_H_
#define _PROC_H_

#include "types.h"
#include "mm.h"
#include "trap.h"  // 添加这行

#define NPROC 32
#define STACK_SIZE 4096

#define PRIORITY_MIN 0//最小优先级
#define PRIORITY_MAX 10//最大优先级
#define PRIORITY_DEFAULT 5//默认优先级
#define AGING_THRESHOLD 10//aging阈值
#define MLFQ_LEVELS 3//mlfq队列层级

// 进程状态
enum procstate {
    UNUSED = 0,
    USED,
    RUNNABLE,
    RUNNING,
    SLEEPING,
    ZOMBIE
};

// 上下文结构
struct context {
    uint64_t ra;
    uint64_t sp;
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
};

// 进程结构体
struct proc {
    enum procstate state;
    int pid;
    struct context context;
    uint64_t kstack;
    pagetable_t pagetable;
    struct proc *parent;
    void *chan;
    int killed;
    int xstate;
    char name[16];
    struct trap_context *trap_context; // 添加陷阱上下文指针
    uint64_t sz;                       // 进程大小
    int priority;                      // 静态优先级（数值越大越重要）
    int ticks;                         // 已消耗的时间片数量
    int wait_time;                     // 等待时长（用于aging）
    int queue_level;                   // MLFQ 当前队列层级(0最高)
    int queue_ticks;                   // 在当前层级已消耗的时间片数
};

// 系统调用
extern struct proc proc[NPROC];
extern struct proc *curr_proc;
extern volatile int proc_lock;

// 函数声明
void spin_lock(volatile int *lock);
void spin_unlock(volatile int *lock);
void proc_init(void);
struct proc* alloc_proc(void);
int create_process(void (*entry)(void));
void exit_process(int status);
int wait_process(int *status);
void scheduler(void);
void yield(void);
void sleep(void *chan);
void wakeup(void *chan);
int proc_set_priority(int pid, int priority);//设置进程优先级
int proc_get_priority(int pid);//获取进程优先级

#endif