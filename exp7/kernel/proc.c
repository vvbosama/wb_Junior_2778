#include "proc.h"
#include "printf.h"
#include "mm.h"
#include "trap.h"
#include "clock.h"
#include "priority.h"

// 简易关机：QEMU virt/sifive 测试器（finisher），若存在则可用于退出仿真
#define QEMU_FINISHER_ADDR 0x100000UL
#define QEMU_FINISHER_PASS 0x5555
#define QEMU_FINISHER_FAIL 0x3333
static inline void qemu_poweroff_success(void) {
    volatile unsigned int *fin = (volatile unsigned int *)QEMU_FINISHER_ADDR;
    *fin = QEMU_FINISHER_PASS;
}


// 添加外部函数声明 - 使用新的函数名
extern void context_switch(struct context *old, struct context *new);

struct proc proc[NPROC];
struct proc *curr_proc = 0;
static int next_pid = 1;
static struct context scheduler_context; // 调度器自身的上下文

// 简单的自旋锁
inline void spin_lock(volatile int *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {}// 原子测试并设置
}

inline void spin_unlock(volatile int *lock) {
    __sync_lock_release(lock);// 原子释放锁
}

volatile int proc_lock = 0;// 进程表锁

// 调度器循环函数
void scheduler_loop(void) {
    printf("Scheduler: entered scheduler loop\n");
    
    while (1) {
        scheduler();
        // 如果没有可运行进程，等待中断
        asm volatile("wfi");
    }
}


// 进程初始化
void proc_init(void) {
    printf("Process: initializing process table with %d slots\n", NPROC);
    
    for (int i = 0; i < NPROC; i++) {
        proc[i].state = UNUSED;
        proc[i].pid = 0;
        proc[i].kstack = 0;
        proc[i].pagetable = 0;
        proc[i].parent = 0;
        proc[i].chan = 0;// 睡眠通道
        proc[i].killed = 0;
        proc[i].xstate = 0;// 退出状态
        proc[i].name[0] = '\0';//进程名
    }

    // 正确初始化调度器上下文
    scheduler_context.ra = (uint64_t)scheduler_loop;
    scheduler_context.sp = (uint64_t)alloc_page() + PAGE_SIZE;  // 分配调度器栈
    
    printf("Process: process table initialized\n");
}


// 分配进程结构
struct proc* alloc_proc(void) {
    spin_lock(&proc_lock);// 加锁保护进程表
    
    struct proc *p = 0;
    // 查找空闲进程槽
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].state == UNUSED) {
            p = &proc[i];
            break;
        }
    }
    
    if (p) {
        // 分配内核栈
        void *stack = alloc_page();
        if (!stack) {
            spin_unlock(&proc_lock);
            printf("Process: failed to allocate stack for new process\n");
            return NULL;
        }
        
        p->state = USED;
        p->pid = next_pid++;
        p->kstack = (uint64_t)stack;
        p->pagetable = kernel_pagetable;
        p->parent = curr_proc;
        p->killed = 0;
        p->xstate = 0;
        
        // 手动构建进程名 "procX"
        char *name = p->name;
        name[0] = 'p';
        name[1] = 'r';
        name[2] = 'o';
        name[3] = 'c';
        
        // 手动转换 PID 为字符串
        int pid = p->pid;
        int pos = 4;
        char buf[8];
        int i = 0;
        
        // 处理 PID 为 0 的情况
        if (pid == 0) {
            buf[i++] = '0';
        } else {
            // 提取数字
            while (pid > 0 && i < 7) {
                buf[i++] = '0' + (pid % 10);
                pid /= 10;
            }
        }
        
        // 反转数字
        while (i > 0 && pos < 15) {
            name[pos++] = buf[--i];
        }
        name[pos] = '\0';
        
        printf("Process: allocated process %d (%s), parent=%s\n", 
               p->pid, p->name, p->parent ? p->parent->name : "main");
    } else {
        printf("Process: process table full, cannot allocate new process\n");
    }
    
    spin_unlock(&proc_lock);
    return p;
}

// 创建新进程
int create_process(void (*entry)(void)) {
    struct proc *p = alloc_proc();
    if (!p) {
        printf("Process: failed to allocate process\n");
        return -1;
    }
    
    // 设置内核栈
    uint64_t stack_top = p->kstack + PAGE_SIZE;
    
    // 设置上下文，使进程从指定入口开始执行
    // 重要：设置返回地址为进程入口
    p->context.ra = (uint64_t)entry;//返回地址为进程入口
    p->context.sp = stack_top;//栈指针为栈顶
    
    // 初始化其他寄存器为0
    p->context.s0 = 0;
    p->context.s1 = 0;
    p->context.s2 = 0;
    p->context.s3 = 0;
    p->context.s4 = 0;
    p->context.s5 = 0;
    p->context.s6 = 0;
    p->context.s7 = 0;
    p->context.s8 = 0;
    p->context.s9 = 0;
    p->context.s10 = 0;
    p->context.s11 = 0;
    
    // 设置为可运行状态
    p->state = RUNNABLE;
    
    printf("Process: created process %d, entry=%p, stack=%p\n", 
           p->pid, (void*)entry, (void*)stack_top);
    
    return p->pid;
}

// 进程退出
void exit_process(int status) {
    if (curr_proc == 0) {
        printf("Process: no current process to exit\n");
        return;
    }
    
    printf("Process %d: exiting with status %d\n", curr_proc->pid, status);
    
    curr_proc->xstate = status;
    curr_proc->state = ZOMBIE;
    curr_proc->killed = 0;
    
    // 唤醒父进程
    if (curr_proc->parent) {
        wakeup(curr_proc->parent);
    }
    
    // 直接切回调度器上下文，彻底离开该进程上下文
    printf("Process %d: yielding to scheduler\n", curr_proc->pid);
    // 直接调用调度器，而不是上下文切换
    // struct proc *old_proc = curr_proc;
    curr_proc = 0;
    
    // 直接调用调度器，让它选择下一个进程
    scheduler();
    
    // 不应该到达这里
    printf("ERROR: returned from scheduler after exit!\n");
    for (;;) { asm volatile("wfi"); }
}


// 在 proc.c 中找到 wait_process 函数，修改如下：

int wait_process(int *status) {
    printf("DEBUG: wait_process called, curr_proc=%s\n", 
           curr_proc ? curr_proc->name : "NULL");
    
    if (!curr_proc) {
        // 修改后的逻辑：回收任何僵尸进程
        struct proc *p;
        int found = 0;
        
        spin_lock(&proc_lock);
        for (int i = 0; i < NPROC; i++) {
            p = &proc[i];
            if (p->state == ZOMBIE) {
                found = 1;
                printf("DEBUG: found zombie process %d to reap\n", p->pid);
                break;
            }
        }
        spin_unlock(&proc_lock);
        
        if (found) {
            if (status) {
                *status = p->xstate;
            }
            
            // 释放进程资源
            p->state = UNUSED;
            if (p->kstack) {
                free_page((void*)p->kstack);
            }
            
            printf("Process: reaped zombie process %d\n", p->pid);
            return p->pid;
        }
        printf("DEBUG: no zombie processes found\n");
        return -1;
    }
    
    printf("DEBUG: current process %d waiting for children\n", curr_proc->pid);
    
    while (1) {
        int found = 0;
        struct proc *p;
        
        spin_lock(&proc_lock);
        // 查找僵尸状态的子进程
        for (int i = 0; i < NPROC; i++) {
            p = &proc[i];
            if (p->state == ZOMBIE && p->parent == curr_proc) {
                found = 1;
                printf("DEBUG: found child zombie process %d\n", p->pid);
                break;
            }
        }
        spin_unlock(&proc_lock);
        
        if (found) {
            if (status) {
                *status = p->xstate;
            }
            
            // 释放进程资源
            p->state = UNUSED;
            if (p->kstack) {
                free_page((void*)p->kstack);
            }
            
            printf("Process: reaped process %d with status %d\n", p->pid, p->xstate);
            return p->pid;
        }
        
        printf("DEBUG: no children found, process %d going to sleep\n", curr_proc->pid);
        // 没有找到子进程，睡眠等待
        sleep(curr_proc);
        printf("DEBUG: process %d woke up from sleep\n", curr_proc->pid);
    }
}

// 简单的睡眠/唤醒机制
void sleep(void *chan) {
    if (!curr_proc) return;
    
    curr_proc->chan = chan;
    curr_proc->state = SLEEPING;
    yield();//让出CPU
}

void wakeup(void *chan) {
    spin_lock(&proc_lock);
    
    // 唤醒所有在指定通道上睡眠的进程
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].state == SLEEPING && proc[i].chan == chan) {
            proc[i].state = RUNNABLE;
            proc[i].chan = 0;
        }
    }
    
    spin_unlock(&proc_lock);
}

// 主动让出CPU
void yield(void) {
    if (curr_proc && curr_proc->state == RUNNING) {
        curr_proc->state = RUNNABLE;
    }
    scheduler();
}

void scheduler(void) {
    static int scheduler_started_logged = 0;

    if (!scheduler_started_logged) {
        printf("Scheduler: starting...\n");
        scheduler_started_logged = 1;
    }
    
    // 开启中断
    asm volatile("csrs mstatus, %0" : : "r" (1 << 3));
    
    int found = 0;
    struct proc *p;
    
    spin_lock(&proc_lock);
    
    // 查找可运行进程
    for (p = proc; p < &proc[NPROC]; p++) {
        if (p->state == RUNNABLE) {
            found = 1;
            break;
        }
    }
    
    if (found) {
        printf("Scheduler: switching to process %d\n", p->pid);
        printf("  Process %d context: ra=%p, sp=%p\n", 
               p->pid, (void*)p->context.ra, (void*)p->context.sp);
        
        p->state = RUNNING;
        struct proc *prev_proc = curr_proc;
        curr_proc = p;
        
        spin_unlock(&proc_lock);
        
        // 上下文切换
        if (prev_proc) {
            printf("  Switching from process %d to %d\n", prev_proc->pid, p->pid);
            context_switch(&prev_proc->context, &p->context);
        } else {
            // 第一次调度或从退出进程切换
            printf("  Switching from scheduler to process %d\n", p->pid);
            context_switch(&scheduler_context, &p->context);
        }
        
        // 切换回来后
        printf("Scheduler: returned from process %d\n", curr_proc->pid);
        // curr_proc = 0;
    } else {
        // 没有可运行进程
        spin_unlock(&proc_lock);
        printf("Scheduler: no runnable processes found\n");
        
        // 检查是否有僵尸进程需要清理
        int zombie_count = 0;
        spin_lock(&proc_lock);
        for (int i = 0; i < NPROC; i++) {
            if (proc[i].state == ZOMBIE) {
                zombie_count++;
                printf("  Found zombie process %d\n", proc[i].pid);
            }
        }
        spin_unlock(&proc_lock);
        
        if (zombie_count > 0) {
            printf("Scheduler: %d zombie processes waiting to be reaped\n", zombie_count);
        }
        
        return;
    }
}

// 简单测试任务
void simple_task(void) {
    printf("🚀 Process %d: SIMPLE TASK STARTED\n", curr_proc->pid);
    
    // 做少量工作
    for (int i = 0; i < 3; i++) {
        printf("Process %d: working... step %d\n", curr_proc->pid, i + 1);
        
        // 很短延时
        for (volatile int j = 0; j < 5000; j++);
    }
    
    printf("✅ Process %d: TASK COMPLETED successfully\n", curr_proc->pid);
    exit_process(0);
}

// 计算密集型任务
void cpu_intensive_task(void) {
    printf("Process %d: CPU intensive task started\n", curr_proc->pid);
    
    uint64_t result = 0;
    for (uint64_t i = 0; i < 1000000; i++) {
        result += i * i;
        if (i % 100000 == 0) {
            printf("Process %d: progress %lu\n", curr_proc->pid, i);
        }
    }
    
    printf("Process %d: CPU task completed, result=%lu\n", curr_proc->pid, result);
    exit_process(0);
}

// 简单的共享缓冲区
#define BUFFER_SIZE 10
static int buffer[BUFFER_SIZE];
static int count = 0;
static int in = 0, out = 0;
static void *buffer_chan = (void*)0x1234;

// 共享缓冲区初始化函数
void shared_buffer_init(void) {
    count = in = out = 0;
    printf("Shared buffer initialized\n");
}

void producer_task(void) {
    printf("Process %d: producer started\n", curr_proc->pid);
    
    for (int i = 0; i < 5; i++) {
        // 等待缓冲区有空位
        while (count == BUFFER_SIZE) {
            sleep(buffer_chan);
        }
        
        // 生产项目
        buffer[in] = i;
        in = (in + 1) % BUFFER_SIZE;
        count++;
        
        printf("Process %d: produced item %d\n", curr_proc->pid, i);
        
        // 唤醒消费者
        wakeup(buffer_chan);
        
        // 延时
        for (volatile int j = 0; j < 500000; j++);
    }
    
    printf("Process %d: producer finished\n", curr_proc->pid);
    exit_process(0);
}

void consumer_task(void) {
    printf("Process %d: consumer started\n", curr_proc->pid);
    
    for (int i = 0; i < 5; i++) {
        // 等待缓冲区有数据
        while (count == 0) {
            sleep(buffer_chan);
        }
        
        // 消费项目
        int item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        count--;
        
        printf("Process %d: consumed item %d\n", curr_proc->pid, item);
        
        // 唤醒生产者
        wakeup(buffer_chan);
        
        // 延时
        for (volatile int j = 0; j < 500000; j++);
    }
    
    printf("Process %d: consumer finished\n", curr_proc->pid);
    exit_process(0);
}