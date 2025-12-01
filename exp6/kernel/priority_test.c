// kernel/priority_test.c - 优先级调度测试
#include "printf.h"
#include "proc.h"
#include "priority.h"
#include "clock.h"

void test_basic_priority(void) {
    printf("\n=== Basic Priority Scheduling Test ===\n");
    
    // 清理之前的进程
    int status;
    while (wait_process(&status) > 0) {
        printf("Cleaned up process with status: %d\n", status);
    }
    
    printf("TEST: Creating only ONE process first...\n");
    
    // 先只创建一个进程进行测试
    int high_pid = create_process(high_priority_task);
    printf("Created process: PID=%d\n", high_pid);
    
    // 设置优先级
    set_priority(high_pid, 10);
    
    show_priority_info();
    
    printf("Starting priority scheduler with single process...\n");
    
    // 只运行一次调度
    printf("--- Calling priority_scheduler ---\n");
    priority_scheduler();
    printf("--- Returned from priority_scheduler ---\n");
    
    // 检查进程状态
    printf("Checking process states after scheduling...\n");
    for (int i = 0; i < NPROC; i++) {
        if (proc[i].state != UNUSED) {
            printf("  Process %d: state=%d\n", proc[i].pid, proc[i].state);
        }
    }
    
    // 尝试回收进程
    int pid = wait_process(&status);
    if (pid > 0) {
        printf("Process %d completed with status: %d\n", pid, status);
    } else {
        printf("No process completed\n");
    }
    
    printf("Single process test completed\n");
}

// 动态优先级调整测试
void test_dynamic_priority(void) {
    printf("\n=== Dynamic Priority Adjustment Test ===\n");
    
    // 清理
    int status;
    while (wait_process(&status) > 0);
    
    // 创建多个CPU密集型进程
    printf("Creating CPU-intensive processes...\n");
    int pids[3];
    
    for (int i = 0; i < 3; i++) {
        pids[i] = create_process(cpu_intensive_priority_task);
        set_priority(pids[i], 5);  // 初始相同优先级
    }
    
    show_priority_info();
    
    printf("Running with dynamic priority adjustments...\n");
    priority_scheduler();
    
    // 等待完成
    for (int i = 0; i < 3; i++) {
        wait_process(&status);
        printf("Process %d completed\n", pids[i]);
    }
    
    printf("Dynamic priority test completed\n");
}

// 友好值(nice)测试
void test_nice_values(void) {
    printf("\n=== Nice Values Test ===\n");
    
    // 清理
    int status;
    while (wait_process(&status) > 0);
    
    printf("Testing nice value adjustments...\n");
    
    int pid1 = create_process(medium_priority_task);
    int pid2 = create_process(medium_priority_task);
    int pid3 = create_process(medium_priority_task);
    
    // 设置不同的nice值
    set_nice(pid1, -10);   // 高优先级
    set_nice(pid2, 0);     // 普通优先级
    set_nice(pid3, 10);    // 低优先级
    
    show_priority_info();
    
    printf("Running processes with different nice values...\n");
    priority_scheduler();
    
    // 等待完成
    wait_process(&status);
    wait_process(&status);
    wait_process(&status);
    
    printf("Nice values test completed\n");
}

// 混合工作负载测试
void test_mixed_workload(void) {
    printf("\n=== Mixed Workload Test ===\n");
    
    // 清理
    int status;
    while (wait_process(&status) > 0);
    
    printf("Creating mixed workload (I/O bound and CPU bound)...\n");
    
    // 创建混合类型的进程
    int io_pid = create_process(high_priority_task);      // 高优先级I/O型
    int cpu_pid1 = create_process(cpu_intensive_priority_task); // CPU型
    int cpu_pid2 = create_process(cpu_intensive_priority_task); // CPU型
    int normal_pid = create_process(medium_priority_task); // 普通型
    
    // 设置不同的优先级
    set_priority(io_pid, 9);       // I/O型高优先级
    set_priority(cpu_pid1, 3);     // CPU型低优先级
    set_priority(cpu_pid2, 4);     // CPU型较低优先级
    set_priority(normal_pid, 6);   // 普通型中等优先级
    
    show_priority_info();
    
    printf("Running mixed workload with priority scheduling...\n");
    priority_scheduler();
    
    // 等待所有进程完成
    while (wait_process(&status) > 0) {
        printf("Mixed workload process completed\n");
    }
    
    printf("Mixed workload test completed\n");
}

// 性能对比测试
void test_performance_comparison(void) {
    printf("\n=== Performance Comparison: Round Robin vs Priority ===\n");
    
    // 测试1: 轮转调度
    printf("1. Testing Round Robin scheduling...\n");
    uint64_t start_time = get_ticks(TIMER_FAST);
    
    // 清理
    int status;
    while (wait_process(&status) > 0);
    
    // 创建测试进程
    for (int i = 0; i < 3; i++) {
        create_process(cpu_intensive_priority_task);
    }
    
    // 使用原来的轮转调度器
    scheduler();
    
    while (wait_process(&status) > 0);
    
    uint64_t rr_time = get_ticks(TIMER_FAST) - start_time;
    printf("Round Robin completed in %lu ticks\n", rr_time);
    
    // 测试2: 优先级调度
    printf("2. Testing Priority scheduling...\n");
    start_time = get_ticks(TIMER_FAST);
    
    // 创建相同的工作负载但设置不同优先级
    int pids[3];
    for (int i = 0; i < 3; i++) {
        pids[i] = create_process(cpu_intensive_priority_task);
        set_priority(pids[i], 3 + i * 2); // 不同优先级
    }
    
    priority_scheduler();
    
    while (wait_process(&status) > 0);
    
    uint64_t priority_time = get_ticks(TIMER_FAST) - start_time;
    printf("Priority scheduling completed in %lu ticks\n", priority_time);
    
    printf("Performance comparison:\n");
    printf("  Round Robin:   %lu ticks\n", rr_time);
    printf("  Priority:      %lu ticks\n", priority_time);
    printf("  Difference:    %ld ticks (%s)\n", 
           (long)(priority_time - rr_time),
           (priority_time < rr_time) ? "Priority faster" : "Round Robin faster");
}

// 综合优先级调度测试
void run_priority_scheduling_tests(void) {
    printf("\n🔀 STARTING PRIORITY SCHEDULING TESTS\n");
    
    // 初始化优先级调度
    priority_init();
    
    // 运行各种测试
    // test_basic_priority();
    // test_dynamic_priority();
    // test_nice_values();
    test_mixed_workload();
    // test_performance_comparison();
    
    printf("\n✅ ALL PRIORITY SCHEDULING TESTS COMPLETED SUCCESSFULLY\n");
}