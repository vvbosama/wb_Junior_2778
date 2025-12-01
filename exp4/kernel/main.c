// kernel/main.c
#include "types.h"
#include "printf.h"
#include "console.h"
#include "trap.h"
#include "clock.h"
#include "uart.h"
#include "mm.h"
#include "exception.h"

int main(void) {
    console_init();
    printf("\n"
           "===============================================\n"
           "    RISC-V OS - Three Speed Timer Test\n"
           "===============================================\n");
    
    // 初始化系统
    pmm_init();
    kvminit();
    kvminithart();
    
    printf("System initialized successfully.\n\n");
      
    // 先初始化陷阱处理，再测试异常
    printf("Initializing trap handler...\n");
    trap_init();

    // // 运行异常处理测试
    // test_exception_handling();
    run_comprehensive_tests();


    // UART输入测试
    printf("=== UART Input Test ===\n");
    printf("Type a character to verify UART: ");
    
    while (1) {
        volatile unsigned char *uart_lsr = (volatile unsigned char *)(0x10000000 + 5);
        if (*uart_lsr & 1) {
            volatile unsigned char *uart_rhr = (volatile unsigned char *)0x10000000;
            char c = *uart_rhr;
            printf("'%c' - OK\n\n", c);
            break;
        }
    }
    
    // 初始化中断系统
    printf("=== Three Speed Timer Test ===\n");
    // trap_init();
    clock_init();
    
    // 启用中断
    asm volatile("csrs mie, %0" : : "r" (1 << 7));  // MTIE
    asm volatile("csrs mstatus, %0" : : "r" (1 << 3)); // MIE
    
    printf("Three-speed timer interrupts enabled!\n\n");
    
    printf("=== TEST STARTED ===\n");
    printf("You should see three different speed outputs:\n");
    printf("  FAST   (.1s): Numbers 1-9 then +,-,*,/ symbols\n");
    printf("  MEDIUM (.25s): Capital letters A-Z (every 5 ticks)\n");
    printf("  SLOW   (.5s):  Lowercase letters a-z (every 3 ticks)\n");
    printf("\nLive tick counters will show below:\n");
    printf("Press 'q' to quit, 'r' to reset counters.\n\n");
    
    uint64_t last_fast = 0;
    uint64_t last_medium = 0;
    uint64_t last_slow = 0;
    uint64_t last_display_time = 0;
    uint64_t current_time = 0;
    
    while (1) {
         // 获取当前时间
        current_time = get_ticks(TIMER_FAST);
        
        // 每秒更新一次显示（FAST计时器每0.1秒触发一次，所以10个ticks=1秒）
        if (current_time - last_display_time >= 10) {
        // 获取当前ticks
        uint64_t fast_ticks = get_ticks(TIMER_FAST);
        uint64_t medium_ticks = get_ticks(TIMER_MEDIUM);
        uint64_t slow_ticks = get_ticks(TIMER_SLOW);
        
        // 显示实时计数（每秒更新一次）
        if (fast_ticks != last_fast || medium_ticks != last_medium || slow_ticks != last_slow) {
            printf("FAST: %lu ticks | MEDIUM: %lu ticks | SLOW: %lu ticks\n", 
                   fast_ticks, medium_ticks, slow_ticks);
            last_fast = fast_ticks;
            last_medium = medium_ticks;
            last_slow = slow_ticks;
        }
    }
        
        // 检查UART输入
        volatile unsigned char *uart_lsr = (volatile unsigned char *)(0x10000000 + 5);
        if (*uart_lsr & 1) {
            volatile unsigned char *uart_rhr = (volatile unsigned char *)0x10000000;
            char c = *uart_rhr;
            uint64_t fast_ticks = get_ticks(TIMER_FAST);
        uint64_t medium_ticks = get_ticks(TIMER_MEDIUM);
        uint64_t slow_ticks = get_ticks(TIMER_SLOW);
            if (c == 'q' || c == 'Q') {
                printf("\n\n=== TEST COMPLETED ===\n");
                printf("Final counts:\n");
                printf("  FAST:   %llu ticks\n", fast_ticks);
                printf("  MEDIUM: %llu ticks\n", medium_ticks);
                printf("  SLOW:   %llu ticks\n", slow_ticks);
                printf("\nAll three speed tests completed successfully!\\n");
                break;
            } else if (c == 'r' || c == 'R') {
                reset_ticks(TIMER_FAST);
                reset_ticks(TIMER_MEDIUM);
                reset_ticks(TIMER_SLOW);
                printf("\nCounters reset!\n");
            } else {
                printf("\nInput: '%c' (UART working, press 'q' to quit)\\n", c);
            }
        }
        
        // 防止优化
        asm volatile("" ::: "memory");
    }
    
    printf("\nSystem halted.\n");
    while(1) {
        asm volatile("wfi");
    }
    
    
    return 0;
}