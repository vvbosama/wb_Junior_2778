#pragma once

// console.c
void console_init(void);
void console_putc(char c);
void console_puts(const char *s);

/* ANSI 控制 */
void clear_screen(void);        /* \033[2J\033[H */
void goto_xy(int col, int row); /* \033[{row};{col}H */
