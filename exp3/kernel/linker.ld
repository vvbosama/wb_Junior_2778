OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
    /* 起始地址，和 QEMU -kernel 保持一致 */
    . = 0x80000000;

    /* 文本段 */
    .text : {
        *(.text._start)       /* 汇编入口 */
        *(.text*)             /* 其他文本 */
        . = ALIGN(0x1000);
        PROVIDE(etext = .);   /* 数据段起点符号 */
    }

    /* 只读数据 */
    .rodata : {
        . = ALIGN(16);
        *(.rodata*)
    }

    /* 数据段 */
    .data : {
        . = ALIGN(16);
        *(.data*)
        PROVIDE(__edata = .);  /* 数据段结束，BSS 开始 */
    }

    /* BSS 段 */
    .bss : {
        . = ALIGN(16);
        PROVIDE(__bss_start = .);  /* 可选，BSS 起始符号 */
        *(.bss*)
        PROVIDE(__end = .);        /* BSS 结束 */
    }

    /* 一个固定的栈顶（可以按需调整） */
    PROVIDE(stack_top = 0x80010000);
}

