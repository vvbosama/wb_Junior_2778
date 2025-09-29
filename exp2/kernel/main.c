/* kernel/main.c */
#include "uart.h"
#include "defs.h"
#include "console.h"
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

    uart_puts("Line 1\n");
    uart_puts("Line 2\n");
    uart_puts("Line 3 (will be cleared in 1 sec)\n");

    // 粗暴“延时”（忙等），避免清屏太快看不到前面的内容
    for (volatile unsigned long i = 0; i < 2000000000UL; i++)
    {
    }

    clear_screen(); // <—— 这里调用

    for (volatile unsigned long i = 0; i < 2000000000UL; i++)
    {
    }

    uart_puts("Screen cleared!\n");

    // 再测试一下定位功能（可选）
    goto_xy(10, 5); // 第5行第10列
    uart_puts("Hello at (10,5)\n");

    test_printf_basic();

    test_printf_edge_cases();

    /* 永远循环，保持内核不返回 */
    for (;;)
    {
        asm volatile("wfi");
    }
}
