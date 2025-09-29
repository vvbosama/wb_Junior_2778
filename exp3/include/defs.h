// test.c
// void test_printf_basic();
// void test_printf_edge_cases();

#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>
#include <stddef.h>

/* ---- UART / 控制台 / printf ---- */
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);
int uart_getc_nonblock(void);

void console_init(void);
void console_putc(char c);
void console_puts(const char *s);

int printf(const char *fmt, ...);
void printfint(int x);

/* ---- 简单断言与 panic ---- */
static inline void panic(const char *msg)
{
    uart_puts("PANIC: ");
    uart_puts(msg);
    uart_puts("\n");
    for (;;)
        asm volatile("wfi");
}
#define assert(x)                           \
    do                                      \
    {                                       \
        if (!(x))                           \
            panic("assertion failed: " #x); \
    } while (0)

/* ---- 物理内存管理（pmm/kalloc） ---- */
void pmm_init(void);
void *alloc_page(void);
void free_page(void *page);
void *alloc_pages(int n);

/* ---- 页表管理（vm） ---- */
typedef uint64_t pte_t;
typedef uint64_t *pagetable_t;

pagetable_t create_pagetable(void);
int map_page(pagetable_t pt, uint64_t va, uint64_t pa, int perm);
int map_region(pagetable_t pt, uint64_t va, uint64_t pa, uint64_t size, int perm);
void destroy_pagetable(pagetable_t pt);
pte_t *walk_create(pagetable_t pt, uint64_t va);
pte_t *walk_lookup(pagetable_t pt, uint64_t va);
void dump_pagetable(pagetable_t pt, int level);

void kvminit(void);
void kvminithart(void);
extern pagetable_t kernel_pagetable;

/* ---- 测试 ---- */
void test_physical_memory(void);
void test_pagetable(void);
void test_virtual_memory(void);

#endif /* DEFS_H */
