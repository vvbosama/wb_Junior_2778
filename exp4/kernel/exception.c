// kernel/exception.c - 修复版本
#include "exception.h"
#include "printf.h"
#include "trap.h"
#include "clock.h"

// 异常处理函数
// ctx：指向陷阱上下文的指针，包含所有寄存器状态
// cause：异常原因寄存器值
void handle_exception(struct trap_context *ctx, uint64_t cause) {
    int exc_code = cause & 0xF;//提取异常代码（低4位）

    //mepc 是机器异常程序计数器，指向触发异常的指令
    printf("HANDLE_EXCEPTION: code=%d at mepc=%p\n", exc_code, (void*)ctx->mepc);
    
    switch (exc_code) {
        case CAUSE_ILLEGAL_INSTRUCTION://非法指令
            printf("ILLEGAL INSTRUCTION - skipping\n");
            // 检查指令是否有效，如果无效则跳过
            // 如果是零指令，跳过该指令继续执行
            if (*(uint32_t*)ctx->mepc == 0x00000000) {
                printf("Found zero instruction, skipping\n");
                ctx->mepc += 4;
            } else { //如果是其他非法指令，系统挂起
                printf("Unknown illegal instruction, halting\n");
                while(1) asm volatile("wfi");//"wfi"：RISC-V 汇编指令，意思是 Wait For Interrupt（等待中断），可以通过中断唤醒
            }
            break;
            
        case CAUSE_BREAKPOINT://断点异常
            printf("BREAKPOINT - skipping ebreak instruction\n");
            ctx->mepc += 4;  // 跳过ebreak
            break;
            
        case CAUSE_MACHINE_ECALL://环境调用异常：由 ecall 指令触发
            printf("ECALL - skipping ecall instruction\n");
            ctx->mepc += 4;  // 跳过ecall
            break;

          // 新增内存访问异常处理
        case CAUSE_LOAD_ACCESS:
            printf("LOAD ACCESS FAULT at address %p - skipping instruction\n", (void*)ctx->mtval);
            ctx->mepc += 4;  // 跳过导致异常的加载指令
            break;
            
        default:
            printf("UNHANDLED EXCEPTION %d - HALTING\n", exc_code);
            while(1) asm volatile("wfi");
            break;
    }
    
    printf("Exception handling complete, returning to mepc=%p\n", (void*)ctx->mepc);
}

// 测试异常处理 - 使用内联汇编确保正确性
void test_exception_handling(void) {
    printf("=== Testing Exception Handling ===\n");
    
    // 测试1: 断点异常 (ebreak)
    printf("1. Testing breakpoint exception...\n");
    printf("Before ebreak\n");
    
    // 使用内联汇编确保正确执行流程
    asm volatile(
        "nop\n"                    // 确保对齐
        "ebreak\n"                 // 触发断点
        "j 1f\n"                   // 跳过填充
        ".align 2\n"               // 确保对齐
        "1:\n"
        "nop\n"                    // 继续执行
        :
        :
    );
    
    printf("After ebreak - Breakpoint handled!\n");
    
    // 测试2: 环境调用异常 (ecall)
    printf("2. Testing ECALL exception...\n");
    printf("Before ecall\n");
    
    asm volatile(
        "nop\n"                    // 确保对齐
        "ecall\n"                  // 触发环境调用
        "j 1f\n"                   // 跳过填充
        ".align 2\n"               // 确保对齐
        "1:\n"
        "nop\n"                    // 继续执行
        :
        :
    );
    
    printf("After ecall - ECALL handled!\n");

    // 测试3: 非法指令异常
    printf("3. Testing illegal instruction exception...\n");
    printf("Before illegal instruction\n");
    
    // 使用一个已知的非法指令编码
    asm volatile(
        "nop\n"
        ".word 0x00000000\n"  // 全零指令，通常是非法的
        "j 1f\n"
        ".align 2\n"
        "1:\n"
        "nop\n"
        :
        :
    );
    
    printf("After illegal instruction - Exception handled!\n");

    printf("4、Testing Memory Access Exceptions\n");

    //访问未映射的高地址
    printf("Testing access to unmapped high memory...\n");
    printf("Before accessing unmapped memory\n");
    
    // 使用内联汇编确保异常处理流程正确
    asm volatile(
        "nop\n"
        "li t0, 0xFFFFFFFF00000000\n"  // 加载未映射地址
        "ld t1, 0(t0)\n"               // 尝试加载，应该触发异常
        "j 1f\n"                       // 如果异常处理正确，会跳到这里
        ".align 2\n"
        "1:\n"
        "nop\n"
        :
        :
        : "t0", "t1"
    );
    
    printf("After load access fault - Exception handled!\n");
    
}

// 中断性能测试函数
void test_interrupt_overhead(void) {
    printf("\n=== Interrupt Overhead Measurement ===\n");
    
    // 保存初始时间
    uint64_t start_time, end_time;
    uint64_t total_cycles = 0;
    int test_iterations = 100;
    
    printf("Measuring interrupt handling overhead (%d iterations)...\n", test_iterations);
    
    // 测量多次中断处理的平均时间
    for (int i = 0; i < test_iterations; i++) {
        // 读取当前时间作为开始
        asm volatile("csrr %0, time" : "=r"(start_time));
        
        // 模拟一个简单的中断处理流程
        // 这里我们直接调用时钟中断处理来测量
        clock_set_next_event();
        
        // 读取结束时间
        asm volatile("csrr %0, time" : "=r"(end_time));
        
        total_cycles += (end_time - start_time);
        
        // 小延迟以避免测量偏差
        for (int j = 0; j < 100; j++) {
            asm volatile("nop");
        }
    }
    
    uint64_t avg_cycles = total_cycles / test_iterations;
    printf("Average interrupt handling overhead: %lu cycles\n", avg_cycles);
    
    // 测量上下文切换成本
    printf("\nMeasuring context switch cost...\n");
    
    total_cycles = 0;
    for (int i = 0; i < test_iterations; i++) {
        asm volatile("csrr %0, time" : "=r"(start_time));
        
        // 模拟上下文保存（简化版）
        asm volatile(
            "addi sp, sp, -16\n"
            "sd ra, 0(sp)\n"
            "sd a0, 8(sp)\n"
            :
            :
        );
        
        // 模拟上下文恢复
        asm volatile(
            "ld a0, 8(sp)\n"
            "ld ra, 0(sp)\n"
            "addi sp, sp, 16\n"
            :
            :
        );
        
        asm volatile("csrr %0, time" : "=r"(end_time));
        
        total_cycles += (end_time - start_time);
    }
    
    avg_cycles = total_cycles / test_iterations;
    printf("Average context switch cost: %lu cycles\n", avg_cycles);
    
    // // 分析中断频率影响
    // printf("\nAnalyzing interrupt frequency impact...\n");
    
    // uint64_t fast_ticks_before = get_ticks(TIMER_FAST);
    // uint64_t medium_ticks_before = get_ticks(TIMER_MEDIUM);
    // uint64_t slow_ticks_before = get_ticks(TIMER_SLOW);
    
    // // 让系统运行一段时间来观察中断频率
    // printf("Running system for measurement period...\n");
    
    // // 简单延迟循环
    // for (volatile int i = 0; i < 1000000; i++) {
    //     asm volatile("nop");
    // }
    
    // uint64_t fast_ticks_after = get_ticks(TIMER_FAST);
    // uint64_t medium_ticks_after = get_ticks(TIMER_MEDIUM);
    // uint64_t slow_ticks_after = get_ticks(TIMER_SLOW);
    
    // printf("Interrupt counts during measurement:\n");
    // printf("  FAST:   %lu interrupts\n", fast_ticks_after - fast_ticks_before);
    // printf("  MEDIUM: %lu interrupts\n", medium_ticks_after - medium_ticks_before);
    // printf("  SLOW:   %lu interrupts\n", slow_ticks_after - slow_ticks_before);
    
    // // 计算中断处理占用率估算
    // uint64_t total_interrupts = (fast_ticks_after - fast_ticks_before) +
    //                            (medium_ticks_after - medium_ticks_before) +
    //                            (slow_ticks_after - slow_ticks_before);
    
    // // 假设平均中断处理时间为测量值
    // uint64_t total_processing_cycles = total_interrupts * avg_cycles;
    // printf("Estimated total interrupt processing: %lu cycles\n", total_processing_cycles);
    
    printf("=== Interrupt Overhead Analysis Completed ===\n\n");
}

// 综合系统测试函数
void run_comprehensive_tests(void) {
    printf("=== COMPREHENSIVE SYSTEM TESTS ===\n\n");
    
    // 1. 基本异常处理测试
    test_exception_handling();
    
    // 2. 中断性能测试
    test_interrupt_overhead();
    
}