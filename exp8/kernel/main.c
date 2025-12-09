#include "printf.h"
#include "proc.h"
#include "mm.h"
#include "console.h"
#include "trap.h"
#include "syscall.h"
#include "clock.h"
#include "uart.h"
#include "syscall_test.h"
#include "fs_test.h"
#include "file_time.h"
#include "priority.h"

void run_file_time_tests(void);

void run_filesystem_tests(void);

void main(void) {
    
    // // 运行文件系统测试
    // run_filesystem_tests();

    // // 文件创建时间记录
    // run_file_time_tests();

    // 优先级调度测试
    run_priority_scheduling_tests();

    printf("=== All Tests Completed ===\n");
    
    while (1) {
        asm volatile("wfi");
    }
}