// kernel/clock.c
#include "clock.h"
#include "printf.h"

#define CLINT_MTIME 0x200BFF8
#define CLINT_MTIMECMP 0x2004000

static uint64_t timer_ticks[NUM_TIMERS] = {0};  //存储三种不同类型定时器的滴答计数数组
static uint64_t next_event_time = 0;            //下一个定时器事件时间
  
// 获取当前时间
static inline uint64_t read_mtime(void) {
    return *(volatile uint64_t*)CLINT_MTIME;
}

// 写入 mtimecmp
// 当 mtime >= mtimecmp 时触发中断
static inline void write_mtimecmp(uint64_t value) {
    *(volatile uint64_t*)CLINT_MTIMECMP = value;
}

// 计算下一个事件时间（三个定时器的最小间隔）
static uint64_t calculate_next_event(void) {
    uint64_t current_time = read_mtime();
    uint64_t min_next = (uint64_t)-1;   // 初始化为最大值
    
    // 计算三个定时器下一次触发的时间
    for (int i = 0; i < NUM_TIMERS; i++) {
        uint64_t interval;
        switch (i) {
            case TIMER_FAST:   interval = INTERVAL_FAST; break;
            case TIMER_MEDIUM: interval = INTERVAL_MEDIUM; break;
            case TIMER_SLOW:   interval = INTERVAL_SLOW; break;
            default:           interval = INTERVAL_MEDIUM; break;
        }
        
        // 计算这个定时器下一次应该触发的时间
        uint64_t next_trigger = ((timer_ticks[i] * interval) / INTERVAL_FAST) * INTERVAL_FAST + interval;
        uint64_t next_time = current_time + next_trigger;
        
        if (next_time < min_next) {
            min_next = next_time;
        }
    }
    
    return min_next;
}

// 检查并更新所有定时器
static void update_all_timers(void) {
    uint64_t current_time = read_mtime();
    
    // 检查每个定时器是否需要触发
    for (int i = 0; i < NUM_TIMERS; i++) {
        uint64_t interval;
        switch (i) {
            case TIMER_FAST:   interval = INTERVAL_FAST; break;
            case TIMER_MEDIUM: interval = INTERVAL_MEDIUM; break;
            case TIMER_SLOW:   interval = INTERVAL_SLOW; break;
            default:           interval = INTERVAL_MEDIUM; break;
        }
        
        // 检查是否到了这个定时器的触发时间
        uint64_t expected_ticks = current_time / interval;
        if (expected_ticks > timer_ticks[i]) {
            timer_ticks[i] = expected_ticks;
        }
    }
}

// 时钟初始化
void clock_init(void) {
    uint64_t current_time = read_mtime();
    next_event_time = current_time + INTERVAL_FAST;  // 从最快定时器开始
    write_mtimecmp(next_event_time);
    
    printf("Clock: initialized with 3 timers\n");
    printf("  Fast:   %d cycles (%d Hz)\n", INTERVAL_FAST, CLOCK_FREQ / INTERVAL_FAST);
    printf("  Medium: %d cycles (%d Hz)\n", INTERVAL_MEDIUM, CLOCK_FREQ / INTERVAL_MEDIUM);
    printf("  Slow:   %d cycles (%d Hz)\n", INTERVAL_SLOW, CLOCK_FREQ / INTERVAL_SLOW);
}

// 设置下一次时钟中断
void clock_set_next_event(void) {
    update_all_timers();
    next_event_time = calculate_next_event();
    write_mtimecmp(next_event_time);
}

// 获取指定定时器的ticks
uint64_t get_ticks(timer_type_t timer) {
    if (timer < NUM_TIMERS) {
        return timer_ticks[timer];
    }
    return 0;
}

// 重置指定定时器的ticks
void reset_ticks(timer_type_t timer) {
    if (timer < NUM_TIMERS) {
        timer_ticks[timer] = 0;
    }
}