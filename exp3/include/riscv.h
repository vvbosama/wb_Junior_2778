#ifndef RISCV_H
#define RISCV_H

#include <stdint.h>
#include "memlayout.h"

/* ========== Sv39 PTE 布局与权限位 ========== */
/* PTE: [63:54] 保留 | [53:28] PPN[2] | [27:19] PPN[1] | [18:10] PPN[0] | [9:8] RSW | [7]D [6]A [5]G [4]U [3]X [2]W [1]R [0]V */

typedef uint64_t pte_t;
typedef uint64_t *pagetable_t; /* 指向 4KiB 页（含 512 个 pte） */

#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)

/* PPN 与 PA/VA 相关宏 */
#define PTE_FLAGS(pte) ((pte) & 0x3FFUL)
#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PA2PTE(pa) (((uint64_t)(pa) >> 12) << 10)

/* 从虚拟地址提取各级索引（Sv39: 三级，每级9位） */
#define VPN_SHIFT(level) (12 + 9 * (level)) /* level: 0,1,2 */
#define VPN_MASK(va, level) (((uint64_t)(va) >> VPN_SHIFT(level)) & 0x1FFUL)

/* ========== satp/CSR 操作 ========== */
/* satp: MODE[63:60] | ASID[59:44] | PPN[43:0] */
#define SATP_MODE_SV39 (8UL << 60)
#define SATP_ASID(asid) (((uint64_t)(asid) & 0xFFFFUL) << 44)
#define SATP_PPN(ppn) ((ppn) & 0xFFFFFFFFFFFUL)
#define MAKE_SATP(pt) (SATP_MODE_SV39 | SATP_ASID(0) | (((uint64_t)(pt) >> 12) & 0xFFFFFFFFFFFUL))

static inline void sfence_vma(void)
{
    asm volatile("sfence.vma zero, zero" ::: "memory");
}

static inline void w_satp(uint64_t x)
{
    asm volatile("csrw satp, %0" ::"r"(x));
}

static inline uint64_t r_satp(void)
{
    uint64_t x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

#endif /* RISCV_H */
