# 实验八：优先级调度与多级反馈队列

## 一、实验目的

1. 解析 xv6 风格调度框架，理解 `struct proc`、`scheduler()`、`yield()` 等组件的协作方式
2. 在轮转调度基础上引入静态优先级字段与 aging 机制，提供抢占式调度策略
3. 设计多级反馈队列（MLFQ）运行队列，按层级分配不同时片并与优先级联动
4. 扩展系统调用与用户工具（`setpriority/getpriority`、`nice`），支持运行时调整进程权重
5. 构建内核测试矩阵（`priority_test.c`）验证高低优先级顺序、公平性与饥饿防护

## 二、实验环境

- 架构：RISC-V RV64，QEMU `virt` 单核配置
- 工具链：`riscv64-unknown-elf-gcc`、`ld`、`objdump`
- 运行模式：内核独立启动，`kernel/main.c` 直接调用调度测试入口
- 目录结构：`kernel/`（调度/测试源码）、`include/`（公共头）、`user/`（用户命令 `nice`）

## 三、实验原理

### 3.1 进程结构扩展

`include/proc.h` 在原有字段基础上新增：
- `priority`（0~10，默认 5，值越大优先级越高）
- `ticks`（累计运行次数）、`wait_time`（等待时长）、`queue_level`/`queue_ticks`（MLFQ 层级与已用时间片）
- `trap_context` 指向保存在 `trap_entry.S` 的寄存器快照，供系统调用参数解析使用

### 3.2 调度主循环

`proc.c` 中：
- `scheduler_loop()` 无限调用 `scheduler()` 并在空闲时执行 `wfi`
- `scheduler()`：
  1. 使能中断，持有 `proc_lock`
  2. 调用 `age_runnable_processes()` 为 RUNNABLE/SLEEPING 进程递增 `wait_time`
  3. 通过 `select_highest_priority()` 选取最优进程（比较 `priority`，再比较 `wait_time`、`ticks`、`pid`）
  4. 重置统计信息，切换上下文；若无 RUNNABLE 进程则输出僵尸进程信息
- `yield()`：增加当前进程的 `queue_ticks`，超过 `mlfq_time_slices[]{1,2,4}` 即调用 `mlfq_demote()`，随后回到调度器

### 3.3 Aging 与多级反馈队列

- `AGING_THRESHOLD` 设为 10：`age_runnable_processes()` 中 `wait_time >= 10` 时调用 `mlfq_promote()` 并清空计数
- MLFQ 共有 3 层：
  - 层 0：最高优先级（映射为 `PRIORITY_MAX`），时间片 1
  - 层 1：默认优先级（5），时间片 2
  - 层 2：最低优先级（`PRIORITY_MIN+2`），时间片 4
- `mlfq_apply_level()` 根据 `queue_level` 写回 `priority`，`set_priority()` 会同步更新层级，使手动调整与 MLFQ 兼容

### 3.4 系统调用与用户接口

- `include/syscall.h` 新增 `SYS_setpriority/SYS_getpriority`
- `sysproc.c` 实现 `sys_setpriority()` / `sys_getpriority()`，直接调用 `proc_set_priority()/proc_get_priority()`
- `user/` 下的 `nice.c` 解析命令行参数并调用 `setpriority(pid, value)`
- `priority.h/.c` 暴露 `priority_init()`、`set_nice()`、测试任务（`high_priority_task` 等）与调度信息打印器

## 四、实验内容与实现

### 4.1 关键模块

| 文件 | 核心功能 |
| --- | --- |
| `kernel/proc.c` | 进程表管理、`scheduler()`、MLFQ / aging / 优先级维护、`proc_set_priority` |
| `kernel/priority.c` | 调度接口封装、`set_nice()`、示例任务 `HIGH/MED/LOW/CPU`、`show_priority_info()` |
| `kernel/priority_test.c` | 集成测试：高低优先级顺序、同级公平性、aging 防饥饿 |
| `kernel/sysproc.c` & `include/syscall.h/user.h/usys.S` | 新增 `setpriority/getpriority` 系统调用与用户声明 |
| `user/nice.c` | 简化版 `nice` 命令，便于从用户态调整优先级 |
| `kernel/main.c` | 入口仅运行 `run_priority_scheduling_tests()` 并进入 `wfi` 循环 |

### 4.2 测试框架

`run_priority_scheduling_tests()` 依次执行：
1. **T1 高低优先级**：创建两个任务，设置不同优先级并检查完成顺序
2. **T2 同级公平性**：三个相同优先级的轮转任务，比较各自 `ticks` 与完成顺序
3. **T3 Aging 防饥饿**：构造一个高优先级“霸占”任务 + 两个低优先级任务，观察等待超过阈值后是否被提升

辅助工具：
- `drive_scheduler_until_idle()` 重复调用 `scheduler()` 直到无 RUNNABLE 进程
- `cleanup_all_processes()`/`collect_completion()` 统一收集并回收僵尸进程

## 五、实验结果与分析

运行 `run_priority_scheduling_tests()` 的典型日志：
```
=== PRIORITY SCHEDULER TESTS START ===
[T1] 高低优先级对比...
  T1 completion order: 3 4
  [PASS] 高优先级进程 3 最先完成
[T2] 相同优先级任务公平性...
  调度次数 ticks: 4 4 5
  [PASS] 调度次数接近，表现与 RR 类似
[T3] Aging 防止饥饿...
  初始状态: low1 priority=0 level=2 ...
  Aging后状态: low1 priority=10 level=0, low2 priority=10 level=0
  [PASS] Aging 防止饥饿机制生效
=== PRIORITY SCHEDULER TESTS END ===
```

- **T1** 验证抢占：高优先级任务在 `select_highest_priority()` 中被优先选中
- **T2** `ticks` 差值 ≤ 1，说明相同优先级仍近似 RR
- **T3** 低优先级进程在 `wait_time >= 10` 后被 `mlfq_promote()` 提升至高层，最终与高优先级任务一起完成

## 六、思考题解答

1. **为何结合静态优先级与 MLFQ？** 静态值便于 `setpriority/nice` 显式控制，MLFQ 根据运行行为自动升降级，兼顾可控与自适应。
2. **aging 如何实现？** `age_runnable_processes()` 为每个 RUNNABLE/SLEEPING 进程递增 `wait_time`，超过阈值即调用 `mlfq_promote()` 并重置等待时间。
3. **时间片如何分配？** `mlfq_time_slices[]{1,2,4}` 与 `queue_level` 对应，`yield()` 或耗尽时间片后 `curr_proc->queue_ticks` 触发降级。
4. **系统调用与用户态的协同？** `sys_setpriority()` 直接更新 `proc` 结构并同步队列信息，`user/nice.c` 通过 `usys.S` 生成的桩在用户态调用。
5. **如何观察调度状态？** `show_priority_info()` 在遍历 `proc[]` 时打印 `priority/queue_level/ticks/wait_time`，便于调试。

## 七、实验总结

本实验在 xv6 核心调度器上完成了以下增强：
- 扩展 `struct proc`，实现静态优先级、wait-tick 统计与 MLFQ 元信息
- 设计 `scheduler()` + aging + MLFQ 的调度链路，实现抢占、公平性与防饥饿策略
- 新增 `setpriority/getpriority` 系统调用及用户态 `nice` 工具，支持运行时调参与测试
- 提供系统化测试覆盖 T1/T2/T3 场景，确保高优先级响应、轮转公平与 aging 有效

通过该实验，进一步理解了优先级调度设计、策略调优与验证流程，为后续实现更复杂的多级反馈队列或实时调度奠定基础。
