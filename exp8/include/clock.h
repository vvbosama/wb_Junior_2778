// kernel/clock.h
#ifndef _CLOCK_H_
#define _CLOCK_H_

#include "types.h"

#define CLOCK_FREQ 10000000

// 三种不同的时钟间隔
#define INTERVAL_FAST   (CLOCK_FREQ / 10)    // 0.1秒 - 快速
#define INTERVAL_MEDIUM (CLOCK_FREQ / 4)     // 0.25秒 - 中等
#define INTERVAL_SLOW   (CLOCK_FREQ / 2)     // 0.5秒 - 慢速

// 定时器类型
typedef enum {
    TIMER_FAST,
    TIMER_MEDIUM, 
    TIMER_SLOW,
    NUM_TIMERS
} timer_type_t;

// 时钟管理函数
void clock_init(void);
void clock_set_next_event(void);
uint64_t get_ticks(timer_type_t timer);
void reset_ticks(timer_type_t timer);

#endif