#ifndef _PROC_H_
#define _PROC_H_

#include "types.h"
#include "mm.h"

#define NPROC 32           // 最大进程数
#define STACK_SIZE 4096    // 每个进程的栈大小

// 进程状态
enum procstate {
    UNUSED = 0,
    USED,      // 已分配但未初始化
    RUNNABLE,  // 可运行
    RUNNING,   // 正在运行
    SLEEPING,  // 睡眠中
    ZOMBIE     // 已终止但未回收
};

// 上下文结构 - 保存寄存器状态
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
    enum procstate state;      // 进程状态
    int pid;                   // 进程ID
    struct context context;    // 切换上下文
    uint64_t kstack;           // 内核栈地址
    pagetable_t pagetable;     // 页表
    struct proc *parent;       // 父进程
    void *chan;                // 等待通道
    int killed;                // 是否被杀死
    int xstate;                // 退出状态
    char name[16];             // 进程名
};

// 系统调用
extern struct proc proc[NPROC];        // 进程表
extern struct proc *curr_proc; // 当前进程
extern volatile int proc_lock;         // 进程锁

// 锁函数声明
void spin_lock(volatile int *lock);
void spin_unlock(volatile int *lock);

// 进程管理函数
void proc_init(void);
struct proc* alloc_proc(void);
int create_process(void (*entry)(void));
void exit_process(int status);
int wait_process(int *status);
void scheduler(void);
void yield(void);
void sleep(void *chan);
void wakeup(void *chan);

// 测试函数
void simple_task(void);
void cpu_intensive_task(void);
void producer_task(void);
void consumer_task(void);
void shared_buffer_init(void);

#endif