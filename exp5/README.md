# 实验五：进程管理与调度

## 一、实验目的

1. 设计符合 RISC-V 调用约定的 `struct proc`/`struct context`，支持完整的 RUNNABLE→RUNNING→SLEEPING→ZOMBIE 生命周期管理  
2. 在机器定时器中断驱动下完成 `yield()`、`sleep()/wakeup()` 等调度入口，实现可抢占的轮转调度  
3. 构建最小可用的 `create_process/exit_process/wait_process` 接口，演示孤儿回收与父子协作  
4. 在 `switch.S`/`trap_entry.S` 上实现可靠的上下文保存与恢复，保证调度器与用户任务之间的切换语义  
5. 拓展优先级调度框架，引入动态优先级、nice 值和多种工作负载测试，比较与轮转调度的差异

## 二、实验环境

- CPU：QEMU virt (RV64)  
- 工具链：`riscv64-unknown-elf-gcc`、`ld`、`objdump`  
- 必要设备：CLINT 定时器、UART16550 控制台  
- 主要代码目录：`kernel/`（调度/陷阱/测试）、`kernel/mm/`（页表与物理内存）、`include/`（公共头文件）

## 三、实验原理

### 3.1 进程抽象与生命周期

`include/proc.h` 定义了 32 个进程槽 (`NPROC=32`)、6 种状态以及内核栈、页表、父子关系等字段。生命周期遵循 `UNUSED → USED → RUNNABLE → RUNNING → (SLEEPING | RUNNABLE) → ZOMBIE → UNUSED`。  
`alloc_proc()` 负责分配内核栈、PID、命名并记录父进程；`exit_process()`/`wait_process()` 配对释放资源并解决孤儿进程。一个简单的自旋锁 (`proc_lock`) 确保进程表一致性。

### 3.2 上下文切换与陷阱入口

- `kernel/switch.S` 的 `context_switch()` 保存/恢复 `ra, sp, s0-s11` 等 callee-saved 寄存器，满足 RISC-V ABI。  
- `kernel/trap_entry.S` 在陷阱到来时推入 31 个通用寄存器 + `mepc/mstatus`，跳入 `trap_handler()`。返回前写回 CSR 并执行 `mret`。  
- `trap_handler()` 区分中断/异常：定时器中断 (`mcause=0x8000000000000007`) 时调用 `clock_set_next_event()` 并触发 `yield()`，实现抢占。

### 3.3 轮转调度器

`scheduler()`（`kernel/proc.c`）遍历进程表，找到 RUNNABLE 进程切换到 RUNNING 状态并调用 `context_switch()`。调度器自身使用 `scheduler_context` 保存状态，支持首次切换与进程退出后回切。  
`yield()` 负责主动让出 CPU；`sleep(void *chan)`/`wakeup(void *chan)` 为同步原语基础，配合自旋锁避免 lost wakeup。  
`clock_init()`/`clock_set_next_event()` 的机器定时器提供调度节拍。

### 3.4 优先级调度扩展

`kernel/priority.c` 在原有进程表外维护 `struct proc_priority`，引入静态/动态优先级、累计运行/等待时间与 nice 值。  
`priority_scheduler()` 使用“最高优先级优先”策略：每轮选择 `dynamic_priority` 最大的 RUNNABLE 进程执行，若被中断则重新入队。接口 `set_priority()/set_nice()` 允许实验不同策略，并提供多种任务（高/中/低优先级、CPU 密集型等）用于观察效果。

## 四、实验内容与实现

### 4.1 进程与上下文结构（`include/proc.h`）

- `enum procstate` 显式区分每种状态及其用途（ZOMBIE 让父进程查询退出码）。  
- `struct context` 仅保存 ABI 规定的 callee-saved 寄存器，减少切换开销。  
- `struct proc` 在最小内核中保留 `pagetable`、`kstack`、`parent`、`chan` 等关键字段，配合 `curr_proc`/`scheduler_context` 构成调度栈。

### 4.2 进程基本操作（`kernel/proc.c`）

1. **初始化**：`proc_init()` 清空进程槽，并为调度器上下文清零。  
2. **创建**：`create_process()` 通过 `alloc_proc()` 分配页框，设置 `context.ra=entry`、`context.sp=kstack_top`，标记为 RUNNABLE。  
3. **退出/回收**：`exit_process()` 将当前进程置为 ZOMBIE，唤醒父进程并切回调度器；`wait_process()` 遍历进程表查找子进程 ZOMBIE，回收内核栈并返回退出状态。  
4. **同步**：`sleep(chan)` 将进程挂在 channel 上并 `yield()`，`wakeup(chan)` 扫描链表唤醒匹配进程。

### 4.3 调度与中断联动（`kernel/trap.c`, `kernel/clock.c`）

- 中断路径：机器定时器中断 → `clock_set_next_event()` → `yield()` → `scheduler()`，实现基本抢占。  
- `clock.c` 复用实验四的多速率计时器逻辑，为调度器/性能测试提供 `get_ticks()`。

### 4.4 优先级框架与测试（`kernel/priority.c`, `kernel/priority_test.c`）

- `priority_init()` 将所有进程映射到 `priority_procs[]`，默认优先级 5。  
- `set_priority()`、`set_nice()` 随时调整调度顺序；`get_priority()`/`get_dynamic_priority()` 用于调试输出。  
- `run_priority_scheduling_tests()` 包含混合工作负载测试：`high_priority_task()`、`cpu_intensive_priority_task()`、`medium/low_priority_task()` 等展示不同优先级下的运行顺序。  
- `test_performance_comparison()`（可选）比较轮转与优先级调度的 tick 开销。

### 4.5 系统启动与功能验证（`kernel/main.c`）

1. 初始化 UART、时钟、陷阱、进程表并开启中断。  
2. 顺序执行以下测试：  
   - `test_process_creation()`：创建大量进程验证表容量与清理逻辑  
   - `test_scheduler()`：运行多个计算任务观测轮转调度  
   - `test_synchronization()`：构建 `producer_task()/consumer_task()`，验证 `sleep/wakeup`  
   - `run_priority_scheduling_tests()`：切换到优先级调度器，执行混合负载  
3. 所有测试结束后进入 `wfi` 循环等待进一步指令。

## 五、实验结果与分析

### 5.1 进程创建与回收

```
Testing process creation...
Created process with PID: 2
Created process 3
...
Failed to create process (table full at 32 processes)
Running all processes...
Process 5 exited with status 0
...
```

日志显示当达到 `NPROC` 上限时能够正确拒绝新建，并在 `wait_process()` 中逐个回收。

### 5.2 调度与同步测试

```
Testing scheduler...
Process 7: CPU intensive task started
Scheduler: switching to process 8
Process 8: progress 100000
...
Testing synchronization...
Process 11: producer started
Process 12: consumer started
Process 12: consumed item 0
```

轮转调度在多个 CPU 任务间轮替，Producer/Consumer 通过 `sleep(buffer_chan)` 协调，证明 `sleep/wakeup` 行为可靠。

### 5.3 优先级调度

```
🔀 STARTING PRIORITY SCHEDULING TESTS
Priority: initializing priority scheduler
Creating mixed workload...
Priority: selected process 15 for execution
🎯 HIGH PRIORITY Process 15: TASK STARTED
🐢 LOW PRIORITY Process 18: STARTED
Priority: all processes completed, returning from scheduler
✅ ALL PRIORITY SCHEDULING TESTS COMPLETED SUCCESSFULLY
```

高优先级任务率先运行并快速退出，低优先级任务在其后执行，证明调度策略生效。切换过程中日志展示 `context_switch()` 的 `old/new` 地址，便于调试。

## 六、思考题

1. **为何需要 ZOMBIE 状态？** 父进程必须读取退出码与释放资源；若立即销毁会导致 `wait_process()` 无法得知子进程结果。  
2. **PID 如何避免重复？** `next_pid` 单调递增（当前实现未回绕），更完善的方案可结合 PID 位图或 epoch，防止 wraparound 与 PID 重用间隔过短。  
3. **`fork()` 的瓶颈与优化？** 当前实验未实现用户空间 `fork()`，若扩展将面临整段内存复制的问题，可通过 Copy-on-Write、写时共享页等降低成本。  
4. **轮转调度公平性与实时性？** 轮转在任务时长相当时较公平，但对高优先级/实时需求不足；优先级调度展示了抢占式差异，可进一步引入多级反馈队列或 CFS。  
5. **资源限制与扩展性？** 32 个进程槽限制系统并发规模，可考虑链式 PCB、空闲列表或按需扩容。多核场景需为每个 CPU 维护 `struct cpu`、局部运行队列以及跨核唤醒逻辑。

## 七、实验总结

本实验构建了从陷阱入口、上下文切换到进程表管理、调度策略的完整链路。通过轮转调度和优先级调度两套实现，验证了抢占式调度、同步原语及进程生命周期管理。未来可继续：  
(1) 扩展 `fork/exec` 与用户页表、实现真正的用户态进程；  
(2) 在优先级框架上加入动态老化算法、负载统计和抢占限时；  
(3) 支持多核与 CPU 亲和性调度，完善 `sleep/wakeup` 的锁语义。通过该实验，可以清晰理解 xv6 风格进程管理的关键组件以及调度策略之间的权衡。
