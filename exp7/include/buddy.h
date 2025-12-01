// buddy.h
#ifndef _BUDDY_H_
#define _BUDDY_H_

#include "types.h"
#include "mm.h"

// 块状态
#define BUDDY_FREE        0
#define BUDDY_ALLOCATED   1
#define BUDDY_SPLIT       2

// 伙伴系统结构
struct buddy_pool {
    struct list_head free_lists[BUDDY_MAX_ORDER + 1];  // 各阶空闲链表
    uint64_t pool_start;         // 内存池起始地址
    uint64_t pool_size;          // 内存池大小
    uint8_t *bitmap;             // 位图，用于跟踪页面状态
    uint64_t total_pages;        // 总页数
    uint64_t used_pages;         // 已使用页数
    uint64_t bitmap_size;        // 位图大小
};

// 伙伴系统API
void buddy_init(void);
void* buddy_alloc(int order);
void buddy_free(void* addr, int order);
void buddy_dump(void);
uint64_t buddy_get_total_pages(void);
uint64_t buddy_get_used_pages(void);

// 内部函数声明
void set_buddy_status(uint64_t index, int order, int status);
int get_buddy_status(uint64_t index, int order);
int is_buddy_free(uint64_t index, int order);

#endif