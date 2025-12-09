// kernel/priority_test.c - 优先级调度测试
#include "printf.h"
#include "proc.h"
#include "priority.h"

#define MAX_TEST_PROCS 8

struct completion_log {
    int order[MAX_TEST_PROCS];
    int count;
};

static volatile int aging_low_run_count = 0;

// 用于验证 aging 的全局变量
static volatile int aging_test_low1_initial_priority = -1;
static volatile int aging_test_low1_final_priority = -1;
static volatile int aging_test_low2_initial_priority = -1;
static volatile int aging_test_low2_final_priority = -1;
static volatile int aging_test_hog_should_stop = 0;

static void busy_spin(int loops) {
    for (int i = 0; i < loops; i++) {
        for (volatile int j = 0; j < 5000; j++);
    }
}

static void cleanup_all_processes(void) {
    int status;
    int pid;
    while ((pid = wait_process(&status)) > 0) {
        printf("  Cleaned zombie pid=%d status=%d\n", pid, status);
    }
}

static int has_runnable_process(void) {
    int active = 0;
    spin_lock(&proc_lock);
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].state == RUNNABLE) {
            active = 1;
            break;
        }
    }
    spin_unlock(&proc_lock);
    return active;
}

static void drive_scheduler_until_idle(void) {
    int guard = 0;
    while (has_runnable_process()) {
        scheduler();
        if (++guard > 1024) {
            printf("  WARNING: scheduler guard triggered, breaking out\n");
            break;
        }
    }
}

static void collect_completion(struct completion_log *log, int expected) {
    log->count = 0;
    int status;
    for (int i = 0; i < expected; i++) {
        int pid = wait_process(&status);
        if (pid <= 0) {
            break;
        }
        log->order[log->count++] = pid;
    }
}

static void print_completion(const char *tag, const struct completion_log *log) {
    printf("  %s completion order:", tag);
    for (int i = 0; i < log->count; i++) {
        printf(" %d", log->order[i]);
    }
    printf("\n");
}

static int snapshot_ticks(int pid) {
    int ticks = -1;
    spin_lock(&proc_lock);
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].pid == pid) {
            ticks = proc[i].ticks;
            break;
        }
    }
    spin_unlock(&proc_lock);
    return ticks;
}

// ----------- 测试专用任务 -----------

static void high_priority_worker(void) {
    for (int i = 0; i < 40; i++) {
        busy_spin(150);
        if (i % 4 == 0) {
            yield();
        }
    }
    exit_process(0);
}

static void low_priority_worker(void) {
    for (int i = 0; i < 8; i++) {
        busy_spin(40);
        yield();
    }
    exit_process(0);
}

static void rr_worker(void) {
    for (int i = 0; i < 12; i++) {
        busy_spin(60);
        yield();
    }
    exit_process(0);
}

// 持续运行的高优先级任务，确保低优先级任务等待足够长时间
// 关键：每次 yield 后重新设置最高优先级，防止降级
static void aging_test_hog_worker(void) {
    int iterations = 0;
    while (!aging_test_hog_should_stop && iterations < 50) {
        busy_spin(200);
        yield();
        // 重新设置最高优先级，防止被 MLFQ 降级
        // 这样低优先级任务就会一直等待，直到 wait_time >= 10 触发 aging
        if (curr_proc) {
            set_priority(curr_proc->pid, PRIORITY_MAX);
        }
        iterations++;
    }
    exit_process(0);
}

// 低优先级任务，检查自己的 priority 是否被提升了
static void aging_test_low_worker(void) {
    int pid = curr_proc->pid;
    int initial_prio = curr_proc->priority;
    
    printf("  Aging test low worker pid=%d: initial priority=%d\n", pid, initial_prio);
    
    // 保存初始优先级
    if (pid == aging_test_low1_initial_priority || aging_test_low1_initial_priority == -1) {
        aging_test_low1_initial_priority = initial_prio;
        aging_test_low1_final_priority = curr_proc->priority;
    } else {
        aging_test_low2_initial_priority = initial_prio;
        aging_test_low2_final_priority = curr_proc->priority;
    }
    
    aging_low_run_count++;
    exit_process(0);
}

// ----------- 具体测试用例 -----------

static void test_priority_gap(void) {
    printf("\n[T1] 高低优先级对比（高优先级应先完成）\n");
    cleanup_all_processes();
    
    int fast = create_process(high_priority_worker);
    int slow = create_process(low_priority_worker);
    set_priority(fast, PRIORITY_MAX);
    set_priority(slow, PRIORITY_MIN + 1);
    
    drive_scheduler_until_idle();
    
    struct completion_log log;
    collect_completion(&log, 2);
    print_completion("T1", &log);
    
    if (log.count >= 1 && log.order[0] == fast) {
        printf("  [PASS] 高优先级进程 %d 最先完成\n", fast);
    } else {
        printf("  [FAIL] 期望 %d 先完成，但实际顺序不同\n", fast);
    }
    
    cleanup_all_processes();
}

static void test_equal_priority_rr(void) {
    printf("\n[T2] 相同优先级任务公平性（应近似 RR）\n");
    cleanup_all_processes();
    
    int pids[3];
    for (int i = 0; i < 3; i++) {
        pids[i] = create_process(rr_worker);
        set_priority(pids[i], PRIORITY_DEFAULT);
    }
    
    drive_scheduler_until_idle();
    
    int ticks[3];
    for (int i = 0; i < 3; i++) {
        ticks[i] = snapshot_ticks(pids[i]);
    }
    
    struct completion_log log;
    collect_completion(&log, 3);
    print_completion("T2", &log);
    
    int min = ticks[0], max = ticks[0];
    for (int i = 1; i < 3; i++) {
        if (ticks[i] < min) min = ticks[i];
        if (ticks[i] > max) max = ticks[i];
    }
    
    printf("  调度次数 ticks: %d %d %d\n", ticks[0], ticks[1], ticks[2]);
    if (max - min <= 1) {
        printf("  [PASS] 调度次数接近，表现与 RR 类似\n");
    } else {
        printf("  [WARN] 调度次数差异较大，可进一步检查\n");
    }
    
    cleanup_all_processes();
}

// 辅助函数：获取进程的 priority 和 queue_level
static int get_proc_priority(int pid) {
    int result = -1;
    spin_lock(&proc_lock);
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].pid == pid) {
            result = proc[i].priority;
            break;
        }
    }
    spin_unlock(&proc_lock);
    return result;
}

static int get_proc_queue_level(int pid) {
    int result = -1;
    spin_lock(&proc_lock);
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].pid == pid) {
            result = proc[i].queue_level;
            break;
        }
    }
    spin_unlock(&proc_lock);
    return result;
}

static void test_aging_prevents_starvation(void) {
    printf("\n[T3] Aging 防止饥饿（低优先级任务等待超过阈值后升级）\n");
    cleanup_all_processes();
    aging_low_run_count = 0;
    aging_test_hog_should_stop = 0;
    aging_test_low1_initial_priority = -1;
    aging_test_low1_final_priority = -1;
    aging_test_low2_initial_priority = -1;
    aging_test_low2_final_priority = -1;
    
    // 创建高优先级任务，持续运行
    int hog = create_process(aging_test_hog_worker);
    set_priority(hog, PRIORITY_MAX);
    
    // 创建低优先级任务
    int low1 = create_process(aging_test_low_worker);
    int low2 = create_process(aging_test_low_worker);
    set_priority(low1, PRIORITY_MIN);
    set_priority(low2, PRIORITY_MIN);
    
    // 记录初始状态
    int low1_initial_prio = get_proc_priority(low1);
    int low1_initial_level = get_proc_queue_level(low1);
    int low2_initial_prio = get_proc_priority(low2);
    int low2_initial_level = get_proc_queue_level(low2);
    
    printf("  初始状态: low1(pid=%d) priority=%d level=%d, low2(pid=%d) priority=%d level=%d\n",
           low1, low1_initial_prio, low1_initial_level,
           low2, low2_initial_prio, low2_initial_level);
    
    // 让高优先级任务运行足够长时间，确保低优先级任务 wait_time >= 10
    // 每次调度都会在 age_runnable_processes() 中增加 RUNNABLE 进程的 wait_time
    // 需要运行至少 12 次调度（留一些余量），确保低优先级任务 wait_time >= 10
    printf("  运行调度器，让低优先级任务累积 wait_time...\n");
    for (int i = 0; i < 12; i++) {
        scheduler();
        // 检查低优先级任务的 wait_time（用于调试）
        spin_lock(&proc_lock);
        int low1_wait = -1, low2_wait = -1;
        for (int j = 0; j < NPROC; j++) {
            if (proc[j].pid == low1) {
                low1_wait = proc[j].wait_time;
            }
            if (proc[j].pid == low2) {
                low2_wait = proc[j].wait_time;
            }
        }
        spin_unlock(&proc_lock);
        if (i % 3 == 0) {
            printf("    调度 %d: low1 wait_time=%d, low2 wait_time=%d\n", i, low1_wait, low2_wait);
        }
    }
    
    // 检查低优先级任务的 priority 和 queue_level 是否被提升了
    int low1_after_prio = get_proc_priority(low1);
    int low1_after_level = get_proc_queue_level(low1);
    int low2_after_prio = get_proc_priority(low2);
    int low2_after_level = get_proc_queue_level(low2);
    
    printf("  Aging后状态: low1 priority=%d level=%d, low2 priority=%d level=%d\n",
           low1_after_prio, low1_after_level,
           low2_after_prio, low2_after_level);
    
    // 停止高优先级任务，让低优先级任务被调度并完成
    aging_test_hog_should_stop = 1;
    
    // 继续运行直到所有任务完成
    drive_scheduler_until_idle();
    
    struct completion_log log;
    collect_completion(&log, 3);
    print_completion("T3", &log);
    
    // 验证 aging 是否生效
    int aging_verified = 0;
    if (low1_after_prio > low1_initial_prio || low1_after_level < low1_initial_level) {
        printf("  ✓ low1 的 priority 从 %d 提升到 %d，level 从 %d 降到 %d\n",
               low1_initial_prio, low1_after_prio, low1_initial_level, low1_after_level);
        aging_verified++;
    }
    if (low2_after_prio > low2_initial_prio || low2_after_level < low2_initial_level) {
        printf("  ✓ low2 的 priority 从 %d 提升到 %d，level 从 %d 降到 %d\n",
               low2_initial_prio, low2_after_prio, low2_initial_level, low2_after_level);
        aging_verified++;
    }
    
    if (aging_verified >= 1 && aging_low_run_count == 2) {
        printf("  [PASS] Aging 防止饥饿机制生效：低优先级任务等待后成功升级\n");
    } else {
        printf("  [FAIL] Aging 未生效：aging_verified=%d, low_run_count=%d\n",
               aging_verified, aging_low_run_count);
    }
    
    cleanup_all_processes();
}

// 综合优先级调度测试
void run_priority_scheduling_tests(void) {
    printf("\n=== PRIORITY SCHEDULER TESTS START ===\n");
    priority_init();
    
    test_priority_gap();
    test_equal_priority_rr();
    test_aging_prevents_starvation();
    
    printf("=== PRIORITY SCHEDULER TESTS END ===\n");
}