// kernel/printf.c - 修复版本
#include "types.h"
#include "printf.h"
#include "console.h"
#include <stdarg.h>

static char digits[] = "0123456789abcdef";

// 支持64位数字的打印函数
static void print_number(uint64_t num, int base, int sign) {
    char buf[64];
    int i = 0;
    uint64_t x = num;

    if (sign && (int64_t)num < 0) {
        x = -(int64_t)num;
    }

    do {
        buf[i++] = digits[x % base];
    } while((x /= base) != 0);

    if (sign && (int64_t)num < 0) {
        buf[i++] = '-';
    }

    while(--i >= 0) {
        console_putc(buf[i]);
    }
}

// 主printf函数
int printf(const char *fmt, ...) {
    va_list ap;
    int i;
    char c;
    char *s;
    
    va_start(ap, fmt);
    
    for(i = 0; (c = fmt[i]) != '\0'; i++) {
        if(c != '%') {
            console_putc(c);
            continue;
        }
        
        i++; // 跳过'%'
        
        if(fmt[i] == '\0') {
            console_putc('%');
            break;
        }
        
        switch(fmt[i]) {
            case 'd': // 有符号十进制
                print_number(va_arg(ap, int), 10, 1);
                break;
                
            case 'u': // 无符号十进制
                print_number(va_arg(ap, unsigned int), 10, 0);
                break;
                
            case 'x': // 十六进制
                print_number(va_arg(ap, unsigned int), 16, 0);
                break;
                
            case 'l': // 长整型
                i++;
                if (fmt[i] == 'l') {
                    i++;
                    if (fmt[i] == 'u') {
                        // %llu
                        print_number(va_arg(ap, uint64_t), 10, 0);
                    } else if (fmt[i] == 'x') {
                        // %llx
                        print_number(va_arg(ap, uint64_t), 16, 0);
                    } else if (fmt[i] == 'd') {
                        // %lld
                        print_number(va_arg(ap, int64_t), 10, 1);
                    } else {
                        console_putc('%');
                        console_putc('l');
                        console_putc('l');
                        console_putc(fmt[i]);
                    }
                } else {
                    // 处理 %lx 和 %ld
                    if (fmt[i] == 'x') {
                        // %lx - 长十六进制
                        print_number(va_arg(ap, unsigned long), 16, 0);
                    } else if (fmt[i] == 'd') {
                        // %ld - 长十进制
                        print_number(va_arg(ap, long), 10, 1);
                    } else if (fmt[i] == 'u') {
                        // %lu - 长无符号十进制
                        print_number(va_arg(ap, unsigned long), 10, 0);
                    } else {
                        console_putc('%');
                        console_putc('l');
                        console_putc(fmt[i]);
                    }
                }
                break;
                
            case 'p': // 指针
                console_putc('0');
                console_putc('x');
                print_number(va_arg(ap, uint64_t), 16, 0);
                break;
                
            case 'c': // 字符
                console_putc((char)va_arg(ap, int));
                break;
                
            case 's': // 字符串
                s = va_arg(ap, char*);
                if(s == NULL) {
                    console_puts("(null)");
                } else {
                    console_puts(s);
                }
                break;
                
            case '%': // 字面量%
                console_putc('%');
                break;
                
            default: // 未知格式符
                console_putc('%');
                console_putc(fmt[i]);
                break;
        }
    }
    
    va_end(ap);
    return 0;
}