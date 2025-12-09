// kernel/priority.h
#ifndef _PRIORITY_H
#define _PRIORITY_H

#include "proc.h"

// 优先级调度函数
void priority_init(void);
void priority_scheduler(void);
int set_priority(int pid, int priority);
int set_nice(int pid, int nice);
int get_priority(int pid);
int get_dynamic_priority(int pid);
void show_priority_info(void);
// 优先级测试任务
void high_priority_task(void);
void medium_priority_task(void);
void low_priority_task(void);
void cpu_intensive_priority_task(void);

// 综合测试函数
void run_priority_scheduling_tests(void);

#endif