OUTPUT_ARCH("riscv")
ENTRY(_entry)

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text .text.*)
    PROVIDE(etext = .);
  }

  .rodata : {
    *(.rodata .rodata.*)
    *(.srodata .srodata.*)
  }

  .data : {
    *(.data .data.*)
    *(.sdata .sdata.*)
  }

  .bss : {
    . = ALIGN(16);
    _bss_start = .;
    *(.bss .bss.*)
    *(.sbss .sbss.*)
    . = ALIGN(16);
    _bss_end = .;
  }

  PROVIDE(end = .);
  PROVIDE(kernel_base = 0x80000000);  /* 添加这行 */

  .stack : {
    . = ALIGN(16);
    stack0 = .;
    . += 4096;
  }
}