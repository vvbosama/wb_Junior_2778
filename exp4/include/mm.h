// mm.h 文件内容
#ifndef _MM_H_
#define _MM_H_

#include "types.h"
#include <stddef.h>

#define PAGE_SIZE       4096    //页大小：4KB
#define PAGE_SHIFT      12      //页大小对应的位移数（2^12 = 4096）,因此Offset占12位

/* Page alignment macros */
#define PGROUNDUP(sz)   (((sz) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))  // 向上页对齐
#define PGROUNDDOWN(a)  ((a) & ~(PAGE_SIZE - 1))                     // 向下页对齐

//页表项标志位
#define PTE_V (1L << 0)   /* Valid */
#define PTE_R (1L << 1)   /* Read */
#define PTE_W (1L << 2)   /* Write */
#define PTE_X (1L << 3)   /* Execute */
#define PTE_U (1L << 4)   /* User */

//页表项操作宏
#define PTE_PPN_SHIFT   10  // 物理页号在PTE中的偏移
#define PTE_PA(pte)     (((pte) >> PTE_PPN_SHIFT) << PAGE_SHIFT)  // 从PTE提取物理地址

/* Virtual address to VPN extraction */
// 提取指定层级的虚拟页号
#define VA2VPN(va, level) (((va) >> (12 + 9 * (level))) & 0x1FF)

/* 分级分配器配置 */
#define BUDDY_MAX_ORDER   8      // 最大阶数：2^10 = 1024页 = 4MB
#define BUDDY_MIN_ORDER   0      // 最小阶数：2^0 = 1页 = 4KB

/* 链表结构定义 - 必须放在最前面 */
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

/* 链表操作函数 - 改为静态内联函数 */
static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

static inline void list_add(struct list_head *new, struct list_head *head) {
    new->next = head->next;
    new->prev = head;
    head->next->prev = new;
    head->next = new;
}

static inline void list_del(struct list_head *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    entry->next = entry;
    entry->prev = entry;
}

static inline int list_empty(struct list_head *head) {
    return head->next == head;
}

/* 链表遍历宏 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

/* Slab分配器配置 */
#define SLAB_CACHE_SIZE   8      // Slab缓存数量
#define SLAB_MIN_SIZE     32     // 最小对象大小
#define SLAB_MAX_SIZE     2048   // 最大对象大小

struct slab_cache {
    size_t obj_size;             // 对象大小
    size_t objs_per_slab;        // 每个slab的对象数量
    struct list_head partial;    // 部分空闲slab链表
    struct list_head full;       // 完全分配slab链表
};

/* Physical Memory Manager */
//物理内存管理器函数声明
void pmm_init(void);             // 物理内存管理器初始化
void* alloc_page(void);          // 分配一页物理内存
void free_page(void* page);      // 释放一页物理内存

/* PMM 统计变量 - 外部声明 */
extern int total_pages;          //总页数
extern int used_pages;           //已使用页数

/* Virtual Memory Manager */
//虚拟内存管理器
typedef uint64_t* pagetable_t;    // 页表类型（指向页表基地址）
typedef uint64_t pte_t;           // 页表项类型

pagetable_t create_pagetable(void);   // 创建新页表
int map_page(pagetable_t pt, uint64_t va, uint64_t pa, int perm);  // 映射虚拟地址到物理地址
pte_t* walk_lookup(pagetable_t pt, uint64_t va);        // 查找虚拟地址对应的PTE
void free_pagetable(pagetable_t pt);   // 释放页表
void dump_pagetable(pagetable_t pt);   // 打印页表内容

/* Kernel Virtual Memory */
//内核虚拟内存函数
void kvminit(void);    // 初始化内核虚拟内存空间
void kvminithart(void);  // 为当前CPU hart初始化内核页表

/* Advanced Allocators */
void* alloc_pages(int count);
void free_pages(void* start, int count);
// void* alloc_page_fast(void);
// void free_page_fast(void* page);
// void prefetch_cache(int count);
// void get_cache_stats(void);

/* Buddy System */
void buddy_init(void);
void* buddy_alloc(int order);
void buddy_free(void* addr, int order);
void buddy_dump(void);
uint64_t buddy_get_total_pages(void);
uint64_t buddy_get_used_pages(void);

// /* Slab Allocator */
// void slab_init(void);
// void* slab_alloc(size_t size);
// void slab_free(void* obj);

#endif