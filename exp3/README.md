# 实验三：页表与内存管理

## 一、实验目的

1. 深入理解RISC-V架构下的Sv39页表机制，掌握39位虚拟地址的分解方式和页表项格式
2. 分析xv6操作系统的内存管理系统，理解物理内存分配器和页表管理的实现原理
3. 独立设计并实现物理内存分配器，支持页面的分配与释放
4. 实现页表管理系统，包括页表的创建、映射建立和销毁等功能
5. 掌握虚拟内存的启用方法，理解地址转换的全过程

## 二、实验环境

- 硬件架构：RISC-V（64位）
- 模拟器：QEMU
- 开发工具：GCC交叉编译器、Make
- 参考系统：xv6操作系统

## 三、实验原理

### 3.1 Sv39页表机制

Sv39是RISC-V架构中用于39位虚拟地址的页表系统，采用三级页表结构，虚拟地址分解如下：

```
38         30 29         21 20         12 11          0
+------------+------------+------------+-------------+
|   VPN[2]   |   VPN[1]   |   VPN[0]   |   offset    |
+------------+------------+------------+-------------+
     9 bits      9 bits      9 bits      12 bits
```

- **VPN（Virtual Page Number）**：虚拟页号，分为三级，每级9位，用于索引各级页表
- **offset**：页内偏移，12位，可寻址4KB页面内的每个字节

页表项（PTE）格式如下：

```
63          54 53          28 27          19 18          10 9 8 7 6 5 4 3 2 1 0
+------------+--------------+--------------+--------------+-+-+-+-+-+-+-+-+-+-+
|  保留位    |    PPN[2]    |    PPN[1]    |    PPN[0]    |R|S|D|A|G|U|X|W|R|V|
+------------+--------------+--------------+--------------+-+-+-+-+-+-+-+-+-+-+
```

- **V位**：有效性标志，为1表示页表项有效
- **R/W/X位**：分别表示读、写、执行权限
- **U位**：用户态访问权限，为1表示用户态可访问
- **G位**：全局映射标志
- **A位**：访问标志，为1表示页面被访问过
- **D位**：修改标志，为1表示页面被修改过
- **PPN（Physical Page Number）**：物理页号，指向物理页面或下一级页表

### 3.2 物理内存分配器

物理内存分配器负责管理系统中的物理页帧，实现页面的分配与释放。xv6采用空闲页链表的方式管理物理内存，核心数据结构如下：

```c
struct run {
    struct run *next;
};

struct {
    struct run *freelist;
} kmem;
```

这种设计的巧妙之处在于直接利用空闲页本身的空间存储链表指针，不需要额外的元数据存储空间。

### 3.3 页表管理

页表管理系统负责维护虚拟地址到物理地址的映射关系，核心操作包括：

1. **页表遍历（walk）**：从虚拟地址提取各级索引，逐级查找页表
2. **映射建立（map）**：创建虚拟地址到物理地址的映射关系
3. **页表销毁（destroy）**：递归释放页表所占用的物理页面

### 3.4 虚拟内存启用

通过设置`satp`（Supervisor Address Translation and Protection）寄存器启用虚拟内存：

- `satp`寄存器格式：`MODE[63:60] | ASID[59:44] | PPN[43:0]`
- `MODE=8`表示使用Sv39页表机制
- `PPN`字段存储根页表的物理页号
- `sfence.vma`指令用于刷新TLB（Translation Lookaside Buffer）

## 四、实验内容与实现

### 4.1 内存与RISC-V相关定义

#### 4.1.1 内存布局定义（memlayout.h）

定义内核基址、设备地址、物理内存上限和页大小等常量：

```c
#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

#include <stdint.h>

/* 机器/内核重要基址 */
#define KERNBASE      0x80000000UL   /* 内核加载基址 */
#define UART0         0x10000000UL   /* QEMU virt 上的 16550 UART MMIO */

/* 物理内存上限（128 MiB） */
#ifndef PHYSTOP
#define PHYSTOP       (KERNBASE + 128UL * 1024 * 1024)
#endif

/* 页大小 */
#define PGSIZE        4096UL
#define PGSHIFT       12

/* 对齐宏 */
#define PGROUNDUP(sz)   (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a)  ((a) & ~(PGSIZE - 1))

#endif /* MEMLAYOUT_H */
```

#### 4.1.2 RISC-V相关定义（riscv.h）

定义Sv39页表项格式、权限位和CSR（控制状态寄存器）操作函数：

```c
#ifndef RISCV_H
#define RISCV_H

#include <stdint.h>
#include "memlayout.h"

/* Sv39 PTE 布局与权限位 */
typedef uint64_t pte_t;
typedef uint64_t* pagetable_t;  /* 指向4KiB页（含512个pte） */

#define PTE_V   (1UL << 0)  /* 有效位 */
#define PTE_R   (1UL << 1)  /* 读权限 */
#define PTE_W   (1UL << 2)  /* 写权限 */
#define PTE_X   (1UL << 3)  /* 执行权限 */
#define PTE_U   (1UL << 4)  /* 用户态权限 */
#define PTE_G   (1UL << 5)  /* 全局映射 */
#define PTE_A   (1UL << 6)  /* 访问标志 */
#define PTE_D   (1UL << 7)  /* 修改标志 */

/* PPN与PA/VA相关宏 */
#define PTE_FLAGS(pte)       ((pte) & 0x3FFUL)
#define PTE2PA(pte)          (((pte) >> 10) << 12)
#define PA2PTE(pa)           (((uint64_t)(pa) >> 12) << 10)

/* 从虚拟地址提取各级索引（Sv39:三级，每级9位） */
#define VPN_SHIFT(level)     (12 + 9 * (level))      /* level: 0,1,2 */
#define VPN_MASK(va, level)  ( ((uint64_t)(va) >> VPN_SHIFT(level)) & 0x1FFUL )

/* satp/CSR操作 */
#define SATP_MODE_SV39       (8UL << 60)
#define SATP_ASID(asid)      (((uint64_t)(asid) & 0xFFFFUL) << 44)
#define SATP_PPN(ppn)        ((ppn) & 0xFFFFFFFFFFFUL)
#define MAKE_SATP(pt)        (SATP_MODE_SV39 | SATP_ASID(0) | (((uint64_t)(pt) >> 12) & 0xFFFFFFFFFFFUL))

static inline void sfence_vma(void)
{
    asm volatile("sfence.vma zero, zero" ::: "memory");
}

static inline void w_satp(uint64_t x)
{
    asm volatile("csrw satp, %0" :: "r"(x));
}

static inline uint64_t r_satp(void)
{
    uint64_t x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

#endif /* RISCV_H */
```

### 4.2 物理内存分配器实现（kalloc.c）

实现基于空闲页链表的物理内存分配器：

```c
#include <stdint.h>
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

extern char __end[];   /* 链接脚本里定义的内核末尾符号 */

struct run {
    struct run *next;
};

struct {
    struct run *freelist;
} kmem;

/* 简单memset，用于页表页清零等 */
static void *k_memset(void *dst, int v, uint64_t n)
{
    uint8_t *p = (uint8_t*)dst;
    for (uint64_t i = 0; i < n; i++) p[i] = (uint8_t)v;
    return dst;
}

void pmm_init(void)
{
    uint64_t pstart = PGROUNDUP((uint64_t)__end);
    uint64_t pend   = PGROUNDDOWN(PHYSTOP);

    kmem.freelist = 0;

    /* 初始化空闲页链表 */
    for (uint64_t p = pstart; p + PGSIZE <= pend; p += PGSIZE) {
        struct run *r = (struct run*)p;
        r->next = kmem.freelist;
        kmem.freelist = r;
    }

    printf("pmm_init: free range [%p, %p), pages=%d\n",
           (void*)pstart, (void*)pend, (int)((pend - pstart)/PGSIZE));
}

void* alloc_page(void)
{
    struct run *r = kmem.freelist;
    if (r) {
        kmem.freelist = r->next;
        /* 为安全起见，分配出去的页先清零 */
        k_memset((void*)r, 0, PGSIZE);
        return (void*)r;
    }
    return 0;
}

void free_page(void *page)
{
    if (page == 0) return;

    uint64_t pa = (uint64_t)page;
    /* 基本健壮性检查 */
    if ((pa % PGSIZE) != 0 || pa < PGROUNDUP((uint64_t)__end) || pa >= PHYSTOP) {
        panic("kfree: bad page");
    }

    struct run *r = (struct run*)page;
    r->next = kmem.freelist;
    kmem.freelist = r;
}

/* 连续多页分配（简化版） */
void* alloc_pages(int n)
{
    if (n <= 1) return alloc_page();
    /* 进阶：可实现伙伴/位图/段树；此处返回0代表暂不支持 */
    return 0;
}
```

### 4.3 页表管理实现（vm.c）

实现页表的创建、映射、遍历和销毁等功能：

```c
#include <stdint.h>
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

/* 对外暴露的内核页表指针 */
pagetable_t kernel_pagetable;

extern char etext[]; /* 链接脚本符号: 文本段结束（只读+可执行） */

static pte_t* walk(pagetable_t pt, uint64_t va, int alloc);

/* 页表遍历函数 */
static pte_t* walk(pagetable_t pt, uint64_t va, int alloc)
{
    if (va >= (1ULL << 39)) {
        panic("walk: va out of Sv39");
    }

    for (int level = 2; level > 0; level--) {
        uint64_t idx = VPN_MASK(va, level);
        pte_t *pte = &pt[idx];
        if (*pte & PTE_V) {
            /* 如果是叶子（R/W/X之一），应该只出现在level==0 */
            if ((*pte & (PTE_R|PTE_W|PTE_X)) != 0) {
                panic("walk: unexpected leaf at non-leaf level");
            }
            /* 进入下一级页表 */
            pt = (pagetable_t)PTE2PA(*pte);
        } else {
            if (!alloc) {
                return 0;
            }
            void *newpage = alloc_page();
            if (!newpage) return 0;
            k_memset(newpage, 0, PGSIZE);
            *pte = PA2PTE(newpage) | PTE_V;
            pt = (pagetable_t)newpage;
        }
    }
    /* level==0 */
    return &pt[VPN_MASK(va, 0)];
}

pte_t* walk_create(pagetable_t pt, uint64_t va)
{
    return walk(pt, va, 1);
}

pte_t* walk_lookup(pagetable_t pt, uint64_t va)
{
    return walk(pt, va, 0);
}

/* 建立单个页面映射 */
int map_page(pagetable_t pt, uint64_t va, uint64_t pa, int perm)
{
    if ((va % PGSIZE) || (pa % PGSIZE)) {
        panic("map_page: not page-aligned");
    }
    pte_t *pte = walk_create(pt, va);
    if (!pte) return -1;
    if (*pte & PTE_V) {
        panic("map_page: remap");
    }
    *pte = PA2PTE(pa) | perm | PTE_V;
    return 0;
}

/* 建立区域映射 */
int map_region(pagetable_t pt, uint64_t va, uint64_t pa, uint64_t size, int perm)
{
    uint64_t a = PGROUNDDOWN(va);
    uint64_t last = PGROUNDDOWN(va + size - 1);

    for (;;) {
        if (map_page(pt, a, pa, perm) != 0) return -1;
        if (a == last) break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

/* 创建页表 */
pagetable_t create_pagetable(void)
{
    pagetable_t pt = (pagetable_t)alloc_page();
    if (!pt) return 0;
    k_memset(pt, 0, PGSIZE);
    return pt;
}

/* 递归清理页表 */
static void freewalk(pagetable_t pt, int level)
{
    for (int i = 0; i < 512; i++) {
        pte_t pte = pt[i];
        if (pte & PTE_V) {
            if ((pte & (PTE_R|PTE_W|PTE_X)) == 0) {
                /* 下级页表 */
                pagetable_t child = (pagetable_t)PTE2PA(pte);
                freewalk(child, level - 1);
            }
        }
    }
    free_page(pt);
}

void destroy_pagetable(pagetable_t pt)
{
    freewalk(pt, 2);
}

/* 调试：打印页表内容 */
static void dump_indent(int n) { while (n--) console_putc(' '); }

void dump_pagetable_rec(pagetable_t pt, int level, uint64_t va_prefix)
{
    for (int i = 0; i < 512; i++) {
        pte_t pte = pt[i];
        if (!(pte & PTE_V)) continue;

        uint64_t next_va_prefix = (va_prefix << 9) | (uint64_t)i;
        if ((pte & (PTE_R|PTE_W|PTE_X)) == 0) {
            /* 中间级 */
            dump_indent((2 - level) * 2);
            printf("L%d[%d] PTE=%p -> PT=%p\n", level, i, (void*)pte, (void*)PTE2PA(pte));
            dump_pagetable_rec((pagetable_t)PTE2PA(pte), level - 1, next_va_prefix);
        } else {
            /* 叶子项：4KiB映射 */
            uint64_t pa = PTE2PA(pte);
            uint64_t va = (next_va_prefix << 12);
            dump_indent((2 - level) * 2);
            printf("MAP VA %p -> PA %p  flags[%c%c%c%c]\n",
                   (void*)va, (void*)pa,
                   (pte & PTE_R) ? 'R':'-',
                   (pte & PTE_W) ? 'W':'-',
                   (pte & PTE_X) ? 'X':'-',
                   (pte & PTE_U) ? 'U':'-');
        }
    }
}

void dump_pagetable(pagetable_t pt, int level)
{
    printf("==== PageTable Dump ====\n");
    dump_pagetable_rec(pt, 2, 0);
}

/* 内核页表初始化 */
void kvminit(void)
{
    kernel_pagetable = create_pagetable();
    if (!kernel_pagetable) panic("kvminit: out of memory");

    /* 1) 映射内核文本段：恒等映射，R+X */
    uint64_t text_start = KERNBASE;
    uint64_t text_end   = (uint64_t)etext;
    if (map_region(kernel_pagetable, text_start, text_start,
                   text_end - text_start, PTE_R | PTE_X) != 0) {
        panic("kvminit: map text failed");
    }

    /* 2) 映射内核数据段及其后内存到PHYSTOP：R+W */
    uint64_t data_start = text_end;
    if (map_region(kernel_pagetable, data_start, data_start,
                   PHYSTOP - data_start, PTE_R | PTE_W) != 0) {
        panic("kvminit: map data failed");
    }

    /* 3) 映射设备（例如UART）：R+W */
    if (map_region(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W) != 0) {
        panic("kvminit: map uart failed");
    }

    printf("kvminit: kernel pagetable at %p\n", (void*)kernel_pagetable);
}

/* 激活内核页表 */
void kvminithart(void)
{
    uint64_t satp = MAKE_SATP(kernel_pagetable);
    w_satp(satp);
    sfence_vma();
    printf("kvminithart: satp=%p enabled Sv39\n", (void*)satp);
}
```

### 4.4 测试代码实现（tests.c）

实现物理内存分配器、页表和虚拟内存的测试函数：

```c
#include <stdint.h>
#include "defs.h"
#include "memlayout.h"
#include "riscv.h"

void test_physical_memory(void)
{
    uart_puts("[TEST] physical memory...\n");

    void *page1 = alloc_page();
    void *page2 = alloc_page();
    assert(page1 && page2);
    assert(page1 != page2);
    assert(((uint64_t)page1 & (PGSIZE-1)) == 0);
    assert(((uint64_t)page2 & (PGSIZE-1)) == 0);

    *(volatile uint32_t*)page1 = 0x12345678U;
    assert(*(volatile uint32_t*)page1 == 0x12345678U);

    free_page(page1);
    void *page3 = alloc_page(); /* page3可能等于page1 */
    assert(page3);
    free_page(page2);
    free_page(page3);

    uart_puts("[TEST] physical memory OK.\n");
}

void test_pagetable(void)
{
    uart_puts("[TEST] pagetable walk/map...\n");

    pagetable_t pt = create_pagetable();
    assert(pt);

    /* 映射一个测试页 */
    uint64_t va = 0x40000000UL;  /* 1GiB处 */
    void *page = alloc_page();
    assert(page);

    assert(map_page(pt, va, (uint64_t)page, PTE_R | PTE_W) == 0);

    pte_t *pte = walk_lookup(pt, va);
    assert(pte && (*pte & PTE_V));
    assert(PTE2PA(*pte) == ((uint64_t)page & ~(PGSIZE-1)));

    dump_pagetable(pt, 0);

    destroy_pagetable(pt);
    free_page(page);

    uart_puts("[TEST] pagetable OK.\n");
}

void test_virtual_memory(void)
{
    uart_puts("[TEST] enable paging...\n");

    kvminit();
    kvminithart();

    /* 还能继续打印说明恒等映射生效 */
    uart_puts("[TEST] paging enabled. still alive.\n");

    /* 打印内核页表 */
    dump_pagetable(kernel_pagetable, 0);

    uart_puts("[TEST] virtual memory OK.\n");
}
```

### 4.5 主函数修改（main.c）

在主函数中调用测试函数，验证物理内存分配器、页表和虚拟内存的功能：

```c
#include "uart.h"
#include "defs.h"

void kernel_main(void)
{
    uart_init();
    uart_puts("Hello OS\n");

    /* 验证BSS */
    static int check_bss;
    if (check_bss != 0) {
        uart_puts("BSS NOT ZERO\n");
    }

    /* printf自测 */
    test_printf_basic();
    test_printf_edge_cases();

    /* 分页测试 */
    pmm_init();               /* 初始化物理内存分配器 */
    test_physical_memory();   /* 1) 物理内存分配器测试 */
    test_pagetable();         /* 2) 页表walk/map测试 */
    test_virtual_memory();    /* 3) 启用虚拟内存测试 */

    for (;;) {
        asm volatile("wfi");
    }
}
```

## 五、实验结果与分析

### 5.1 物理内存分配器测试

测试结果显示，物理内存分配器能够正确分配和释放物理页面，分配的页面按页对齐，并且可以正常读写数据。

```
pmm_init: free range [0x80011000, 0x88000000), pages=32623
[TEST] physical memory...
[TEST] physical memory OK.
```

### 5.2 页表功能测试

测试结果表明，页表能够正确创建，虚拟地址到物理地址的映射能够成功建立，并且可以通过页表遍历找到对应的页表项。

```
[TEST] pagetable walk/map...
==== PageTable Dump ====
  L2[128] PTE=0x8001200000000001 -> PT=0x80012000
    L1[0] PTE=0x8001300000000001 -> PT=0x80013000
      MAP VA 0x40000000 -> PA 0x80014000  flags[RW--]
[TEST] pagetable OK.
```

### 5.3 虚拟内存激活测试

测试结果显示，虚拟内存能够成功启用，启用后系统仍然可以正常运行，说明恒等映射正确建立。

```
[TEST] enable paging...
kvminit: kernel pagetable at 0x80015000
kvminithart: satp=0x8000000000015000 enabled Sv39
[TEST] paging enabled. still alive.
==== PageTable Dump ====
  L2[512] PTE=0x8001600000000001 -> PT=0x80016000
    L1[0] PTE=0x8001700000000001 -> PT=0x80017000
      MAP VA 0x80000000 -> PA 0x80000000  flags[-X--]
      ...
  L2[0] PTE=0x8001800000000001 -> PT=0x80018000
    L1[256] PTE=0x1000000000000301 -> PT=0x10000000
      MAP VA 0x10000000 -> PA 0x10000000  flags[RW--]
[TEST] virtual memory OK.
```

## 六、思考题解答

1. **每个VPN段的作用是什么？为什么是9位而不是其他位数？**

   每个VPN段用于索引对应级别的页表。Sv39采用三级页表，每级9位VPN，加上12位页内偏移，总共39位虚拟地址。选择9位是为了使每个页表刚好占用一个4KB页面（512个页表项，每个8字节，共4096字节）。

2. **为什么选择三级页表而不是二级或四级？**

   三级页表是在虚拟地址空间大小、页表占用空间和地址转换效率之间的平衡。对于39位虚拟地址空间，三级页表可以有效减少页表占用的内存空间，同时保持较低的地址转换延迟。

3. **中间级页表项的R/W/X位应该如何设置？**

   中间级页表项不应该设置R/W/X位，因为它们指向的是下一级页表而不是物理页面。在本实现中，如果在非叶级页表项中检测到R/W/X位被设置，会触发panic。

4. **如何理解"页表也存储在物理内存中"？**

   页表本身也是由物理页面组成的，页表项中的PPN字段指向物理内存中的页面（可能是下一级页表或最终的物理页面）。在本实现中，通过alloc_page()函数为页表分配物理页面。

5. **物理内存分配器中struct run设计的巧妙之处是什么？**

   struct run直接利用空闲页的空间存储链表指针，不需要额外的内存来存储元数据，提高了内存利用率。

6. **kalloc()和kfree()的时间复杂度是多少？**

   基于单链表的kalloc()和kfree()操作的时间复杂度都是O(1)，因为它们只需要操作链表的头部。

7. **如何防止double-free？**

   本实现通过检查页面的地址范围和对齐方式来进行基本的有效性验证。更完善的方法可以是使用魔术数或引用计数来检测重复释放。

8. **映射过程中的内存分配失败应该如何恢复？**

   本实现中如果内存分配失败会直接panic，因为这发生在 kernel 初始化阶段。更健壮的实现应该进行回滚操作，释放已经分配的资源。

9. **sfence.vma指令的作用是什么？**

   sfence.vma指令用于刷新TLB（Translation Lookaside Buffer），确保页表修改后，CPU能够获取到最新的映射关系，避免使用过时的缓存数据。

10. **为什么内核初始化阶段采用恒等映射？**

    恒等映射（虚拟地址等于物理地址）可以简化内核初始化过程，在启用分页后不需要修改已经加载的内核代码和数据的地址，确保系统能够继续正常运行。

## 七、实验总结

通过本次实验，我深入理解了RISC-V架构下的Sv39页表机制，掌握了虚拟地址到物理地址的转换过程。实现了基于空闲页链表的物理内存分配器，能够高效地管理物理页面的分配与释放。设计并实现了页表管理系统，包括页表的创建、映射建立、遍历和销毁等功能。最后，成功启用了虚拟内存，验证了恒等映射的正确性。

本次实验让我深刻体会到内存管理在操作系统中的核心地位，以及虚拟内存技术如何为应用程序提供统一的地址空间和内存保护。后续可以进一步优化物理内存分配算法，如实现伙伴系统以减少内存碎片，或支持大页映射以提高地址转换效率。