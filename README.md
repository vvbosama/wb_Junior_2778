简介

这是一个基于 RISC-V 架构 的极简内核实验项目。
项目特点：

使用 riscv64-unknown-elf-gcc 工具链交叉编译

目标平台：qemu-system-riscv64 (virt 机器，裸机模式)

提供 UART 驱动、控制台输出、简单 printf 实现

使用自定义链接脚本，手动控制内核内存布局
