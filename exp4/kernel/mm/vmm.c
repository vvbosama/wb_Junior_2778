// 虚拟内存管理
#include "mm.h"
#include "printf.h"
#include "console.h"

// 内联的简单内存设置函数 - 使用 uint64_t 替代 size_t
// 将指定内存区域填充为特定值
static inline void simple_memset(void* dst, char value, uint64_t n) {
    char* ptr = (char*)dst;
    for (uint64_t i = 0; i < n; i++) {
        ptr[i] = value;
    }
}

//遍历页表，为虚拟地址创建或查找对应的页表项。
static pte_t* walk_create(pagetable_t pt, uint64_t va, int alloc) {
    pagetable_t current_pt = pt;
    
    for(int level = 2; level > 0; level--) {
        uint64_t vpn = VA2VPN(va, level);// 提取指定层级的虚拟页号
        pte_t* pte = &current_pt[vpn];// 获取当前级别的PTE指针
        
        if(*pte & PTE_V) {// 如果PTE有效位被设置
            current_pt = (pagetable_t)PTE_PA(*pte);// 进入下一级页表
        } else {
            if(!alloc) // 如果不允许分配新页表
                return NULL;
                
            pagetable_t new_pt = alloc_page();// 分配新页表页
            if(!new_pt) // 分配失败
                return NULL;
                
            simple_memset(new_pt, 0, PAGE_SIZE);// 清零新页表
            *pte = ((uint64_t)new_pt >> 12) <<10| PTE_V;// 设置PTE
            current_pt = new_pt;// 进入新创建的页表
        }
    }
    
    return &current_pt[VA2VPN(va, 0)];// 返回第0级的PTE指针
}

//创建页表
pagetable_t create_pagetable(void) {
    pagetable_t pt = alloc_page();
    if(pt) {
        simple_memset(pt, 0, PAGE_SIZE);
    }
    return pt;
}

//建立虚拟地址到物理地址映射的函数
//perm: 权限标志（如PTE_R、PTE_W、PTE_X等）
int map_page(pagetable_t pt, uint64_t va, uint64_t pa, int perm) {
    if(va % PAGE_SIZE != 0 || pa % PAGE_SIZE != 0)
        return -1;
    
    pte_t* pte = walk_create(pt, va, 1);
    //!pte: walk_create返回NULL，可能是：内存不足，无法分配新页表页
    //(*pte & PTE_V): 目标PTE已经有效，防止重复映射
    if(!pte || (*pte & PTE_V))
        return -1;

    *pte = (pa >> 12)<<10 | perm | PTE_V;//物理地址右移12位（物理地址有12位offset，PTE中物理页号后面还有10位）
    return 0;
}

//页表查找函数
pte_t* walk_lookup(pagetable_t pt, uint64_t va) {
    pagetable_t current_pt = pt;// 从根页表开始
    
    for(int level = 2; level >= 0; level--) {
        pte_t* pte = &current_pt[VA2VPN(va, level)];
        if(!(*pte & PTE_V))
            return NULL;
        if(level == 0)
            return pte;
        current_pt = (pagetable_t)PTE_PA(*pte);
    }
    return NULL;
}

// 改进的页表转储函数，递归遍历所有层级
void dump_pagetable_recursive(pagetable_t pt, int level, uint64_t base_va) {
    for(int i = 0; i < 512; i++) { // 遍历所有512个PTE
        if(pt[i] & PTE_V) {
            uint64_t pa = PTE_PA(pt[i]);
            uint32_t perm = pt[i] & 0xFF;
            uint64_t current_va = base_va | ((uint64_t)i << (12 + 9 * level));
            
            if(level == 0) {
                // 叶子PTE - 显示完整的映射信息
                printf("  VA %p -> PA %p perm=0x%x", 
                       (void*)current_va, (void*)pa, perm);
                
                // 显示权限的文本描述
                printf(" (");
                if(perm & PTE_R) printf("R");
                if(perm & PTE_W) printf("W");
                if(perm & PTE_X) printf("X");
                if(perm & PTE_U) printf("U");
                printf(")\n");
            } else {
                // // 非叶子PTE - 递归进入下一级
                // printf("L%d[%03d]: next PT at %p\n", level, i, (void*)pa);
                dump_pagetable_recursive((pagetable_t)pa, level - 1, current_va);
            }
        }
    }
}

// 修改 dump_pagetable 函数
void dump_pagetable(pagetable_t pt) {
    printf("=== Page Table Dump (Root at %p) ===\n", pt);
    dump_pagetable_recursive(pt, 2, 0);
}

/* Kernel page table - 只保留一个定义 */
static pagetable_t kernel_pagetable = NULL;

extern char etext[];  /* Defined in kernel.ld */

// 映射连续区域的辅助函数 - 改进版本
//pt: 目标页表指针，va: 起始虚拟地址，pa: 起始物理地址
static int map_region(pagetable_t pt, uint64_t va, uint64_t pa, uint64_t size, int perm) {
    if (size == 0) {
        return 0;  // 大小为0，不需要映射
    }
    
    // 计算需要映射的页范围
    uint64_t start_va = va;
    uint64_t end_va = va + size;
    uint64_t start_page = PGROUNDDOWN(start_va);
    uint64_t end_page = PGROUNDUP(end_va);
    
    printf("VMM: mapping region: va=%p->%p, size=%d bytes, pages=%d\n", 
           (void*)start_va, (void*)end_va, (int)size,
           (int)((end_page - start_page) / PAGE_SIZE));
    
    int mapping_count = 0;
    for (uint64_t page_va = start_page; page_va < end_page; page_va += PAGE_SIZE) {
        // 计算这个页对应的物理地址
        uint64_t page_pa = pa + (page_va - va);
        
        // 检查这个页是否已经被映射
        pte_t* existing_pte = walk_lookup(pt, page_va);
        if (existing_pte && (*existing_pte & PTE_V)) {
            printf("VMM: page %p already mapped with perm 0x%x, need 0x%x\n", 
                   (void*)page_va, (int)(*existing_pte & 0xFF), perm);
            
            // // 如果权限不同，可能需要重新映射
            // if ((*existing_pte & 0xFF) != perm) {
            //     printf("VMM: WARNING: permission conflict on page %p\n", (void*)page_va);
            // }
            continue;
        }
        
        if (map_page(pt, page_va, page_pa, perm) == 0) {
            mapping_count++;
        } else {
            printf("VMM: failed to map page at va=%p, pa=%p\n", 
                   (void*)page_va, (void*)page_pa);
            return -1;
        }
    }
    
    printf("VMM: successfully mapped %d new pages\n", mapping_count);
    return mapping_count;
}

//内核虚拟内存初始化函数 - 精确映射版本
void kvminit(void) {
    // 防止重复初始化
    if (kernel_pagetable != NULL) {
        printf("VMM: kernel page table already initialized at %p\n", kernel_pagetable);
        return;
    }
    
    kernel_pagetable = create_pagetable();
    if(!kernel_pagetable) {
        printf("VMM: failed to create kernel page table\n");
        return;
    }
    
    printf("VMM: kernel page table created at %p\n", kernel_pagetable);
    
    /* 精确的内存映射 */
    
    // 1. 映射内核代码段 (R+X权限) - 精确到实际代码大小
    uint64_t kernel_base = 0x80000000;
    uint64_t code_end = (uint64_t)etext;
    uint64_t code_size = code_end - kernel_base;
    
    printf("VMM: mapping kernel code [%p, %p) R+X, %d bytes\n", 
           (void*)kernel_base, (void*)code_end, (int)code_size);
    
    int code_mappings = map_region(kernel_pagetable, kernel_base, kernel_base, 
                                  code_size, PTE_R | PTE_X);
    if (code_mappings < 0) {
        printf("VMM: failed to map kernel code region\n");
        return;
    }
    
    // 2. 映射内核数据段 (R+W权限) - 精确到实际数据大小
    uint64_t data_start = (uint64_t)etext;
    // #define PHYSTOP 0x88000000L  // 定义物理内存顶部（可根据实际情况调整）
    #define PHYSTOP (kernel_base + 128*1024*1024)
    // extern char end[];
    // uint64_t data_end = (uint64_t)&end;
    // uint64_t data_size = data_end - data_start;
     uint64_t data_size = PHYSTOP - data_start;

    printf("VMM: mapping kernel data [%p, %p) R+W, %d bytes\n",
           (void*)data_start, (void*)PHYSTOP, (int)data_size);

    int data_mappings = 0;
    if (data_size > 0) {
        data_mappings = map_region(kernel_pagetable, data_start, data_start, 
                                  data_size, PTE_R | PTE_W);
        if (data_mappings < 0) {
            printf("VMM: failed to map kernel data region\n");
        }
    } else {
        printf("VMM: no kernel data to map\n");
    }
    
    // 3. 映射设备 (UART等) - R+W权限
    uint64_t uart_base = 0x10000000;
    
    printf("VMM: mapping UART device [%p, %p) R+W\n", 
           (void*)uart_base, (void*)(uart_base + PAGE_SIZE));
    
    int uart_result = map_page(kernel_pagetable, uart_base, uart_base, 
                              PTE_R | PTE_W);
    if (uart_result < 0) {
        printf("VMM: failed to map UART device\n");
    }
    
    printf("VMM: kernel page table initialized:\n");
    printf("     - Code: %d pages (R+X)\n", code_mappings);
    printf("     - Data: %d pages (R+W)\n", data_mappings);
    printf("     - UART: %s\n", uart_result == 0 ? "mapped (R+W)" : "mapping failed");
    
    // 显示详细的映射信息
    printf("\nVMM: Detailed mapping information:\n");
    printf("     Code: 0x80000000 - 0x%p (%d bytes)\n", (void*)code_end, (int)code_size);
    printf("     Data: 0x%p - 0x%p (%d bytes)\n", (void*)data_start, (void*)PHYSTOP, (int)data_size);
}

//启用虚拟内存 - 修改为使用更清晰的SATP构造方式
void kvminithart(void) {
    if(!kernel_pagetable) {//检查内核页表是否已正确初始化
        printf("VMM: kernel page table not initialized\n");
        return;
    }
    
    /* SATP register format for Sv39: 
     * MODE (4 bits) | ASID (16 bits) | PPN (44 bits)
     * MODE=8 for Sv39
     * PPN = physical page number >> 12
     */
    uint64_t satp = (8L << 60) | ((uint64_t)kernel_pagetable >> 12);
    
    // 写入SATP寄存器并刷新TLB
    asm volatile("csrw satp, %0" : : "r"(satp));
    asm volatile("sfence.vma");
    
    printf("VMM: virtual memory enabled (satp=%p)\n", (void*)satp);
    printf("     Kernel page table active with separate code/data mappings\n\n");
}