// kernel/trap.h
#ifndef _TRAP_H_
#define _TRAP_H_

#include "types.h"

// 中断原因
#define CAUSE_MISALIGNED_FETCH    0x0
#define CAUSE_FETCH_ACCESS        0x1
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT          0x3
#define CAUSE_MISALIGNED_LOAD     0x4
#define CAUSE_LOAD_ACCESS         0x5
#define CAUSE_MISALIGNED_STORE    0x6
#define CAUSE_STORE_ACCESS        0x7
#define CAUSE_USER_ECALL          0x8
#define CAUSE_SUPERVISOR_ECALL    0x9
#define CAUSE_MACHINE_ECALL       0xb
#define CAUSE_FETCH_PAGE_FAULT    0xc
#define CAUSE_LOAD_PAGE_FAULT     0xd
#define CAUSE_STORE_PAGE_FAULT    0xf

// 中断类型
#define IRQ_S_SOFT    1
#define IRQ_H_SOFT    2
#define IRQ_M_SOFT    3
#define IRQ_S_TIMER   5
#define IRQ_H_TIMER   6
#define IRQ_M_TIMER   7
#define IRQ_S_EXT     9
#define IRQ_H_EXT     10
#define IRQ_M_EXT     11

// 陷阱上下文结构
struct trap_context {
    uint64_t ra;
    uint64_t sp;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t s0;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
    
    // 特殊寄存器
    uint64_t mepc;
    uint64_t mstatus;
    uint64_t mtval;
};

// 函数声明
void trap_init(void);
void trap_handler(struct trap_context *ctx);
void enable_interrupts(void);
void disable_interrupts(void);

#endif