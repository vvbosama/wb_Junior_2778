/* kernel/main.c */
#include "uart.h"
#include "defs.h"
/* C 入口：kernel_main（不要使用 int main） */
void kernel_main(void)
{
    uart_init();
    uart_puts("Hello OS\n");

    /* 验证 BSS（如需） */
    static int check_bss;
    if (check_bss != 0)
    {
        uart_puts("BSS NOT ZERO\n");
    }

    test_printf_basic();

    test_printf_edge_cases();

    /* 永远循环，保持内核不返回 */
    for (;;)
    {
        asm volatile("wfi");
    }
}
