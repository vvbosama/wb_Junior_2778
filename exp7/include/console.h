#ifndef _CONSOLE_H_
#define _CONSOLE_H_

// 控制台初始化
void console_init(void);

// 输出函数
void console_putc(char c);
void console_puts(const char *s);

// 高级控制功能
void clear_screen(void);
void clear_line(void);
void goto_xy(int x, int y);
void set_color(int color);

#endif // _CONSOLE_H_