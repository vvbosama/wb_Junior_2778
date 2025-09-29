# RISC-V 裸机内核项目 README

## 1. 项目概述
本项目是一款基于 **RISC-V 64位架构** 的裸机（Bare-metal）内核程序，无需依赖任何操作系统或固件（如 OpenSBI），可直接在 QEMU 模拟器的 `virt` 虚拟平台上运行。项目包含 RISC-V 裸机开发核心能力，如内核启动流程、UART 串口通信、控制台输出、格式化打印等.


## 2. 项目目录结构
| 文件/目录路径       | 核心功能说明                                                                 |
|---------------------|------------------------------------------------------------------------------|
| `kernel/`           | 内核核心代码目录，存放启动、驱动、链接相关文件                               |
| ├─ `kernel/entry.S` | 汇编启动文件：定义内核入口 `_start`，初始化栈指针，最终跳转至 `kernel_main`  |
| ├─ `kernel/main.c`  | 内核主程序：负责硬件（如 UART）初始化，调用测试逻辑与核心功能                 |
| ├─ `kernel/uart.c`  | UART 驱动文件：实现串口初始化、单字符发送/接收，是控制台的底层依赖           |
| ├─ `kernel/console.c` | 控制台封装：基于 UART 提供字符打印、字符串输出等高层接口                     |
| ├─ `kernel/printf.c` | 格式化打印库：实现标准 `printf` 函数，支持 `%d`/`%s`/`%x` 等格式符           |
| ├─ `kernel/linker.ld` | 链接脚本：定义内核内存布局（代码段、数据段、栈地址、段边界等关键信息）       |
| `test.c`            | 测试程序文件：包含内核功能验证逻辑（如打印测试、硬件初始化结果检查）         |
| `include/`          | 头文件目录：存放所有模块的头文件（如 `uart.h`/`printf.h`/`console.h`）        |
| `Makefile`          | 项目构建脚本：集成编译、链接、运行、调试、清理等全流程命令                   |


## 3. 环境依赖与安装
需提前安装 **RISC-V 交叉编译工具链** 和 **QEMU 模拟器**，确保支持 64位 RISC-V 程序的编译与运行。

### 3.1 RISC-V 交叉编译工具链
用于将源代码编译为 RISC-V 架构可执行文件，推荐工具链：`riscv64-unknown-elf-gcc`（版本 10.2+）。

#### 安装方式（Ubuntu/Debian 系统）
```bash
# 更新软件源并安装工具链
sudo apt update && sudo apt install -y riscv64-unknown-elf-gcc riscv64-unknown-elf-binutils
```

#### 安装验证
```bash
# 查看工具链版本，确认安装成功
riscv64-unknown-elf-gcc --version
```
成功输出示例：`riscv64-unknown-elf-gcc (GCC) 10.2.0`


### 3.2 QEMU 模拟器（RISC-V 版）
用于模拟 RISC-V 64位硬件平台，运行裸机内核镜像，推荐版本：QEMU 6.0+。

#### 安装方式（Ubuntu/Debian 系统）
```bash
# 安装 RISC-V 架构的 QEMU 模拟器
sudo apt install -y qemu-system-riscv64
```

#### 安装验证
```bash
# 查看 QEMU 版本，确认安装成功
qemu-system-riscv64 --version
```
成功输出示例：`QEMU emulator version 6.2.0 (Debian 1:6.2+dfsg-2ubuntu6.17)`


## 4. 快速使用指南
### 4.1 编译内核
1. 进入项目根目录（假设项目目录为 `riscv-os`）：
   ```bash
   cd riscv-baremetal-kernel
   ```
2. 执行 `make` 命令编译项目，默认生成内核镜像 `kernel.elf`：
   ```bash
   # 等价于 make all，编译所有源文件并链接
   make
   ```

#### 编译流程说明
1. 先将 `.c` 源文件（如 `main.c`/`uart.c`）编译为 `.o` 目标文件；
2. 再将 `.S` 汇编文件（如 `entry.S`）编译为 `.o` 目标文件；
3. 最后通过 `kernel/linker.ld` 链接所有 `.o` 文件，生成最终可执行内核 `kernel.elf`。

#### 编译成功标志
项目根目录下生成 `kernel.elf` 文件，且终端无报错信息。


### 4.2 运行内核
通过 QEMU 模拟器启动裸机内核，命令如下：
```bash
make run
```

#### 运行成功标志
终端输出内核初始化信息（如 UART 初始化成功、测试打印内容），示例：
```
Running bare-metal kernel (no OpenSBI) ...
qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf
Hello OS
```

#### 退出 QEMU
按下 `Ctrl + A` 后松开，再按下 `x` 即可退出 QEMU 模拟器。


