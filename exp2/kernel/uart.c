/* kernel/uart.c - Minimal 16550 UART driver for QEMU virt
 *
 * 作用：
 *   - 提供最小的串口初始化/收发能力，适用于裸机内核早期输出
 * 原理：
 *   - 16550 UART 通过 MMIO 暴露一组 8-bit 寄存器(基地址 + 偏移)
 *   - 发送：轮询 LSR.THRE (bit5) 直到发送保持寄存器空，再写 THR
 *   - 接收：检查 LSR.DR   (bit0) 是否为 1，若为 1 则从 RBR 读数据
 *
 * 依赖：
 *   - 见 kernel/uart.h 中的寄存器偏移与位定义（UART0_BASE / UART_* / LSR_*）
 */

#include "uart.h"
#include <stdint.h>
#include <stddef.h>

/* ---- MMIO 读写助手：对 8-bit 寄存器进行访问 ----
 * 使用 volatile 保证访问不会被编译器优化掉，并按次序进行实际的总线读写。
 * 使用 uintptr_t 以适配 32/64 位地址宽度（本工程为 RV64，但写法更通用）。
 */
static inline void mmio_write8(uintptr_t addr, uint8_t v)
{
    *(volatile uint8_t *)addr = v;
}
static inline uint8_t mmio_read8(uintptr_t addr)
{
    return *(volatile uint8_t *)addr;
}

/* uart_init()
 * 在 QEMU virt 上，16550 默认已是可用配置（典型为 115200/8N1），
 * 因此这里保持“最小初始化”：读一次 LSR 以“探测”设备并清除潜在状态。
 *
 * 如移植到真实硬件，可在此：
 *   - 通过设置 LCR 的 DLAB=1，配置 DLL/DLM 设定波特率；
 *   - 设置 LCR（数据位/停止位/奇偶校验，常用 8N1）；
 *   - 写 FCR 使能并清空 FIFO。
 */
void uart_init(void)
{
    (void)mmio_read8(UART0_BASE + UART_LSR);
}

/* uart_putc(c)
 * 发送单个字符：
 *   1) 轮询等待 LSR.THRE=1（发送保持寄存器空）；
 *   2) 写 THR（与 RBR 共享偏移 0x00）。
 *
 * 说明：这是最简单的“轮询式”（polling）发送实现，适合早期启动阶段。
 */
void uart_putc(char c)
{
    /* 等到 THR 空再写，避免覆盖未发完的数据 */
    while ((mmio_read8(UART0_BASE + UART_LSR) & LSR_THRE) == 0)
    {
        /* busy wait（自旋） */
    }
    mmio_write8(UART0_BASE + UART_RBR_THR, (uint8_t)c);
}

/* uart_puts(s)
 * 发送以 '\0' 结尾的字符串。
 * 兼容性处理：多数终端习惯 CRLF，因此遇到 '\n' 时先输出 '\r'。
 */
void uart_puts(const char *s)
{
    if (!s)
        return;
    while (*s)
    {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

/* uart_getc_nonblock()
 * 非阻塞读取一个字节：
 *   - 无数据：LSR.DR=0，返回 -1；
 *   - 有数据：从 RBR 读取并返回 0..255 的 int。
 */
int uart_getc_nonblock(void)
{
    if ((mmio_read8(UART0_BASE + UART_LSR) & LSR_DR) == 0)
        return -1;
    return (int)mmio_read8(UART0_BASE + UART_RBR_THR);
}

/* uart_getc()
 * 阻塞读取一个字节：
 *   - 持续轮询 uart_getc_nonblock() 直到读到数据；
 *   - 返回 0..255 的 int（用 & 0xFF 避免符号扩展）。
 */
int uart_getc(void)
{
    int ch;
    while ((ch = uart_getc_nonblock()) < 0)
    {
        /* busy wait */
    }
    return ch & 0xFF;
}
