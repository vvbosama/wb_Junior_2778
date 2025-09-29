// kernel/tests.c
#include <stdint.h>
#include "defs.h"
#include "memlayout.h"
#include "riscv.h"

/*
 * 1) 物理内存分配器测试
 */
void test_physical_memory(void)
{
    printf("[TEST] physical memory start\n");

    // 测试基本分配和释放
    void *page1 = alloc_page();
    void *page2 = alloc_page();
    assert(page1 != 0 && page2 != 0);
    assert(page1 != page2);
    assert((((uint64_t)page1) & 0xFFFUL) == 0); // 页对齐检查
    assert((((uint64_t)page2) & 0xFFFUL) == 0);

    // 测试数据写入
    *(volatile int *)page1 = 0x12345678;
    assert(*(volatile int *)page1 == 0x12345678);

    // 测试释放和重新分配
    free_page(page1);
    void *page3 = alloc_page();
    // page3 可能等于 page1（取决于分配策略）
    assert(page3 != 0);

    free_page(page2);
    free_page(page3);

    printf("[TEST] physical memory OK\n");
}

/*
 * 2) 页表功能测试
 */
void test_pagetable(void)
{
    printf("[TEST] pagetable start\n");

    pagetable_t pt = create_pagetable();
    assert(pt != 0);

    // 测试基本映射
    uint64_t va = 0x10000000UL;           // 任取对齐的 VA
    uint64_t pa = (uint64_t)alloc_page(); // 分到一页物理页作为目标
    assert(pa != 0);
    assert(map_page(pt, va, pa, PTE_R | PTE_W) == 0);

    // 测试地址转换
    pte_t *pte = walk_lookup(pt, va);
    assert(pte != 0 && (*pte & PTE_V));
    assert(PTE2PA(*pte) == (pa & ~(PGSIZE - 1)));

    // 测试权限位
    assert((*pte & PTE_R) != 0);
    assert((*pte & PTE_W) != 0);
    assert((*pte & PTE_X) == 0);

    // （调试可选）打印该页表
    //  dump_pagetable(pt, 0);

    // 释放：destroy_pagetable 只释放页表页，不释放映射的物理页帧
    destroy_pagetable(pt);
    free_page((void *)pa);

    printf("[TEST] pagetable OK\n");
}

/*
 * 3) 虚拟内存激活测试
 */
void test_virtual_memory(void)
{
    printf("Before enabling paging...\n");

    // 启用分页（恒等映射）
    kvminit();
    kvminithart();

    printf("After enabling paging...\n");
    // 若能继续打印，说明内核代码可执行、数据可访问、设备（UART）可访问
    printf("[TEST] virtual memory OK\n");
}
