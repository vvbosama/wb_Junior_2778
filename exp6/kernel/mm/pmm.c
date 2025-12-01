//物理内存管理器
#include "mm.h"
#include "printf.h"
// 移除 #include "spinlock.h"

#define PMM_MAX_PAGES   512     /* Manage 2MB memory */

// 定义 kernel_base
uint64_t kernel_base = 0x80000000L;
#define PHYSTOP (kernel_base + 128*1024*1024)


#define CACHE_POOL_SIZE   16     // 预分配缓存池大小

struct page {
    struct page* next;// 页结构体，仅包含指向下一页的指针
    // 这个结构体直接存储在物理页的开始处，用于构建空闲链表
};

static struct page* free_list = NULL;// 空闲页链表头指针
static uint64_t pmm_base;// 物理内存管理区域的起始地址
static uint64_t pmm_end;// 物理内存管理区域的结束地址
 int total_pages = 0;// 总页数
 int used_pages = 0;// 已使用页数


void pmm_init(void) {
    /* Available memory: from end of kernel to 0x80400000 */
    extern char end[]; // 声明外部变量end，这个变量在链接脚本kernel.ld中定义，表示内核的结束地址
    pmm_base = PGROUNDUP((uint64_t)&end);// 将内核结束地址向上页对齐，作为物理内存管理的起始地址
    pmm_end = PHYSTOP;// 设置物理内存管理的结束地址为PHYSTOP
    
    printf("PMM: initializing physical memory [%p, %p)\n", 
           (void*)pmm_base, (void*)pmm_end);
    
    /* Add all available pages to free list */
    uint64_t start = PGROUNDUP(pmm_base);
    for (uint64_t pa = start; pa + PAGE_SIZE <= pmm_end; pa += PAGE_SIZE)
    // 遍历从start到pmm_end的所有页
     {
        struct page* page = (struct page*)pa;// 将物理地址转换为页结构体指针
        page->next = free_list;// 将当前页的next指针指向当前的空闲链表头,所以表头的页是最后被添加的页，表头的页的地址最大
        free_list = page;
        total_pages++;
    }
    
    printf("PMM: initialized %d free pages\n", total_pages);
}

void* alloc_page(void) {
    if (!free_list) {// 如果空闲链表为空，返回NULL表示内存耗尽
        printf("PMM: out of memory!\n");
        return NULL;
    }
    
    struct page* page = free_list;// 从链表头部获取一页
    free_list = free_list->next;// 更新链表头指针到下一页
    used_pages++;// 增加已使用页数计数
    
    /* Clear the page - 使用 uint64_t 替代 size_t */
    for (uint64_t i = 0; i < PAGE_SIZE; i += sizeof(uint64_t)) {// 以64位为单位清零整个页，确保分配的内存是干净的
        *(volatile uint64_t*)((char*)page + i) = 0;
    }
    
    return (void*)page;// 返回分配页的指针
}

void free_page(void* page) {
    if ((uint64_t)page % PAGE_SIZE != 0) {// 检查地址是否页对齐，不对齐则报错返回
        printf("PMM: free_page: unaligned address %p\n", page);
        return;
    }
    
    struct page* p = (struct page*)page;
    p->next = free_list;// 将释放的页插入到空闲链表头部
    free_list = p;// 更新链表头指针
    used_pages--;// 减少已使用页数计数
}

void* alloc_pages(int count) {
    if (count <= 0) return NULL;
    if (count == 1) return alloc_page(); // 单页直接使用原有逻辑
    
    // 简单实现：遍历空闲链表寻找连续页面
    // 注意：这在实际系统中效率较低，建议使用更高效的数据结构
    struct page *prev = NULL;
    struct page *current = free_list;
    struct page *start = NULL;
    int found_count = 0;
    
    while (current != NULL) {
        // 检查是否连续
        if (found_count == 0) {
            start = current;
            found_count = 1;
        } else if ((uint64_t)current == (uint64_t)start + found_count * PAGE_SIZE) {
            found_count++;
        } else {
            // 不连续，重新开始计数
            start = current;
            found_count = 1;
        }
        
        // 找到足够数量的连续页面
        if (found_count == count) {
            // 从空闲链表中移除这些页面
            if (prev == NULL) {
                free_list = current->next;
            } else {
                prev->next = current->next;
            }
            
            used_pages += count;
            
            // 清零所有分配的页面
            for (int i = 0; i < count; i++) {
                void* page_addr = (void*)((uint64_t)start + i * PAGE_SIZE);
                for (uint64_t j = 0; j < PAGE_SIZE; j += sizeof(uint64_t)) {
                    *(volatile uint64_t*)((char*)page_addr + j) = 0;
                }
            }
            
            return (void*)start;
        }
        
        prev = current;
        current = current->next;
    }
    
    printf("PMM: failed to allocate %d contiguous pages\n", count);
    return NULL;
}


void free_pages(void* start, int count) {
    if (start == NULL || count <= 0) return;
    
    // 验证地址对齐
    if ((uint64_t)start % PAGE_SIZE != 0) {
        printf("PMM: free_pages: unaligned address %p\n", start);
        return;
    }
    
    // 将页面逐个添加到空闲链表头部
    for (int i = count - 1; i >= 0; i--) {
        void* page_addr = (void*)((uint64_t)start + i * PAGE_SIZE);
        struct page* p = (struct page*)page_addr;
        p->next = free_list;
        free_list = p;
    }
    
    used_pages -= count;
}