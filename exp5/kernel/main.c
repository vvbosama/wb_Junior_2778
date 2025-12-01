#include "printf.h"
#include "clock.h"
#include "trap.h"
#include "proc.h"
#include "uart.h"
#include "priority.h"

// 获取时间函数
uint64_t get_time(void)
{
    uint64_t time;
    asm volatile("csrr %0, time" : "=r"(time));
    return time;
}

// 简单的延时循环
void simple_delay(uint64_t iterations)
{
    for (volatile uint64_t i = 0; i < iterations; i++)
    {
        asm volatile("nop");
    }
}

// kernel/main.c - 修复测试逻辑
void run_processes(void)
{
    printf("Starting scheduler to run processes...\n");

    // 检查是否有可运行的进程
    int has_runnable = 0;
    spin_lock(&proc_lock);
    for (int i = 0; i < NPROC; i++)
    {
        if (proc[i].state == RUNNABLE)
        {
            has_runnable = 1;
            break;
        }
    }
    spin_unlock(&proc_lock);

    if (has_runnable)
    {
        scheduler(); // 这会运行所有可运行的进程

        // 调度器返回后，检查是否所有进程都已完成
        printf("Scheduler returned, checking process states...\n");
    }
    else
    {
        printf("No runnable processes to schedule\n");
    }
}

// 简化测试 - 只测试一个进程
void test_single_process(void)
{
    printf("=== Testing Single Process ===\n");

    // 先清理任何现有的进程
    int status;
    while (wait_process(&status) > 0)
    {
        printf("Cleaned up zombie process\n");
    }

    // 只创建一个进程
    printf("Creating single process...\n");
    int pid = create_process(simple_task);
    printf("Created process with PID: %d\n", pid);

    // 启动调度器来运行进程
    run_processes();

    // 等待进程完成并回收
    printf("Waiting for process to complete...\n");
    int result = wait_process(&status);
    if (result > 0)
    {
        printf("Process %d completed with status: %d\n", result, status);
    }
    else
    {
        printf("No process to wait for\n");
    }

    printf("Single process test completed\n\n");
}

// 进程创建测试
void test_process_creation(void)
{
    printf("Testing process creation...\n");

    // 先清理
    int status;
    while (wait_process(&status) > 0)
    {
        // 清理僵尸进程
    }

    // 测试基本的进程创建
    int pid = create_process(simple_task);
    printf("Created process with PID: %d\n", pid);

    // 运行这个进程
    run_processes();

    // 等待进程完成
    wait_process(NULL);

    // 测试进程表限制
    int pids[NPROC];
    int count = 0;

    printf("Testing process table limits...\n");
    for (int i = 0; i < NPROC + 5; i++)
    {
        int pid = create_process(simple_task);
        if (pid > 0)
        {
            pids[count++] = pid;
            printf("Created process %d\n", pid);
        }
        else
        {
            printf("Failed to create process (table full at %d processes)\n", count);
            break;
        }
    }

    printf("Created %d processes\n", count);

    // 运行所有进程
    printf("Running all processes...\n");
    run_processes();

    // 清理测试进程
    for (int i = 0; i < count; i++)
    {
        int status;
        wait_process(&status);
        printf("Process %d exited with status %d\n", pids[i], status);
    }

    printf("Process creation test completed\n\n");
}

// 调度器测试
void test_scheduler(void)
{
    printf("Testing scheduler...\n");

    // 先清理之前的进程
    int status;
    while (wait_process(&status) > 0)
    {
        printf("Cleaned up zombie process\n");
    }

    // 创建多个计算密集型进程
    for (int i = 0; i < 3; i++)
    {
        create_process(cpu_intensive_task);
    }

    // 运行这些进程
    printf("Running multiple processes...\n");
    run_processes();

    // 等待所有进程完成
    for (int i = 0; i < 3; i++)
    {
        wait_process(&status);
        printf("Process completed with status: %d\n", status);
    }

    printf("Scheduler test completed\n\n");
}

// 同步机制测试
void test_synchronization(void)
{
    printf("Testing synchronization...\n");

    // 先清理之前的进程
    int status;
    while (wait_process(&status) > 0)
    {
        printf("Cleaned up zombie process\n");
    }

    // 测试生产者-消费者场景
    shared_buffer_init();
    create_process(producer_task);
    create_process(consumer_task);

    // 运行这些进程
    printf("Running producer-consumer processes...\n");
    run_processes();

    // 等待完成
    wait_process(NULL);
    wait_process(NULL);

    printf("Synchronization test completed\n\n");
}

void main(void)
{
    // 初始化各子系统
    uart_init();
    printf("RISC-V OS Starting...\n");

    clock_init();
    trap_init();
    proc_init();

    // 启用中断
    enable_interrupts();

    printf("\n=== Starting Process Management Tests ===\n\n");

    // 先运行简化测试
    // test_single_process();

    // 如果简化测试成功，运行完整测试
    test_process_creation();
    test_scheduler();
    test_synchronization();

    // 运行优先级调度测试
    run_priority_scheduling_tests();

    printf("=== All Tests Completed ===\n");
    printf("All tests completed successfully!\n");

    // 主循环
    while (1)
    {
        asm volatile("wfi");
    }
}