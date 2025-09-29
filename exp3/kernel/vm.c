#include <stdint.h>
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

/* 对外暴露的内核页表指针 */
pagetable_t kernel_pagetable;

extern char etext[]; /* 链接脚本符号: 文本段结束（只读+可执行） */

static pte_t *walk(pagetable_t pt, uint64_t va, int alloc);

/* ========== 基本工具 ========== */

static void *k_memset(void *dst, int v, uint64_t n)
{
    uint8_t *p = (uint8_t *)dst;
    for (uint64_t i = 0; i < n; i++)
        p[i] = (uint8_t)v;
    return dst;
}

/* ========== walk / walk_create / walk_lookup ========== */

static pte_t *walk(pagetable_t pt, uint64_t va, int alloc)
{
    if (va >= (1ULL << 39))
    {
        panic("walk: va out of Sv39");
    }

    for (int level = 2; level > 0; level--)
    {
        uint64_t idx = VPN_MASK(va, level);
        pte_t *pte = &pt[idx];
        if (*pte & PTE_V)
        {
            /* 如果是叶子（R/W/X之一），应该只出现在 level==0；否则就是下一级页表 */
            if ((*pte & (PTE_R | PTE_W | PTE_X)) != 0)
            {
                /* 出现大页/叶子，但我们只支持 4KiB 映射，因此报错 */
                panic("walk: unexpected leaf at non-leaf level");
            }
            /* 进入下一级页表 */
            pt = (pagetable_t)PTE2PA(*pte);
        }
        else
        {
            if (!alloc)
            {
                return 0;
            }
            void *newpage = alloc_page();
            if (!newpage)
                return 0;
            k_memset(newpage, 0, PGSIZE);
            *pte = PA2PTE(newpage) | PTE_V;
            pt = (pagetable_t)newpage;
        }
    }
    /* level==0 */
    return &pt[VPN_MASK(va, 0)];
}

pte_t *walk_create(pagetable_t pt, uint64_t va)
{
    return walk(pt, va, 1);
}

pte_t *walk_lookup(pagetable_t pt, uint64_t va)
{
    return walk(pt, va, 0);
}

/* ========== 建立映射 ========== */

int map_page(pagetable_t pt, uint64_t va, uint64_t pa, int perm)
{
    if ((va % PGSIZE) || (pa % PGSIZE))
    {
        panic("map_page: not page-aligned");
    }
    pte_t *pte = walk_create(pt, va);
    if (!pte)
        return -1;
    if (*pte & PTE_V)
    {
        panic("map_page: remap");
    }
    *pte = PA2PTE(pa) | perm | PTE_V;
    return 0;
}

int map_region(pagetable_t pt, uint64_t va, uint64_t pa, uint64_t size, int perm)
{
    uint64_t a = PGROUNDDOWN(va);
    uint64_t last = PGROUNDDOWN(va + size - 1);

    for (;;)
    {
        if (map_page(pt, a, pa, perm) != 0)
            return -1;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

/* ========== 页表创建/销毁 ========== */

pagetable_t create_pagetable(void)
{
    pagetable_t pt = (pagetable_t)alloc_page();
    if (!pt)
        return 0;
    k_memset(pt, 0, PGSIZE);
    return pt;
}

/* 递归清理页表（只释放页表页，不释放映射的物理页帧） */
static void freewalk(pagetable_t pt, int level)
{
    for (int i = 0; i < 512; i++)
    {
        pte_t pte = pt[i];
        if (pte & PTE_V)
        {
            if ((pte & (PTE_R | PTE_W | PTE_X)) == 0)
            {
                /* 下级页表 */
                pagetable_t child = (pagetable_t)PTE2PA(pte);
                freewalk(child, level - 1);
            }
            else
            {
                /* 叶子映射：不在这里释放物理页帧 */
            }
        }
    }
    free_page(pt);
}

void destroy_pagetable(pagetable_t pt)
{
    freewalk(pt, 2);
}

/* ========== 调试：打印页表内容 ========== */

static void dump_indent(int n)
{
    while (n--)
        console_putc(' ');
}

void dump_pagetable_rec(pagetable_t pt, int level, uint64_t va_prefix)
{
    for (int i = 0; i < 512; i++)
    {
        pte_t pte = pt[i];
        if (!(pte & PTE_V))
            continue;

        uint64_t next_va_prefix = (va_prefix << 9) | (uint64_t)i;
        if ((pte & (PTE_R | PTE_W | PTE_X)) == 0)
        {
            /* 中间级 */
            dump_indent((2 - level) * 2);
            printf("L%d[%d] PTE=%p -> PT=%p\n", level, i, (void *)pte, (void *)PTE2PA(pte));
            dump_pagetable_rec((pagetable_t)PTE2PA(pte), level - 1, next_va_prefix);
        }
        else
        {
            /* 叶子项：4KiB 映射 */
            uint64_t pa = PTE2PA(pte);
            uint64_t va = (next_va_prefix << 12);
            dump_indent((2 - level) * 2);
            printf("MAP VA %p -> PA %p  flags[%c%c%c%c]\n",
                   (void *)va, (void *)pa,
                   (pte & PTE_R) ? 'R' : '-',
                   (pte & PTE_W) ? 'W' : '-',
                   (pte & PTE_X) ? 'X' : '-',
                   (pte & PTE_U) ? 'U' : '-');
        }
    }
}

void dump_pagetable(pagetable_t pt, int level /*unused*/)
{
    printf("==== PageTable Dump ====\n");
    dump_pagetable_rec(pt, 2, 0);
}

/* ========== 内核页表初始化与激活 ========== */

void kvminit(void)
{
    kernel_pagetable = create_pagetable();
    if (!kernel_pagetable)
        panic("kvminit: out of memory");

    /* 1) 映射内核文本段：恒等映射，R+X */
    uint64_t text_start = KERNBASE;
    uint64_t text_end = (uint64_t)etext;
    if (map_region(kernel_pagetable, text_start, text_start,
                   text_end - text_start, PTE_R | PTE_X) != 0)
    {
        panic("kvminit: map text failed");
    }

    /* 2) 映射内核数据段及其后内存到 PHYSTOP：R+W */
    uint64_t data_start = text_end;
    if (map_region(kernel_pagetable, data_start, data_start,
                   PHYSTOP - data_start, PTE_R | PTE_W) != 0)
    {
        panic("kvminit: map data failed");
    }

    /* 3) 映射设备（例如 UART）：R+W */
    if (map_region(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W) != 0)
    {
        panic("kvminit: map uart failed");
    }

    printf("kvminit: kernel pagetable at %p\n", (void *)kernel_pagetable);
}

void kvminithart(void)
{
    uint64_t satp = MAKE_SATP(kernel_pagetable);
    w_satp(satp);
    sfence_vma();
    printf("kvminithart: satp=%p enabled Sv39\n", (void *)satp);
}
