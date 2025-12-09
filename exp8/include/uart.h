// include/uart.h (更新)
#ifndef _UART_H_
#define _UART_H_

// UART 寄存器偏移
#define UART_RHR 0    // 接收保持寄存器
#define UART_THR 0    // 发送保持寄存器  
#define UART_IER 1    // 中断使能寄存器
#define UART_IIR 2    // 中断标识寄存器
#define UART_FCR 2    // FIFO控制寄存器
#define UART_LCR 3    // 线路控制寄存器
#define UART_LSR 5    // 线路状态寄存器

// 线路状态寄存器位
#define LSR_RX_READY (1 << 0)   // 数据就绪
#define LSR_TX_IDLE  (1 << 5)   // 发送器空闲

// 中断使能寄存器位
#define IER_RX_ENABLE (1 << 0)  // 接收中断使能

// UART 函数声明
void uart_init(void);
void uart_putc(char c);
void uart_puts(char *s);
int uart_input_available(void);
char uart_getc(void);

// 新增中断相关函数
void uart_enable_rx_interrupt(void);
void uart_disable_interrupts(void);
int uart_check_interrupt(void);
void uart_interrupt_handler(void);

#endif