// kernel/priority.c - 优先级调度扩展
#include "proc.h"
#include "printf.h"
#include "clock.h"
#include "priority.h"

static struct context scheduler_context; // 调度器自身的上下文
// 添加缺少的函数声明
extern void context_switch(struct context *old, struct context *new);
extern uint64_t get_ticks(timer_type_t timer);

// 优先级定义
#define PRIORITY_MIN 1
#define PRIORITY_MAX 10
#define PRIORITY_DEFAULT 5

// 进程控制块扩展 - 添加优先级字段
struct proc_priority {
    struct proc *proc;
    int priority;
    int dynamic_priority;  // 动态优先级（用于反馈调度）
    uint64_t run_time;     // 累计运行时间
    uint64_t wait_time;    // 累计等待时间
    int nice;              // 友好值（-20 到 19）
};

static struct proc_priority priority_procs[NPROC];
static int priority_initialized = 0;

// 获取进程优先级
int get_priority(int pid) {
    spin_lock(&proc_lock);
    
    for (int i = 0; i < NPROC; i++) {
        if (priority_procs[i].proc->pid == pid) {
            int priority = priority_procs[i].priority;
            spin_unlock(&proc_lock);
            return priority;
        }
    }
    
    spin_unlock(&proc_lock);
    return -1;
}

// 获取进程动态优先级
int get_dynamic_priority(int pid) {
    spin_lock(&proc_lock);
    
    for (int i = 0; i < NPROC; i++) {
        if (priority_procs[i].proc->pid == pid) {
            int dpriority = priority_procs[i].dynamic_priority;
            spin_unlock(&proc_lock);
            return dpriority;
        }
    }
    
    spin_unlock(&proc_lock);
    return -1;
}

// 优先级调度初始化
void priority_init(void) {
    printf("Priority: initializing priority scheduler\n");
    
    for (int i = 0; i < NPROC; i++) {
        priority_procs[i].proc = &proc[i];
        priority_procs[i].priority = PRIORITY_DEFAULT;
        priority_procs[i].dynamic_priority = PRIORITY_DEFAULT;
        priority_procs[i].run_time = 0;
        priority_procs[i].wait_time = 0;
        priority_procs[i].nice = 0;
    }
    
    priority_initialized = 1;
    printf("Priority: scheduler initialized with %d priority levels\n", 
           PRIORITY_MAX - PRIORITY_MIN + 1);
}

// 设置进程优先级
int set_priority(int pid, int priority) {
    if (!priority_initialized) {
        priority_init();
    }
    
    if (priority < PRIORITY_MIN || priority > PRIORITY_MAX) {
        printf("Priority: invalid priority %d (must be %d-%d)\n", 
               priority, PRIORITY_MIN, PRIORITY_MAX);
        return -1;
    }
    
    spin_lock(&proc_lock);
    
    for (int i = 0; i < NPROC; i++) {
        if (priority_procs[i].proc->pid == pid) {
            priority_procs[i].priority = priority;
            priority_procs[i].dynamic_priority = priority;
            printf("Priority: set process %d priority to %d\n", pid, priority);
            spin_unlock(&proc_lock);
            return 0;
        }
    }
    
    spin_unlock(&proc_lock);
    printf("Priority: process %d not found\n", pid);
    return -1;
}

// 设置进程友好值
int set_nice(int pid, int nice) {
    if (nice < -20 || nice > 19) {
        printf("Priority: invalid nice value %d (must be -20 to 19)\n", nice);
        return -1;
    }
    
    spin_lock(&proc_lock);
    
    for (int i = 0; i < NPROC; i++) {
        if (priority_procs[i].proc->pid == pid) {
            priority_procs[i].nice = nice;
            // 根据nice值调整优先级
            int new_priority = PRIORITY_DEFAULT + (nice / 4); // 简单映射
            if (new_priority < PRIORITY_MIN) new_priority = PRIORITY_MIN;
            if (new_priority > PRIORITY_MAX) new_priority = PRIORITY_MAX;
            
            priority_procs[i].priority = new_priority;
            printf("Priority: set process %d nice to %d, priority to %d\n", 
                   pid, nice, new_priority);
            spin_unlock(&proc_lock);
            return 0;
        }
    }
    
    spin_unlock(&proc_lock);
    printf("Priority: process %d not found\n", pid);
    return -1;
}

static struct proc* get_highest_priority_proc(void) {
    struct proc *selected = NULL;
    int highest_priority = -1;
    
    printf("get_highest_priority_proc: starting search through %d process slots\n", NPROC);
    
    for (int i = 0; i < NPROC; i++) {
        
        // 直接检查进程状态，不依赖 priority_procs
        if (proc[i].state == RUNNABLE) {
            
            // 获取优先级
            int priority = -1;
            for (int j = 0; j < NPROC; j++) {
                if (priority_procs[j].proc == &proc[i]) {
                    priority = priority_procs[j].dynamic_priority;
                    break;
                }
            }
            
            
            if (priority > highest_priority) {
                highest_priority = priority;
                selected = &proc[i];
            }
        } else if (proc[i].state != UNUSED) {
        } else {
        }
    }
    
    if (selected) {
        printf("get_highest_priority_proc: FINAL SELECTION - process %d with priority %d\n", 
               selected->pid, highest_priority);
    } else {
        printf("get_highest_priority_proc: NO RUNNABLE PROCESSES FOUND\n");
    }
    
    return selected;
}

// 更新动态优先级（简单的反馈机制）
// static void update_dynamic_priority(struct proc *p, uint64_t runtime) {
//     for (int i = 0; i < NPROC; i++) {
//         if (priority_procs[i].proc == p) {
//             // 增加运行时间
//             priority_procs[i].run_time += runtime;
            
//             // 简单的老化机制：长时间等待的进程提高优先级
//             if (priority_procs[i].wait_time > 1000000) { // 阈值
//                 if (priority_procs[i].dynamic_priority < PRIORITY_MAX) {
//                     priority_procs[i].dynamic_priority++;
//                 }
//                 priority_procs[i].wait_time = 0;
//             }
            
//             // 减少运行时间长的进程的优先级（防止饥饿）
//             if (priority_procs[i].run_time > 5000000) { // 阈值
//                 if (priority_procs[i].dynamic_priority > PRIORITY_MIN) {
//                     priority_procs[i].dynamic_priority--;
//                 }
//                 priority_procs[i].run_time = 0;
//             }
//             break;
//         }
//     }
// }

// 增加所有可运行进程的等待时间
// static void increment_wait_time(void) {
//     for (int i = 0; i < NPROC; i++) {
//         if (priority_procs[i].proc->state == RUNNABLE) {
//             priority_procs[i].wait_time++;
//         }
//     }
// }

void priority_scheduler(void) {
    // static uint64_t last_schedule_time = 0;
    static int scheduler_started = 0;
    static int iteration_count = 0;
    
    if (!priority_initialized) {
        priority_init();
    }
    
    if (!scheduler_started) {
        printf("Priority: starting priority-based scheduler\n");
        scheduler_started = 1;
    }
    
    printf("Priority: entering main scheduler loop\n");
    
    for (;;) {
        iteration_count++;
        printf("Priority: scheduler iteration %d\n", iteration_count);
        
        // 安全限制
        if (iteration_count > 10) {
            printf("EMERGENCY: Scheduler iteration limit reached, returning\n");
            return;
        }
        
        asm volatile("csrs mstatus, %0" : : "r" (1 << 3));
        
        printf("Priority: looking for runnable processes...\n");
        
        spin_lock(&proc_lock);
        struct proc *p = get_highest_priority_proc();
        
        if (p) {
            printf("Priority: selected process %d for execution\n", p->pid);
            
            p->state = RUNNING;
            struct proc *prev_proc = curr_proc;
            curr_proc = p;
            
            spin_unlock(&proc_lock);
            
            printf("Priority: BEFORE context_switch\n");
            
            if (prev_proc) {
                context_switch(&prev_proc->context, &p->context);
            } else {
                context_switch(&scheduler_context, &p->context);
            }
            
            // 当上下文切换返回时，进程已经执行完毕或被抢占
            printf("Priority: AFTER context_switch - returned from process\n");
            
            // 重新获取锁来更新状态
            spin_lock(&proc_lock);
            
            // 如果进程还在RUNNING状态，说明它被抢占了，设置回RUNNABLE
            if (p->state == RUNNING) {
                printf("Priority: process %d was preempted, setting to RUNNABLE\n", p->pid);
                p->state = RUNNABLE;
            }
            // 如果进程已经设置为ZOMBIE（通过任务函数），我们保持那个状态
            
            curr_proc = 0;
            spin_unlock(&proc_lock);
            
        } else {
            printf("Priority: no runnable processes found\n");
            spin_unlock(&proc_lock);
            printf("Priority: returning from scheduler\n");
            return;
        }
        
        // 添加检查：如果所有进程都完成了，提前退出
        int all_completed = 1;
        spin_lock(&proc_lock);
        for (int i = 0; i < NPROC; i++) {
            if (proc[i].state == RUNNABLE || proc[i].state == RUNNING) {
                all_completed = 0;
                break;
            }
        }
        spin_unlock(&proc_lock);
        
        if (all_completed) {
            printf("Priority: all processes completed, returning from scheduler\n");
            return;
        }
    }
}

// 显示所有进程的优先级信息
void show_priority_info(void) {
    printf("\n=== Priority Scheduling Information ===\n");
    
    spin_lock(&proc_lock);
    
    for (int i = 0; i < NPROC; i++) {
        if (priority_procs[i].proc->state != UNUSED) {
            printf("Process %d: state=%d, priority=%d, dynamic=%d, nice=%d\n",
                   priority_procs[i].proc->pid,
                   priority_procs[i].proc->state,
                   priority_procs[i].priority,
                   priority_procs[i].dynamic_priority,
                   priority_procs[i].nice);
        }
    }
    
    spin_unlock(&proc_lock);
    printf("=======================================\n\n");
}

// 在 proc.c 中修改 exit_process
void exit_process2(int status) {
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
    
    printf("Process %d: switching back to scheduler\n", curr_proc->pid);
    
    struct proc *p = curr_proc;
    curr_proc = 0;
    
    // 切换到调度器上下文
    context_switch(&p->context, &scheduler_context);
    
    // 这行代码不应该执行
    printf("ERROR: exit_process continued after context switch!\n");
    for (;;) { asm volatile("wfi"); }
}

void high_priority_task(void) {
    if (!curr_proc) {
        printf("ERROR: high_priority_task - no current process!\n");
        return;
    }
    
    int pid = curr_proc->pid;
    printf("🎯 HIGH PRIORITY Process %d: TASK STARTED\n", pid);
    
    for (int i = 0; i < 3; i++) {
        printf("HIGH PRIORITY %d: working step %d/3\n", pid, i + 1);
        for (volatile int j = 0; j < 10000; j++);
    }
    
    printf("✅ HIGH PRIORITY Process %d: TASK COMPLETED - calling exit\n", pid);
    
    // 使用修复后的 exit_process
    exit_process2(0);
    
    // 不应该到达这里
    printf("ERROR: Continued after exit!\n");
    for (;;) { asm volatile("wfi"); }
}

// 优先级测试任务 - 中优先级
void medium_priority_task(void) {
    int pid = curr_proc->pid;
    printf("➡️ MEDIUM PRIORITY Process %d: STARTED\n", pid);
    
    for (int i = 0; i < 8; i++) {
        printf("MEDIUM PRIORITY %d: working at normal speed... step %d\n", pid, i + 1);
        for (volatile int j = 0; j < 200000; j++); // 中等延时
    }
    
    printf("✅ MEDIUM PRIORITY Process %d: COMPLETED\n", pid);
    exit_process2(0);
}

// 优先级测试任务 - 低优先级
void low_priority_task(void) {
    int pid = curr_proc->pid;
    printf("🐢 LOW PRIORITY Process %d: STARTED\n", pid);
    
    for (int i = 0; i < 10; i++) {
        printf("LOW PRIORITY %d: working slowly... step %d\n", pid, i + 1);
        for (volatile int j = 0; j < 300000; j++); // 长延时
    }
    
    printf("✅ LOW PRIORITY Process %d: COMPLETED\n", pid);
    exit_process2(0);
}

// CPU密集型任务（用于测试优先级影响）
void cpu_intensive_priority_task(void) {
    int pid = curr_proc->pid;
    int priority = get_priority(pid);
    printf("Process %d (priority %d): CPU intensive task started\n", pid, priority);
    
    uint64_t computations = 0;
    for (uint64_t i = 0; i < 500000; i++) {
        computations += i * i;
        if (i % 50000 == 0) {
            printf("Process %d (priority %d): computation progress %lu\n", 
                   pid, priority, i);
        }
    }
    
    printf("Process %d (priority %d): completed, result=%lu\n", 
           pid, priority, computations);
    exit_process2(0);
}