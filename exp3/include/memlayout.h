#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

#include <stdint.h>

/* 机器/内核重要基址 */
#define KERNBASE 0x80000000UL /* 内核加载基址（你的链接脚本中 . = 0x80000000） */
#define UART0 0x10000000UL    /* QEMU virt 上的 16550 UART MMIO */

/* 物理内存上限（可按 QEMU -m 调整；默认给 128 MiB） */
#ifndef PHYSTOP
#define PHYSTOP (KERNBASE + 128UL * 1024 * 1024)
#endif

/* 页大小 */
#define PGSIZE 4096UL
#define PGSHIFT 12

/* 对齐宏 */
#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) ((a) & ~(PGSIZE - 1))

#endif /* MEMLAYOUT_H */
