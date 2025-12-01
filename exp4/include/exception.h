// kernel/exception.h
#ifndef _EXCEPTION_H
#define _EXCEPTION_H

#include "types.h"
#include "trap.h"


// 异常处理函数
void handle_exception(struct trap_context *ctx, uint64_t cause);
void test_exception_handling(void);
void test_interrupt_overhead(void);
void run_comprehensive_tests(void);

#endif