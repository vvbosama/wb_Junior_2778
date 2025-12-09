# 实验六：系统调用框架

## 一、实验目的

1. 理解 RISC-V 上下文切换流程及 trapframe 的作用，掌握 `ecall → trap → S 模式` 的状态机
2. 设计可扩展的系统调用分发表，完成参数提取、错误处理和用户内存校验
3. 实现一组实用的系统调用（进程管理、睡眠/计时、简化文件系统接口等），并串接调度器与文件子系统
4. 构建内核态的系统调用测试套件，覆盖功能正确性、参数边界、安全性与性能
5. 熟悉项目代码结构（`boot/`, `trap/`, `proc/`, `fs/`, `core/` 等目录），能够独立调试并扩展接口

## 二、实验环境

- 目标架构：RISC-V RV64（QEMU virt 单核）
- 工具链：`riscv64-unknown-elf-gcc`、`ld`、`objdump`
- 目录结构：
  - `kernel/boot/`：`entry.S` 完成 M→S 切换，`start.c` 初始化 C 运行时
  - `kernel/trap/`：`kernelvec.S`/`trampoline.S` 保存寄存器，`trap.c` 负责中断与系统调用分发
  - `kernel/proc/`：进程管理、调度与系统调用核心逻辑
  - `kernel/mm/`、`kernel/fs/`、`kernel/drivers/`：分别提供物理/虚拟内存、日志型文件系统、UART 等驱动
  - `kernel/core/`：测试入口 `main.c` 与 `test.c`
  - `include/`：统一声明（`defs.h`、`trap.h`、`syscall.h`、`riscv.h` 等）

## 三、实验原理

### 3.1 特权级切换与陷阱入口

`kernel/boot/entry.S` 关闭机器态中断，配置 `mstatus.MPP=S`、`medeleg/mideleg` 并设置 PMP，随后 `mret` 进入 S 模式。  
S 模式的 trap 向量由 `kernel/trap/kernelvec.S` 注册到 `stvec`，它会：
1. 在内核栈上保存 31 个通用寄存器（`struct pushregs`）
2. 调用 C 例程 `kerneltrap()` 处理异常/中断
3. 恢复寄存器并执行 `sret`

用户态 trap 使用 `kernel/trap/trampoline.S`：`uservec` 将用户寄存器保存到每进程的 `struct trapframe`，切换到内核页表后跳入 `usertrap`；`userret` 负责恢复并返回。`trapframe` 额外保留 `kernel_satp/kernel_sp/kernel_trap` 等字段以支持跨页表切换。

### 3.2 中断与系统调用路径

`kernel/trap/trap.c` 的核心流程：
```c
void kerneltrap(struct pushregs *regs) {
  uint64 scause = r_scause();
  if(scause & (1ULL << 63)) {
    dispatch_interrupt(scause & 0xff);
  } else {
    struct trapframe tf = {...};
    handle_exception(&tf, regs);
    w_sepc(tf.epc);
  }
}
```
- 计时器中断：`register_interrupt()` 将 `timer_interrupt_handler()` 注册到 `IRQ_S_TIMER`，它在 `TICK_INTERVAL` 周期更新 `ticks` 并唤醒依赖进程
- `handle_exception()` 根据 `tf->cause` 判断 `ecall`、页故障或非法指令；其中 `cause=8/9` 时调用 `handle_syscall()`，随后由 `advance_sepc()` 跳过触发指令

### 3.3 系统调用分发机制

`kernel/proc/syscall.c` 构建一个简洁的跳转表：
```c
static uint64 (*syscalls[])(void) = {
  [SYS_getpid] sys_getpid,
  [SYS_sleep]  sys_sleep,
  [SYS_open]   sys_open,
  [SYS_write]  sys_write,
  ...
};

void syscall(struct trapframe *tf, struct pushregs *regs) {
  current_regs = regs;
  int num = regs->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num])
    regs->a0 = syscalls[num]();
  else
    printf("unknown sys call %d\n", num);
  current_regs = 0;
}
```
寄存器约定：`a7` 存放系统调用号，`a0~a5` 用于传参。`argraw/argint/argaddr/argstr` 从 `current_regs` 取数；当存在进程页表时，字符串通过 `copyinstr()` 落地，否则直接读取内核地址（方便内核测试）。

### 3.4 用户内存访问与安全检查

- `fetchstr()` 优先走 `myproc()->pagetable` 的 `copyinstr()`；若运行在内核环境，可退化为直接拷贝
- 文件接口 `sys_open/sys_read/sys_write/sys_close` 会验证文件描述符、指针和长度，并交由 `fs/` 模块（`inode`、日志、缓冲）操作
- `sys_sleep`/`sys_uptime` 使用 `ticks` 共享变量，调用 `sleep()`/`wakeup()` 等待时钟中断
- 其他系统调用（`sys_getpid`, `sys_kill`, `sys_yield` 等）直接与 `proc/` 中的进程表和调度器交互

### 3.5 文件系统支撑

虽然本实验聚焦系统调用，但 `sys_open/read/write` 依赖 `kernel/fs`：
- `fs_init()` 格式化 RAM 磁盘、写入超级块、初始化日志（`log.c`）和 inode 缓存
- `create()/ialloc()/writei()` 等接口为测试创建临时文件
- `core/test.c` 的参数与安全性用例大量使用内核文件系统验证 `open/write/read/unlink`

## 四、实验内容与实现

### 4.1 目录与初始化流程

1. `kernel/boot/entry.S` 完成内存清零、设置 `mepc=start` 并跨入 S 模式
2. `kernel/core/main.c` 依次调用 `kinit()`、`kvminit()/kvminithart()`、`fileinit()/fs_init()`、`procinit()`、`timer_init()`，打开中断后创建测试进程 `run_syscall_tests`
3. `scheduler()` 接管 CPU，不再返回

### 4.2 关键结构

- `struct pushregs` (`include/trap.h`)：S 模式 trap 保存的寄存器快照
- `struct trapframe`：用户态陷阱帧，含内核栈/页表入口
- `struct proc` (`include/proc.h`)：新增 `trapframe*`、用户页表、文件描述符表与当前工作目录，供系统调用引用
- `include/syscall.h`：枚举 22 个系统调用号，`SYS_yield` 等后续扩展可直接挂到跳转表

### 4.3 代表性系统调用

- **进程控制**：`sys_getpid`/`sys_kill`/`sys_sleep`/`sys_yield` 直接操作 `struct proc` 或 `ticks`
- **文件操作**：`sys_open/write/read/close/dup/chdir/mkdir/mknod/unlink/fstat` 均通过 `fs/` 导出的 `struct inode` 与日志系统完成
- **时间服务**：`sys_uptime` 返回 `ticks`，`sys_sleep` 会在 `ticks_addr()` 上休眠
- **未实现接口**：`sys_fork/sys_exec/sys_pipe/sys_link/sys_sbrk` 目前返回 `-1`，可作为后续扩展留白

### 4.4 测试与调试工具

`kernel/core/test.c` 提供完整的回归测试：
1. **基本调用**：`getpid`、`sleep` 验证最小功能
2. **参数传递**：构造 `open/write/read/close` 流程，检测长度和返回值
3. **边界 & 安全**：检查空指针、越界长度、非法文件描述符
4. **性能测试**：多次调用 `getpid`，统计 `time` CSR 的差值

测试通过 `struct pushregs` 手动设置寄存器，调用 `handle_syscall()`，避免依赖用户态程序即可验证所有逻辑。

## 五、实验结果与分析

在 QEMU 中运行 `make run` 可看到如下关键信息（节选）：

```
Hello, OS!
============ syscall basic ============
[INFO] getpid -> 1
[INFO] sleep(1 tick) ok, elapsed=1
[PASS] basic system calls
============ syscall parameter ============
[INFO] open /sys_param -> fd=3
[INFO] write(fd=3,len=13) -> 13
[INFO] read(fd=3,len=13) -> Hello, World!
[PASS] parameter passing
============ syscall security ============
[TEST] syscall safety checks...
[INFO] reject invalid fd -1
[INFO] reject NULL buffer
[INFO] reject oversized read 1000 bytes
[PASS] syscall safety checks
============ syscall performance ============
[INFO] 10000 getpid() syscalls took 40216 cycles
```

- 参数传递测试确认 `argint/argaddr/argstr` 与 `copyinstr` 协同工作
- 安全测试验证对 NULL 指针、越界长度、非法 FD 的防御路径
- 性能测试提供了 `getpid` 的平均开销，可作为后续优化参考

## 六、思考题解答

1. **为什么需要同时保存 `pushregs` 与 `trapframe`？** 前者服务于 S 模式中断（无需切换页表），后者用于用户态切换，必须存储 `satp/epc` 及内核入口信息，才能在 `userret` 时恢复用户态环境。
2. **系统调用和中断的差别？** 两者都通过 trap 进入内核，但系统调用由软件触发且需手动递增 `sepc`，异常/中断则根据 `scause` 分类并通常不会修改触发指令。
3. **如何保证参数安全？** `argraw` 从寄存器取值，`fetchstr`/`copyinstr` 通过页表校验地址，`sys_*` 再次验证长度、指针和权限（例如 `open` 检查标志、`write` 限制传输大小）。
4. **为什么将系统调用实现划分到 `sysfile.c`/`proc/`/`fs/` 等模块？** 可以复用文件系统和进程管理逻辑，保持 `syscall.c` 仅负责调度与参数解析，后续新增接口无需修改核心分发器。
5. **如何扩展缺失的系统调用？** 在 `include/syscall.h` 定义号，在 `syscalls[]` 填写实现函数，并在对应子系统（如 `proc/exec.c` 或 `mm/sbrk.c`）完成实际功能即可。

## 七、实验总结

本实验在 RISC-V 平台上实现了一个自洽的系统调用框架，覆盖 trap 入口、参数提取、错误处理、用户内存校验以及与调度器、文件系统的整合。借助 `kernel/core/test.c` 的回归套件，我们验证了基础功能、边界条件和性能指标。下一步可以继续补齐 `fork/exec/pipe` 等接口，引入用户态程序，通过 `usys.pl` 生成桩代码，将本实验扩展为完整的用户态/内核态交互体系。***
