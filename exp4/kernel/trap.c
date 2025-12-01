// kernel/trap.c
#include "trap.h"
#include "printf.h"
#include "clock.h"
#include "uart.h"
#include "exception.h"

// 完整的异常原因定义
static const char* trap_cause_names[] = {
    "instruction address misaligned",
    "instruction access fault", 
    "illegal instruction",
    "breakpoint",
    "load address misaligned",
    "load access fault",
    "store/AMO address misaligned", 
    "store/AMO access fault",
    "environment call from U-mode",
    "environment call from S-mode",
    "reserved",
    "environment call from M-mode", 
    "instruction page fault",
    "load page fault",
    "reserved", 
    "store/AMO page fault"
};

// kernel/trap.c - 修复 printf 格式
void trap_handler(struct trap_context *ctx) {
    // 读取陷阱原因
    uint64_t cause;
    asm volatile("csrr %0, mcause" : "=r" (cause));
    
    // 读取 mtval
    asm volatile("csrr %0, mtval" : "=r" (ctx->mtval));
    
    // 修复：使用正确的格式说明符
    printf("TRAP: cause=0x%lx, mepc=%p, mtval=%p\n", 
           cause, (void*)ctx->mepc, (void*)ctx->mtval);
    
    // 判断是中断还是异常
    if (cause & 0x8000000000000000) {
        // 中断处理
        int int_code = cause & 0x7FFFFFFFFFFFFFFF;
        printf("INTERRUPT: code=%d\n", int_code);
        
        switch (int_code) {
            case 7: // 定时器中断
                printf("\nTIMER interrupt - handling\n");
                clock_set_next_event();
                break;
            default:
                printf("Unknown interrupt: %d\n", int_code);
                break;
        }
    } else {
        // 异常处理
        int exc_code = cause & 0xF;
        
        // 显示异常信息
        printf("EXCEPTION: %d - %s\n", exc_code, 
               exc_code < 16 ? trap_cause_names[exc_code] : "unknown");
        
        // 调用异常处理模块
        handle_exception(ctx, cause);
        
        printf("Exception handled, new mepc=%p\n", (void*)ctx->mepc);
    }
}

// 陷阱初始化
void trap_init(void) {
    extern void trap_vector(void);
    uint64_t mtvec_value = (uint64_t)trap_vector;
    
    // 设置直接模式（bit 0 = 0）
    asm volatile("csrw mtvec, %0" : : "r" (mtvec_value));
    printf("Trap: mtvec set to %p (direct mode)\n", (void*)mtvec_value);
}

void enable_interrupts(void) {
    asm volatile("csrs mstatus, %0" : : "r" (1 << 3)); // MIE
    asm volatile("csrs mie, %0" : : "r" (1 << 7));     // MTIE
    printf("Interrupts enabled\n");
}

void disable_interrupts(void) {
    asm volatile("csrc mstatus, %0" : : "r" (1 << 3)); // MIE
}