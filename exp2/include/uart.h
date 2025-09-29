/* kernel/uart.h */
#ifndef UART_H
#define UART_H

/* UART 基地址 (QEMU virt 机器上 16550 UART 的 MMIO 地址) */
#define UART0_BASE 0x10000000UL

/*
 * UART 寄存器 (偏移量是相对于 UART0_BASE)
 * 注意：LCR 寄存器中的 DLAB 位会影响 0x00 和 0x01 偏移处寄存器的含义
 */

/* 0x00: 接收/发送寄存器
 *  - 读 (DLAB=0)：RBR (Receiver Buffer Register) —— 读取收到的数据
 *  - 写 (DLAB=0)：THR (Transmitter Holding Register) —— 写入要发送的数据
 *  - 写 (DLAB=1)：DLL (Divisor Latch Low) —— 波特率分频低字节
 */
#define UART_RBR_THR 0x00
#define UART_DLL 0x00

/* 0x01:
 *  - (DLAB=0)：IER (Interrupt Enable Register) —— 中断使能
 *  - (DLAB=1)：DLM (Divisor Latch High) —— 波特率分频高字节
 */
#define UART_IER 0x01
#define UART_DLM 0x01

/* 0x02:
 *  - 读：IIR (Interrupt Identification Register) —— 中断标识
 *  - 写：FCR (FIFO Control Register) —— FIFO 控制
 */
#define UART_IIR_FCR 0x02

/* 0x03: LCR (Line Control Register) —— 数据位/停止位/奇偶校验/设置 DLAB */
#define UART_LCR 0x03

/* 0x04: MCR (Modem Control Register) —— 调制解调器控制（一般不用） */
#define UART_MCR 0x04

/* 0x05: LSR (Line Status Register) —— 发送/接收状态 */
#define UART_LSR 0x05

/* 0x06: MSR (Modem Status Register) —— 调制解调器状态（一般不用） */
#define UART_MSR 0x06

/* 0x07: SCR (Scratch Register) —— 临时存储寄存器（软件可自由使用） */
#define UART_SCR 0x07

/* -------- LSR (Line Status Register) 位定义 -------- */

/* LSR[5] = 1: THR 空，可以写入新的数据（发送缓冲区空） */
#define LSR_THRE (1 << 5)

/* LSR[0] = 1: 接收缓冲区有数据可读 */
#define LSR_DR (1 << 0)

/* -------- 函数接口 -------- */

/* 初始化 UART (设置波特率、数据位等) */
void uart_init(void);

/* 发送一个字符 */
void uart_putc(char c);

/* 发送字符串（以 '\0' 结尾） */
void uart_puts(const char *s);

/* 阻塞读取一个字符（如果没数据会一直等待） */
int uart_getc(void);

/* 非阻塞读取一个字符（如果没数据返回 -1） */
int uart_getc_nonblock(void);

#endif /* UART_H */
