// kernel/syscall_test.h
#ifndef _SYSCALL_TEST_H_
#define _SYSCALL_TEST_H_

#include "types.h"
#include "syscall.h"

// 测试函数声明
void test_basic_syscalls(void);
void test_parameter_passing(void);
void test_security(void);
void test_syscall_performance(void);
void test_getprocinfo(void);  // 新增测试函数
void run_comprehensive_syscall_tests(void);

// 系统调用包装函数声明（用于测试）
int getpid(void);
int fork(void);
int exit(int status);
int wait(int *status);
int getppid(void);
int write(int fd, const void *buf, int count);
int read(int fd, void *buf, int count);
int getprocinfo(struct procinfo *info);  // 新增

// 标准库函数声明
int strlen(const char *s);


// 测试模式控制函数声明
void enable_test_mode(void);
void disable_test_mode(void);

// 系统初始化函数声明
void uart_init(void);
void clock_init(void);
void trap_init(void);
void proc_init(void);
void enable_interrupts(void);

// 内联汇编辅助宏
#define READ_TIME(reg) asm volatile("csrr %0, time" : "=r"(reg))

#endif // _SYSCALL_TEST_H_