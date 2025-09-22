/* kernel/uart.c - minimal 16550 UART output for QEMU virt */
#include "uart.h"
#include <stdint.h>

#define UART0 0x10000000UL
#define UART_THR 0x00
#define UART_LSR 0x05
#define LSR_THR_EMPTY 0x20

static inline void mmio_write8(uint64_t addr, uint8_t v)
{
    volatile uint8_t *p = (volatile uint8_t *)addr;
    *p = v;
}
static inline uint8_t mmio_read8(uint64_t addr)
{
    volatile uint8_t *p = (volatile uint8_t *)addr;
    return *p;
}

void uart_init(void)
{
    /* minimal: QEMU virt 16550 usually ready; 不做额外配置 */
    (void)mmio_read8(UART0 + UART_LSR); /* 触发可能的寄存器映射 */
}

void uart_putc(char c)
{
    /* 等待 THR 空 */
    while ((mmio_read8(UART0 + UART_LSR) & LSR_THR_EMPTY) == 0)
    {
        /* spin */
    }
    mmio_write8(UART0 + UART_THR, (uint8_t)c);
}

void uart_puts(const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

int uart_getc_nonblock(void)
{
    if ((mmio_read8(UART0_BASE + UART_LSR) & LSR_DR) == 0)
        return -1;
    return (int)mmio_read8(UART0_BASE + UART_RBR_THR);
}

int uart_getc(void)
{
    int ch;
    while ((ch = uart_getc_nonblock()) < 0)
    {
    }
    return ch & 0xFF;
}
