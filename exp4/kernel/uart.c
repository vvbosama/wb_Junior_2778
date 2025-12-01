// kernel/uart.c
#include "types.h"
#include "uart.h"
#include "printf.h"

#define UART_BASE 0x10000000UL

// 从 UART 寄存器读取
static inline unsigned char uart_read_reg(int reg) {
    volatile unsigned char *addr = (volatile unsigned char *)(UART_BASE + reg);
    return *addr;
}

// 写入 UART 寄存器
static inline void uart_write_reg(int reg, unsigned char val) {
    volatile unsigned char *addr = (volatile unsigned char *)(UART_BASE + reg);
    *addr = val;
}

// 输出单个字符
void uart_putc(char c) {
    while ((uart_read_reg(UART_LSR) & LSR_TX_IDLE) == 0)
        ;
    
    uart_write_reg(UART_THR, c);
    
    if (c == '\n') {
        while ((uart_read_reg(UART_LSR) & LSR_TX_IDLE) == 0)
            ;
        uart_write_reg(UART_THR, '\r');
    }
}

// 输出字符串
void uart_puts(char *s) {
    while (*s) {
        uart_putc(*s);
        s++;
    }
}

// 检查是否有输入可用
int uart_input_available(void) {
    return (uart_read_reg(UART_LSR) & LSR_RX_READY) != 0;
}

// 读取字符
char uart_getc(void) {
    while (!uart_input_available())
        ;
    return uart_read_reg(UART_RHR);
}

// UART 初始化 - 简化版本，禁用所有中断
void uart_init(void) {
    // 禁用所有UART中断
    uart_write_reg(UART_IER, 0);
    
    // 启用FIFO
    uart_write_reg(UART_FCR, 1);
    
    printf("UART: initialized (polling mode only)\n");
}

// 空函数 - 不启用UART中断
void uart_enable_rx_interrupt(void) {
    // 什么都不做 - 保持禁用状态
}

void uart_disable_interrupts(void) {
    // 已经禁用了
}

int uart_check_interrupt(void) {
    return 0;  // 没有中断
}

void uart_interrupt_handler(void) {
    // 空函数
}