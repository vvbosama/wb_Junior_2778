# 实验四：中断处理与时钟管理

## 一、实验目的

1. 理解 RISC-V 中断与异常委托机制，熟悉 `mstatus`、`mie/mip`、`mtvec` 等关键 CSR 的配置方式  
2. 构建完整的陷阱入口框架，实现上下文保存、异常返回以及 C 语言处理函数之间的衔接  
3. 设计基于 CLINT 的多速率时钟管理器，驱动机器定时器中断并为调度/统计提供统一时间来源  
4. 完成异常分发与测试，能够处理 `ebreak`、`ecall`、非法指令与访存异常等常见场景  
5. 编写系统级测试用例，验证中断响应、定时器精度、异常语义以及 UART/交互式调试流程

## 二、实验环境

- CPU 架构：RISC-V RV64（QEMU virt）
- 模拟器：QEMU (machine virt, `-kernel` 方式加载)
- 工具链：GNU 工具链（`riscv64-unknown-elf-gcc`、`objdump`、`gdb`）
- 依赖组件：CLINT 定时器、UART16550 MMIO、最小引导环境
- 代码结构：`kernel/`（核心逻辑）、`include/`（公共头文件）、`kernel/mm/`（内存子系统）

## 三、实验原理

### 3.1 RISC-V 中断/异常架构

- `mstatus.MIE` 控制机器态全局中断开关，`mie.MTIE` 打开机器定时器中断  
- `mtvec` 保存陷阱向量入口地址，本实验使用直接模式指向 `trap_vector`（`kernel/trap_entry.S`）  
- `mcause` 第 63 位区分中断 (`1`) 与异常 (`0`)，低位编码不同事件来源；`mtval` 提供附加故障地址  
- 通过 `mret` 恢复 `mepc/mstatus`，保证陷阱返回后继续执行触发指令或下一条指令

### 3.2 陷阱入口与上下文管理

`kernel/trap_entry.S` 在陷阱发生时将 31 个通用寄存器、`sp`、`mepc`、`mstatus` 压栈，随后调用 C 端 `trap_handler`。  
上下文布局与 `struct trap_context`（`include/trap.h`）严格对应，便于在 C 代码中读写寄存器。返回前重新写回  
`mepc/mstatus`，恢复通用寄存器并执行 `mret`。该过程保证异常可重入，亦为后续调度器扩展奠定基础。

### 3.3 时钟管理与 CLINT

CLINT 提供 `mtime`（实时计数器）和每个 hart 的 `mtimecmp`。当 `mtime >= mtimecmp` 时触发机器定时器中断：  

```
static inline uint64_t read_mtime(void)  { return *(uint64_t*)0x200BFF8; }
static inline void write_mtimecmp(uint64_t v) { *(uint64_t*)0x2004000 = v; }
```

`kernel/clock.c` 实现三个不同节奏（0.1s/0.25s/0.5s）定时器：  
1. `clock_init()` 读取当前 `mtime` 并设置第一次触发时间  
2. `update_all_timers()` 依据当前 `mtime` 更新每个节奏的 tick 计数  
3. `calculate_next_event()` 选择最近一次触发时间，写入 `mtimecmp` 实现“多路复用”  
4. `clock_set_next_event()` 在中断中调用，确保下一个节拍准时发生

### 3.4 异常语义

`handle_exception()`（`kernel/exception.c`）根据信息寄存器解析异常来源：  
- `CAUSE_BREAKPOINT` / `CAUSE_MACHINE_ECALL`：通过 `ctx->mepc += 4` 跳过触发指令  
- `CAUSE_ILLEGAL_INSTRUCTION`：识别全零指令并跳过，其余情况挂起等待调试  
- `CAUSE_LOAD_ACCESS`：输出 `mtval` 给出故障地址，再次跳过以避免死循环  
配套提供 `test_exception_handling()`、`test_interrupt_overhead()` 两组测试用例，覆盖功能验证与性能量化。

## 四、实验内容与实现

### 4.1 陷阱初始化与寄存器配置

- `trap_init()`：写入 `mtvec`，使用直接模式指向 `trap_vector`，后续中断/异常均跳转至统一入口  
- `enable_interrupts()/disable_interrupts()`：通过 `csrs/csrc mstatus, MIE` 与 `csrs mie, MTIE` 控制中断开关  
- `trap_handler()`：读取 `mcause/mtval`，判断中断或异常；定时器中断 (`code=7`) 时调用 `clock_set_next_event()`，其他中断打印警告；异常路径则转发给 `handle_exception()` 并允许其调整 `ctx->mepc`

### 4.2 时钟管理器

- `clock_init()` 预热：对三个档位（FAST/MEDIUM/SLOW）输出频率信息，并设置首次触发  
- `clock_set_next_event()`：在每次 MTI 中断中被调用，完成 tick 累加及下一次比较值计算  
- `get_ticks()/reset_ticks()`：提供对外查询与复位接口，方便 UI 层实时展示及测试用例统计  
- 关键逻辑位于 `update_all_timers()` 与 `calculate_next_event()`，分别负责 tick 累积与多速率调度

### 4.3 异常处理与综合测试

`run_comprehensive_tests()` 在主函数初始化后执行：  
1. `test_exception_handling()` 依次触发 `ebreak`、`ecall`、非法指令以及访问未映射地址，验证 `mepc` 调整逻辑  
2. `test_interrupt_overhead()` 使用 `csrr time` 记录时间戳，重复设置定时器/模拟上下文切换，计算平均开销  
测试均通过 UART 控制台输出，方便定位异常和性能瓶颈。

### 4.4 系统启动流程与交互

1. `kernel/entry.S`：完成栈初始化、BSS 清零、调用 `pmm_init()` / `kvminit()` / `kvminithart()` 后进入 `main`  
2. `kernel/mm/`：提供页框管理与三级页表操作，保证陷阱/中断相关栈空间与代码段映射正确  
3. `kernel/main.c`：  
   - 初始化控制台与内存、设置陷阱、执行综合测试  
   - 进行 UART 回环测试，用于确认输入输出链路  
   - 打开机器定时器中断，调用 `clock_init()` 并进入主循环，周期性打印 FAST/MEDIUM/SLOW 三路 tick  
   - 支持交互命令：`q` 退出测试打印最终统计，`r` 重置所有 tick 计数  
4. `kernel/console.c` 与 `uart.c` 负责基础 I/O，配合 `printf`/`color_printf` 提供调试输出

## 五、实验结果与分析

### 5.1 陷阱初始化日志

```
Trap: mtvec set to 0x80001000 (direct mode)
Interrupts enabled
Clock: initialized with 3 timers
  Fast:   1000000 cycles (10 Hz)
  Medium: 2500000 cycles (4 Hz)
  Slow:   5000000 cycles (2 Hz)
```

日志表明 `mtvec`、`mie/mtie` 配置成功，且三档定时器频率与设计一致。

### 5.2 异常处理测试

`run_comprehensive_tests()` 输出示例：

```
=== Testing Exception Handling ===
1. Testing breakpoint exception...
Before ebreak
TRAP: cause=0x3, mepc=0x800020f4, mtval=0x0
EXCEPTION: 3 - breakpoint
Exception handled, new mepc=0x800020f8
After ebreak - Breakpoint handled!
...
Testing access to unmapped high memory...
TRAP: cause=0x5, mepc=0x80002270, mtval=0xffffffff00000000
LOAD ACCESS FAULT at address 0xffffffff00000000 - skipping instruction
```

可以看到每类异常均被识别并跳转回正确的 `mepc`，非法访存会报告 `mtval` 供调试使用。

### 5.3 三速定时器输出

主循环中每秒刷新一次计数，命令行示例：

```
=== Three Speed Timer Test ===
FAST: 23 ticks | MEDIUM: 9 ticks | SLOW: 4 ticks
FAST: 33 ticks | MEDIUM: 13 ticks | SLOW: 6 ticks
...
Input: 'r' (UART working, press 'q' to quit)
Counters reset!
...
=== TEST COMPLETED ===
Final counts:
  FAST:   81 ticks
  MEDIUM: 32 ticks
  SLOW:   16 ticks
```

Fast/Medium/Slow 的 tick 比例约为 10:4:2，符合 0.1s/0.25s/0.5s 的时间间隔设计。

## 六、思考题解答

1. **为何机器定时器在 M 模式触发却在 S 模式处理中断？**  
   因为 CLINT 只向机器态发出中断，但实际内核工作在 S 模式，需通过 `mideleg`/`medeleg` 将任务委托给 S 态，既保证安全又减少 M 态代码量。本实验处于 M 态直接处理，后续可扩展委托。

2. **如何区分异步中断与同步异常？**  
   `mcause[63]` 指示事件类型，异步中断不与当前指令绑定，可随时打断；同步异常由当前指令触发，需要结合 `mepc`、`mtval` 修复控制流。处理同步异常时一般修改 `mepc` 跳过或重试指令。

3. **上下文保存应关注哪些寄存器？**  
   RISC-V 调用约定区分 caller/callee 保存寄存器，但在陷阱中无法假设是谁触发，因此 `trap_entry.S` 选择保存所有通用寄存器 + `sp`、`mepc`、`mstatus`，以确保异常嵌套/调度场景的正确性。

4. **如何保证多速率定时器的实时性与可扩展性？**  
   通过统一 `mtime` 时间基准，使用最短到期时间驱动 `mtimecmp`，每次中断更新全局 tick，避免为每个节奏单独编队中断。新增节奏只需扩展 `timer_type_t` 和间隔数组即可。

5. **中断处理耗时过长的影响及优化？**  
   处理时间越长越容易导致定时器滞后或丢失输入事件。`test_interrupt_overhead()` 量化开销后，可通过减少打印、在 C 之前过滤无关中断、拆分耗时任务等方式优化；若存在更高实时性需求，可考虑在汇编层进行快速路径处理。

## 七、实验总结

本实验完成了从机器态陷阱入口到 C 端处理函数的完整闭环，实现了寄存器上下文保存、异常调度与机器定时器驱动的多速率时钟管理。同时通过综合测试验证了异常语义与中断性能，并结合 UART 提供交互控制。基于当前框架，可以继续向以下方向扩展：  
(1) 在 S 模式接管中断并接入调度器；(2) 增加更细粒度的中断优先级与嵌套控制；(3) 将计时器与任务调度耦合，进一步完善实验五的上下文切换功能。通过本次实验对中断处理链路的搭建，后续实现多任务调度具备了可靠的时间源与异常处理基础。***
