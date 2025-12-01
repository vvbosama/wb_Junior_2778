#include <stdint.h>
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

extern char __end[]; /* 链接脚本里定义的内核末尾符号 */

struct run
{
    struct run *next;
};

struct
{
    struct run *freelist;
} kmem;

/* 简单 memset，用于页表页清零等 */
static void *k_memset(void *dst, int v, uint64_t n)
{
    uint8_t *p = (uint8_t *)dst;
    for (uint64_t i = 0; i < n; i++)
        p[i] = (uint8_t)v;
    return dst;
}

void pmm_init(void)
{
    uint64_t pstart = PGROUNDUP((uint64_t)__end);
    uint64_t pend = PGROUNDDOWN(PHYSTOP);

    kmem.freelist = 0;

    for (uint64_t p = pstart; p + PGSIZE <= pend; p += PGSIZE)
    {
        struct run *r = (struct run *)p;
        r->next = kmem.freelist;
        kmem.freelist = r;
    }

    printf("pmm_init: free range [%p, %p), pages=%d\n",
           (void *)pstart, (void *)pend, (int)((pend - pstart) / PGSIZE));
}

void *alloc_page(void)
{
    struct run *r = kmem.freelist;
    if (r)
    {
        kmem.freelist = r->next;
        /* 为安全起见，分配出去的页先清零（便于页表/内核使用） */
        k_memset((void *)r, 0, PGSIZE);
        return (void *)r;
    }
    return 0;
}

void free_page(void *page)
{
    if (page == 0)
        return;

    uint64_t pa = (uint64_t)page;
    /* 基本健壮性检查 */
    if ((pa % PGSIZE) != 0 || pa < PGROUNDUP((uint64_t)__end) || pa >= PHYSTOP)
    {
        panic("kfree: bad page");
    }

    struct run *r = (struct run *)page;
    r->next = kmem.freelist;
    kmem.freelist = r;
}

/* 连续多页分配（最简单版：n==1 时等价 alloc_page；n>1 暂不实现） */
void *alloc_pages(int n)
{
    if (n <= 1)
        return alloc_page();
    /* 进阶：可实现伙伴/位图/段树；此处返回 0 代表暂不支持。 */
    return 0;
}
