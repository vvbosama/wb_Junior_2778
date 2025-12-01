#include "types.h"
#include "console.h"

// 添加函数声明（如果console.h中没有的话）
extern void console_putc(char c);
extern void console_puts(const char *s);

extern void uart_init(void);
extern void uart_putc(char c);
extern void uart_puts(char *s);

// 控制台初始化
void console_init(void) {
    uart_init();
}

// 输出一个字符到控制台
void console_putc(char c) {
    uart_putc(c);
}

// 输出字符串到控制台
void console_puts(const char *s) {
    // 需要将const char*转换为char*以兼容现有uart_puts
    uart_puts((char*)s);
}

// 使用ANSI转义序列清屏
void clear_screen(void) {
    console_puts("\033[2J");    // 清除整个屏幕
    console_puts("\033[H");     // 光标回到左上角
    console_puts("\033[3J");    // 清除滚动缓冲区（某些终端需要）
}

// 清除当前行
void clear_line(void) {
    console_puts("\033[2K");    // 清除整行
    console_puts("\r");         // 光标回到行首
}

// // 光标定位到指定位置
// void goto_xy(int x, int y) {
//     char buf[16];
//     // 格式: \033[y;xH (注意：行号在前，列号在后)
//     console_puts("\033[");
    
//     // 输出行号
//     int i = 0;
//     int temp = y;
//     do {
//         buf[i++] = '0' + (temp % 10);
//         temp /= 10;
//     } while (temp != 0);
//     while (--i >= 0) {
//         console_putc(buf[i]);
//     }
    
//     console_putc(';');
    
//     // 输出列号
//     i = 0;
//     temp = x;
//     do {
//         buf[i++] = '0' + (temp % 10);
//         temp /= 10;
//     } while (temp != 0);
//     while (--i >= 0) {
//         console_putc(buf[i]);
//     }
    
//     console_puts("H");
// }

// 在 console.c 中添加输出同步函数
void console_flush(void) {
    // 简单的延迟同步
    for (volatile int i = 0; i < 1000; i++);
}

// 修改 goto_xy 函数，x是列坐标，y是行坐标
void goto_xy(int x, int y) {
    char buf[16];
    
    // 参数验证
    if (x < 1) x = 1;
    if (y < 1) y = 1;
    
    // 先刷新之前的输出
    console_flush();
    
    // 格式: \033[y;xH
    console_puts("\033[");
    
    // 输出行号
    int i = 0;
    int temp = y;
    do {
        //'0' + (temp % 10)：将数字转换为对应的ASCII字符，例如：数字5 → '0' + 5 = '5'
        buf[i++] = '0' + (temp % 10);//逆序存储
        temp /= 10;
    } while (temp != 0);
    //将逆序变成正序输出
    while (--i >= 0) {
        console_putc(buf[i]);
    }
    
    console_putc(';');
    
    // 输出列号
    i = 0;
    temp = x;
    do {
        buf[i++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp != 0);
    while (--i >= 0) {
        console_putc(buf[i]);
    }
    
    console_puts("H");
    
    // 等待定位完成
    console_flush();
}

// 设置文本颜色
void set_color(int color) {
    char buf[8];
    console_puts("\033[");
    
    // 将颜色代码转换为字符串
    int i = 0;
    int temp = color;
    
    // 处理0（重置）的特殊情况
    if (temp == 0) {
        console_putc('0');
    } else {
        do {
            buf[i++] = '0' + (temp % 10);
            temp /= 10;
        } while (temp != 0);
        while (--i >= 0) {
            console_putc(buf[i]);
        }
    }
    
    console_putc('m');
}