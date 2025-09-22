    .section .text
    .align  2
    .globl  _start
    .type   _start, @function

_start:
   /* 早期硬件验证：输出 'S' 到 UART 基址 0x10000000 */
    li      t6, 0x10000000
    li      t0, 'S'
    sb      t0, 0(t6)

    /* 设置栈指针（由 linker 提供 stack_top） */
    la      sp, stack_top

    /* 栈设置完成后输出 'P' 表示栈已设置 */
    li      t0, 'P'
    sb      t0, 0(t6) 
    li      t0, '\n'
    sb      t0, 0(t6) 

    /* 清零 BSS（从 __edata 到 __end）——每次写 8 字节 */
    la      t0, __edata
    la      t1, __end
1:  beq     t0, t1, 2f
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       1b
2:

    /* 跳转到 C 入口（kernel_main），不会返回 */
    call    kernel_main

    /* 如果 kernel_main 返回，停在此处 */
hang:
    j hang

    .size _start, .-_start
