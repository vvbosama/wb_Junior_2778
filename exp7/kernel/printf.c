// kernel/printf.c - 修复版本
#include "types.h"
#include "printf.h"
#include "console.h"
#include <stdarg.h>

static char digits[] = "0123456789abcdef";

// 简单防刷屏：限制总打印配额与重复行折叠
static int log_budget = 200000;           // 本次运行的总打印配额（近似按字符计）
static int flood_notified = 0;            // 已提示静音
static char last_line[128];               // 上一条消息的前缀
static int last_len = 0;                  // 上一条消息长度（截断）
static unsigned int repeat_count = 0;     // 连续重复次数

// 支持宽度和填充的数字打印函数
static void print_number(uint64_t num, int base, int sign, int width, int zero_pad) {
    char buf[64];
    int i = 0;
    uint64_t x = num;
    int digits_count = 0;

    if (sign && (int64_t)num < 0) {
        x = -(int64_t)num;
    }

    // 转换数字为字符串
    do {
        buf[i++] = digits[x % base];
        digits_count++;
    } while((x /= base) != 0);

    if (sign && (int64_t)num < 0) {
        buf[i++] = '-';
        digits_count++;
    }

    // 计算需要的前导字符数量
    int padding = width - digits_count;
    if (padding < 0) padding = 0;

    // 输出前导字符
    if (zero_pad) {
        for (int j = 0; j < padding; j++) {
            console_putc('0');
            if (log_budget > 0) log_budget--;
        }
    } else {
        for (int j = 0; j < padding; j++) {
            console_putc(' ');
            if (log_budget > 0) log_budget--;
        }
    }

    // 输出数字
    while(--i >= 0) {
        console_putc(buf[i]);
        if (log_budget > 0) log_budget--;
    }
}

// 主printf函数
int printf(const char *fmt, ...) {
     va_list ap;
    int i;
    char c;
    char *s;

    if (log_budget <= 0) {
        if (!flood_notified) {
            flood_notified = 1;
            console_puts("\n[LOG SUPPRESSED] too much output; further logs muted.\n");
        }
        return 0;
    }

    va_start(ap, fmt);

    for(i = 0; (c = fmt[i]) != '\0'; i++) {
        if(c != '%') {
            console_putc(c);
            if (log_budget > 0) log_budget--;
            continue;
        }

        i++; // 跳过'%'

        if(fmt[i] == '\0') {
            console_putc('%');
            if (log_budget > 0) log_budget--;
            break;
        }

        // 解析格式化标志
        int zero_pad = 0;
        int width = 0;
        
        // 检查前导零
        if (fmt[i] == '0') {
            zero_pad = 1;
            i++;
        }
        
        // 解析宽度
        while (fmt[i] >= '0' && fmt[i] <= '9') {
            width = width * 10 + (fmt[i] - '0');
            i++;
        }
        
        if(fmt[i] == '\0') {
            // 格式字符串意外结束
            console_putc('%');
            if (log_budget > 0) log_budget--;
            if (zero_pad) console_putc('0');
            // 输出宽度数字...
            break;
        }

        switch(fmt[i]) {
            case 'd': // 有符号十进制
                print_number(va_arg(ap, int), 10, 1, width, zero_pad);
                break;

            case 'u': // 无符号十进制
                print_number(va_arg(ap, unsigned int), 10, 0, width, zero_pad);
                break;

            case 'x': // 十六进制
            case 'X': // 大写十六进制
                print_number(va_arg(ap, unsigned int), 16, 0, width, zero_pad);
                break;

            case 'l': // 长整型
                i++;
                if (fmt[i] == 'l') {
                    i++;
                    if (fmt[i] == 'u') {
                        // %llu
                        print_number(va_arg(ap, uint64_t), 10, 0, width, zero_pad);
                    } else if (fmt[i] == 'x' || fmt[i] == 'X') {
                        // %llx
                        print_number(va_arg(ap, uint64_t), 16, 0, width, zero_pad);
                    } else if (fmt[i] == 'd') {
                        // %lld
                        print_number(va_arg(ap, int64_t), 10, 1, width, zero_pad);
                    } else {
                        // 回退到简单输出
                        console_putc('%'); if (log_budget > 0) log_budget--;
                        console_putc('l'); if (log_budget > 0) log_budget--;
                        console_putc('l'); if (log_budget > 0) log_budget--;
                        console_putc(fmt[i]); if (log_budget > 0) log_budget--;
                    }
                } else {
                    // 处理 %lx 和 %ld
                    if (fmt[i] == 'x' || fmt[i] == 'X') {
                        // %lx - 长十六进制
                        print_number(va_arg(ap, unsigned long), 16, 0, width, zero_pad);
                    } else if (fmt[i] == 'd') {
                        // %ld - 长十进制
                        print_number(va_arg(ap, long), 10, 1, width, zero_pad);
                    } else if (fmt[i] == 'u') {
                        // %lu - 长无符号十进制
                        print_number(va_arg(ap, unsigned long), 10, 0, width, zero_pad);
                    } else {
                        console_putc('%'); if (log_budget > 0) log_budget--;
                        console_putc('l'); if (log_budget > 0) log_budget--;
                        console_putc(fmt[i]); if (log_budget > 0) log_budget--;
                    }
                }
                break;

            case 'p': // 指针
                console_putc('0'); if (log_budget > 0) log_budget--;
                console_putc('x'); if (log_budget > 0) log_budget--;
                print_number(va_arg(ap, uint64_t), 16, 0, width, zero_pad);
                break;

            case 'c': // 字符
                console_putc((char)va_arg(ap, int));
                if (log_budget > 0) log_budget--;
                break;

            case 's': // 字符串
                s = va_arg(ap, char*);
                if(s == NULL) {
                    console_puts("(null)");
                } else {
                    // 简单的字符串宽度处理
                    int len = 0;
                    char *p = s;
                    while (*p++) len++;
                    
                    if (width > len) {
                        for (int j = 0; j < width - len; j++) {
                            console_putc(' ');
                            if (log_budget > 0) log_budget--;
                        }
                    }
                    
                    console_puts(s);
                }
                break;

            case '%': // 字面量%
                console_putc('%'); if (log_budget > 0) log_budget--;
                break;

            default: // 未知格式符
                console_putc('%'); if (log_budget > 0) log_budget--;
                console_putc(fmt[i]); if (log_budget > 0) log_budget--;
                break;
        }
    }

    va_end(ap);
    // 重复行抑制：仅取格式串前缀用于识别（不改变已输出内容）
    int cap = 0;
    while (fmt[cap] && fmt[cap] != '\n' && cap < (int)sizeof(last_line)) cap++;
    if (cap > 0) {
        int same = (cap == last_len);
        if (same) {
            for (int k = 0; k < cap; k++) {
                if (last_line[k] != fmt[k]) { same = 0; break; }
            }
        }
        if (same) {
            repeat_count++;
            if ((repeat_count % 1000) == 0 && log_budget > 0) {
                console_puts("\n[LOG] previous line repeated 1000x\n");
                log_budget -= 40;
            }
        } else {
            for (int k = 0; k < cap; k++) last_line[k] = fmt[k];
            last_len = cap;
            repeat_count = 0;
        }
    }
    return 0;
}