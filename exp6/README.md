# 实验六：系统调用

## 一、实验目的

1. 掌握 RISC-V 架构下用户态通过 `ecall` 进入内核的完整链路，熟悉 `trapframe`、`scause/sepc` 的作用  
2. 设计系统调用描述表、参数提取与错误处理框架，支持多种系统调用号及权限检查  
3. 实现典型的进程/内存/IO 系统调用（`fork/exit/wait/getpid/write/read/sbrk/getprocinfo` 等），并与进程管理模块协同  
4. 构建用户端桩代码及内核内的测试包装器，验证参数校验、指针安全与性能  
5. 编写综合测试用例，覆盖功能正确性、越界/非法参数、防护措施与性能统计

## 二、实验环境

- 平台：QEMU virt（RV64）  
- 工具链：`riscv64-unknown-elf-gcc/binutils`  
- 关键硬件：CLINT 定时器、UART16550 控制台  
- 代码结构：`kernel/`（核心逻辑）、`include/`（头文件）、`kernel/mm/`、`kernel/syscall_test.c` 等  
- 运行模式：内核在 M 态直接调度测试进程，模拟用户/内核切换（无真正用户空间但保持接口一致）

## 三、实验原理

### 3.1 用户态陷入与 `trapframe`

- `kernel/usys.S` 生成用户桩：将系统调用号写入 `a7`，执行 `ecall`  
- `trap_entry.S` 保存 31 个通用寄存器与 `mepc/mstatus` 后调用 `trap_handler()`  
- `trap.c` 中根据 `mcause` 判断：`exc_code=8/9`（U/S 模式 `ecall`）进入 `syscall_dispatch()`，中断则调用 `clock_set_next_event()+yield()`  
- `struct trap_context`（`include/trap.h`）被挂在 `struct proc::trap_context` 上，参数通过 `a0-a5` 读取，返回值写回 `ctx->a0`

### 3.2 系统调用表与分发机制

`kernel/syscall.c` 定义 `struct syscall_desc syscall_table[SYSCALL_MAX]`，记录实现函数、名称、参数计数与类型掩码。`syscall_dispatch()`：  
1. 验证 `curr_proc` 与系统调用号  
2. 调用 `check_syscall_permission()`（当前默认允许）  
3. 重置并记录错误码，执行对应 `sys_*` 函数  
4. 将返回值或 `-1` 写回 `ctx->a0`，并打印调试信息  

```c
void syscall_dispatch(struct trap_context *ctx) {
    struct proc *p = myproc();
    p->trap_context = ctx;
    uint64_t num = ctx->a7;
    if (num >= SYSCALL_MAX || !syscall_table[num].func) {
        set_syscall_error(SYSERR_NOT_SUPPORTED);
        ctx->a0 = -1;
        return;
    }
    long ret = syscall_table[num].func();
    if (get_last_syscall_error() != SYSERR_SUCCESS) ctx->a0 = -1;
    else ctx->a0 = ret;
    p->trap_context = NULL;
}
```

### 3.3 参数提取与用户内存访问

- `argraw/argint/argaddr/argstr` 从 `trap_context->a0~a5` 取整数、指针或字符串  
- `copyin/copyout/fetchstr` 在内核测试环境中直接 memcpy，但仍加上 NULL、地址范围、长度（≤4KB）检查，并拒绝访问 `0x8000_0000` 以上的内核区域  
- `syscall_wrappers.c` 为测试提供用户态视角的 `read/write/wait/getprocinfo` 包装，并实现 `is_valid_user_pointer()` / `is_valid_pointer_for_test()` 以模拟权限检查及“测试模式”放宽策略

### 3.4 典型系统调用

- **进程控制**：`sys_fork()` 复用 `alloc_proc()` 复制上下文，并将子进程返回地址设为 `fork_return_point()`；`sys_exit()/sys_wait()` 与 `proc.c` 中的 `exit_process()/wait_process()` 协作，使用 `copyout` 将状态写回用户空间  
- **查询类**：`sys_getpid()/sys_getppid()` 直接读取 `curr_proc`；`sys_getprocinfo()` 组装 `struct procinfo` 并拷贝至用户缓冲区  
- **IO**：`sys_write()/sys_read()` 仅支持标准输入输出，执行指针有效性与长度限制。`sys_write` 将用户数据复制到内核页再输出到控制台，`sys_read` 返回预置字符串  
- **内存管理**：`sys_brk/sys_sbrk` 目前返回固定 `brk`（1MB）但保留接口，为后续堆管理打基础

### 3.5 用户接口与调试

- `kernel/usys.S` 使用 `SYSCALL` 宏生成 19 个桩函数，与 `include/user.h` 中的原型匹配  
- `kernel/syscall_test.c` 提供 `test_basic_syscalls/ test_parameter_passing/ test_security/ test_syscall_performance` 等测试，验证 fork-wait 流程、写入边界/NULL/内核指针、读缓冲越界等  
- `test_mode_enabled`（`syscall_wrappers.c`）可临时放宽指针限制以构造特殊场景；`syscall_error_str()` 统一打印错误原因

## 四、实验内容与实现

### 4.1 关键头文件

- `include/syscall.h`：系统调用号、`struct syscall_desc`、`struct procinfo`、错误码与原型  
- `include/sysproc.h`：在实验五基础上扩展的进程接口，供系统调用实现调用  
- `include/syscall_test.h`：声明测试入口与测试模式控制函数

### 4.2 内核模块

| 文件 | 作用 |
| --- | --- |
| `kernel/syscall.c` | 系统调用表、分发器、参数提取、`copyin/out`、错误/权限处理、框架自检 |
| `kernel/sysproc.c` | 具体 `sys_*` 实现：fork/exit/wait/kill/getpid/getppid/write/read/brk/sbrk/getprocinfo 等 |
| `kernel/syscall_wrappers.c` | 内核态模拟用户接口，封装 `read/write/getpid/...` 并实现指针校验与测试模式 |
| `kernel/usys.S` | 依据宏生成 `ecall` 桩，保证用户 API 与号位一致 |
| `kernel/syscall_test.c` | 综合测试套件，覆盖功能、参数、安全、性能与信息查询 |
| `kernel/trap.c` | 在异常处理里捕获 `ecall` 并调用 `syscall_dispatch()` |
| `kernel/main.c` | 初始化 `proc/syscall`，创建测试进程并调用 `run_comprehensive_syscall_tests()` |

### 4.3 运行流程

1. `main()` 调用 `proc_init()`/`syscall_init()`，手动创建测试进程并把 `ra` 指向 `initial_process_entry()`  
2. 测试进程通过 `syscall_wrappers` 调用 API，例如 `fork()` → `SYSCALL fork` → `ecall` → `trap_handler`  
3. `syscall_dispatch()` 根据 `a7` 调用 `sys_fork()` 等函数；`sys_fork()` 复制 PCB 并设置子进程入口  
4. 子进程在 `fork_return_point()` 中完成工作并调用 `exit_process()`，父进程使用 `wait()` 获取状态  
5. `sys_write()` 等函数执行参数/指针验证，通过 `copyin/out` 访问用户缓冲区；失败时设置错误码并返回 -1  
6. `syscall_test.c` 中的其他测试继续运行，直到所有断言完成

## 五、测试结果与分析

### 5.1 基本功能

```
=== Testing Basic System Calls ===
Current PID: 2
Parent process: PID=2, created child 3
Parent: child 3 exited with status: 0
✓ Fork/wait/exit test PASSED
```

表明 `fork→wait→exit` 流程闭环，`sys_wait` 通过 `copyout` 返回状态。

### 5.2 参数与安全

```
=== Testing Parameter Passing ===
Write to invalid fd (-1): result=-1
Write with NULL buffer: result=-1
Write with negative length: result=-1
DEBUG: Rejecting kernel space pointer 0x80000000
```

`sys_write`/`read` 对错误参数及时拒绝并打印调试信息；`test_security()` 进一步验证越界与内核指针防护。

### 5.3 指针验证与测试模式

```
DEBUG: Test mode enabled - security checks relaxed
Write with NULL buffer: result=-1
DEBUG: Allowing kernel space pointer for testing: 0x80001000
```

表明在测试模式下可构造特殊指针，但仍对 `NULL/0x1000000/0x30000000` 等明显无效地址进行拦截。

### 5.4 性能测量（可选）

```
1000 getpid() calls took 40216 cycles
Average per call: 40 cycles
```

虽然此数据仅作参考，但展示了使用 `time` CSR 统计系统调用平均开销的方法。

## 六、思考题

1. **为何需要 `trapframe`？** 它记录用户态寄存器快照，既让内核获取参数/恢复执行，也隔离了内核与用户栈。与中断类似，但系统调用由软件触发、`scause` 指向 `ecall`，中断则是异步事件。  
2. **系统调用参数如何安全传递？** 通过 `argint/argaddr/argstr` 在 `trap_context` 中读取，再结合 `copyin/out` 与指针验证，避免内核直接解引用用户地址。  
3. **为何在内核测试中仍实现桩代码？** `usys.S`/`syscall_wrappers.c` 模拟了真实用户态，便于后续移植到真正的用户空间；同时控制“测试模式”以构造边界场景。  
4. **如何扩展系统调用表？** 新增 `SYS_xxx` 号并在 `syscall_table` 中填入函数与元数据，若需要权限控制可在 `check_syscall_permission()` 中结合 `proc` 的 cred 字段进行判定。  
5. **如何提升 `copyin/out` 安全性？** 当前实现直接返回虚拟地址，可在后续实验中接入真实页表遍历（`walkaddr`）、权限位校验和长度逐页验证，配合 `sbrk` 实际管理用户空间。

## 七、实验总结

本实验完成了 RISC-V 系统调用从用户桩、陷阱入口、分发框架到具体实现与测试的闭环。通过统一的参数解析、指针验证与错误码机制，保证了 `fork/exit/wait/write/read` 等核心接口的可用性，同时引入 `getprocinfo`、测试模式开关和性能计数方法。未来可继续：  
(1) 接入真正的用户地址空间和页表校验；  
(2) 扩展文件系统/管道等 I/O 系统调用；  
(3) 将测试迁移到用户态程序，结合 `init`/`sh` 形成完整的系统调用生态。  

借助本实验奠定的框架，后续实验能够更加专注于文件系统与用户程序层面的功能拓展。***
