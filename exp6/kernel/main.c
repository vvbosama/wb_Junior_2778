#include "printf.h"
#include "proc.h"
#include "mm.h"
#include "console.h"
#include "trap.h"
#include "syscall.h"
#include "clock.h"
#include "uart.h"
#include "syscall_test.h"

// 函数声明
void initial_process_entry(void);
void run_comprehensive_syscall_tests(void);

// 初始进程的入口点
void initial_process_entry(void) {
    printf("🏁 Initial process %d starting tests\n", curr_proc->pid);
    
    // 这里可以放置测试代码
    run_comprehensive_syscall_tests();
    
    printf("🏁 Initial process %d tests completed\n", curr_proc->pid);
    
    // 完成后退出或等待
    while (1) {
        asm volatile("wfi");
    }
}

void main(void) {
    proc_init();
    syscall_init();
    
    printf("Setting up test environment...\n");
    
    // 创建并设置初始进程
    struct proc *test_proc = alloc_proc();
    if (test_proc) {
        curr_proc = test_proc;
        curr_proc->state = RUNNING;
        
        // 关键：设置有效的上下文
        curr_proc->context.ra = (uint64_t)initial_process_entry;
        
        // 分配栈空间
        void *stack_page = alloc_page();
        if (stack_page) {
            curr_proc->context.sp = (uint64_t)stack_page + PAGE_SIZE;
        } else {
            curr_proc->context.sp = 0x87fbb000; // 后备栈地址
        }
        
        printf("Initial process created: pid=%d, ra=%p, sp=%p\n", 
               curr_proc->pid, (void*)curr_proc->context.ra, (void*)curr_proc->context.sp);
    } else {
        printf("ERROR: Failed to create initial process\n");
        return;
    }
    
    enable_interrupts();
    
    printf("\n=== Starting System Tests ===\n\n");
    
    // 运行测试
    run_comprehensive_syscall_tests();
    
    printf("=== All Tests Completed ===\n");
    
    while (1) {
        asm volatile("wfi");
    }
}