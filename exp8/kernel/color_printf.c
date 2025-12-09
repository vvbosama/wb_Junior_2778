#include "types.h"
#include "console.h"
#include "printf.h"
#include "colors.h"  // 包含颜色定义
#include <stdarg.h>

// 带颜色的printf函数，color：文本颜色代码，fmt：格式字符串
void printf_color(int color, const char *fmt, ...) {
    va_list ap;//声明可变参数列表变量ap
    
    // 设置颜色
    set_color(color);
    
    // 解析并输出格式字符串
    va_start(ap, fmt);
    
    int i;
    char c;
    char *s;
    
    for(i = 0; (c = fmt[i]) != '\0'; i++) {
        //如果不是格式说明符%，直接输出字符并继续循环
        if(c != '%') {
            console_putc(c);
            continue;
        }
        
        i++; // 跳过'%'
        
        //检查格式字符串是否意外结束，如果是则输出%并退出
        if(fmt[i] == '\0') {
            console_putc('%');
            break;
        }
        
        switch(fmt[i]) {
            case 'd': { // 有符号十进制
                int num = va_arg(ap, int); //从可变参数列表中提取一个int类型的参数
                char buf[32];
                int j = 0;
                unsigned int x;
                int sign = 0;
                
                //处理负数
                if(num < 0) {
                    sign = 1;
                    x = -num;
                } else {
                    x = num;
                }
                
                //逆序存储
                do {
                    buf[j++] = '0' + (x % 10);
                    x /= 10;
                } while(x != 0);
                
                if(sign) {
                    buf[j++] = '-';
                }
                
                //正序输出
                while(--j >= 0) {
                    console_putc(buf[j]);
                }
                break;
            }
                
            case 's': // 字符串
                s = va_arg(ap, char*);
                if(s == NULL) {
                    console_puts("(null)");
                } else {
                    console_puts(s);
                }
                break;
                
            case 'c': // 字符
                //获取字符（在可变参数中char被提升为int）
                console_putc((char)va_arg(ap, int));
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
    
    // 重置颜色
    set_color(COLOR_RESET);
}