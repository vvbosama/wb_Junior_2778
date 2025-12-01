# 实验七：文件系统

## 一、实验目的

1. 分析简化版 xv6 文件系统的磁盘布局、元数据与缓存设计，掌握 `superblock/inode/bitmap` 的作用
2. 实现支持直接块+间接块的文件数据映射，并结合内存 inode 缓存提供 `namei/dirlookup` 等目录操作
3. 构建块缓存（`bio.c`）与写前日志（`log.c`），保证崩溃恢复与并发写入时的原子性
4. 提供 `file.c` 层的文件描述符管理以及扩展功能（创建时间 `file_time.c`），并通过内核态测试验证
5. 设计一套文件系统测试工具（`fs_test.c`、`file_time_test.c`），覆盖完整性、并发性、性能与恢复能力

## 二、实验环境

- 架构：RISC-V RV64，运行于 QEMU virt
- 工具链：`riscv64-unknown-elf-gcc/binutils`
- 块设备：使用 `bio.c` 中的内存磁盘 (`FSSIZE * BSIZE`) 模拟磁盘扇区
- 块大小：4 KiB (`BSIZE=4096`)，文件系统总大小 2000 块
- 核心目录结构：`kernel/`（fs/bio/log/file 等实现）、`include/`（对应头文件）、`kernel/fs_test.c`（测试）

## 三、实验原理

### 3.1 磁盘布局与超级块

- 布局约定：`| boot(0) | super(1) | log | inode blocks | bitmap | data blocks |`
- `struct superblock` （`include/fs.h`）记录魔数、块总数、inode 数、日志大小与各区域起始块
- `fsinit()` 读取超块，不存在则 `mkfs()` 重新创建并初始化 inode 区与根目录
- `next_free_block` 辅助在 `bmap()` 中分配数据块，同时 `fs_reset_allocator()` / 测试代码可重置该计数器，防止碎片或越界

### 3.2 inode 管理

- 磁盘 inode (`struct dinode`) 包含类型、大小、12 个直接块 + 1 个单级间接块以及创建时间戳 `ctime`
- 内存 inode (`struct inode`) 增加引用计数、有效位、锁等字段，存放于 `icache`
- `iget()` 在缓存中查找/分配 inode，`ialloc()` 在磁盘结构上找空闲项并初始化，`iupdate()` 写回
- `bmap()` 输入逻辑块号 -> 直接块/间接块 -> 物理块号，缺页时在位图区域之后按顺序分配并写日志
- `itrunc()` 释放文件占用的块（含间接块），`stati()` 导出统计信息

### 3.3 块缓存与日志

- `bio.c` 维护 `struct buf` 数组与 LRU 链表：
  - `binit()` 初始化缓存并清空模拟磁盘；
  - `bget()` 先查缓存，再按 LRU/引用计数替换；
  - `bread/bwrite/brelse` 完成读写与引用计数管理
- `log.c` 基于 `struct log` 实现写前日志：
  - `begin_op/end_op` 统计未完成操作数，保证日志空间充足
  - `log_write()` 记录被修改的块号
  - `commit()` 将日志块写入日志区，再写回真实位置并清空日志头
  - `recover_from_log()` 在启动时丢弃未提交事务，模拟崩溃回滚
- 自旋锁采用 `__sync_lock_test_and_set` 实现简易原子性

### 3.4 文件接口与扩展

- `file.c` 为 `struct file` 提供 `filealloc/fileclose/filedup/fileread/filewrite/filestat`
- `fs.c` 中 `dirlookup/dirlink/namex/namei` 支持目录项遍历与路径解析，目录项使用固定 14 字节名称 (`DIRSIZ`)
- `file_time.c` 在 inode 中记录 `ctime`，提供 `record_file_creation_time()`、`display_file_time_info()`、`compare_file_ages()` 等接口，并在测试中验证顺序

## 四、实验内容与实现

### 4.1 关键源码

| 模块 | 功能概述 |
| --- | --- |
| `kernel/fs.c` | 超级块加载、mkfs、inode/块分配、读写、目录操作、路径解析、分配器重置 |
| `kernel/bio.c` | LRU 块缓存、内存磁盘读写、pin/unpin |
| `kernel/log.c` | 写前日志、`begin_op/end_op`、崩溃恢复、commit |
| `kernel/file.c` | 文件描述符管理，桥接 inode 与 `read/write/stat` |
| `kernel/file_time.c` | 文件创建时间记录与比较 |
| `kernel/fs_test.c` | 完整性、并发、性能、崩溃恢复测试 |
| `kernel/file_time_test.c` | 文件时间扩展测试套件 |
| `kernel/main.c` | 启动测试入口 (`run_filesystem_tests()` / `run_file_time_tests()`) |

### 4.2 块/日志操作流程

1. 上层 `begin_op()` 进入事务，`writei()` 调用 `bmap()` 找块并写入
2. `log_write()` 将脏块号加入日志头，`bwrite()` 只写内存磁盘
3. `end_op()` 在最后一个调用者退出时触发 `commit()`：写日志头 -> 日志块 -> 清零日志头 -> 将日志块写回对应位置
4. 崩溃/重启后 `recover_from_log()` 读取日志头，若 `n>0` 直接清零（未提交事务被丢弃）

### 4.3 文件系统测试

- `test_filesystem_integrity()`：创建 inode、写入/读取字符串并验证，再 `itrunc` 清理
- `test_kernel_concurrent_access()`：无需用户态 fork，由 4 组 inode 模拟并发写入/读取/清理，记录成功/失败次数
- `test_filesystem_performance()`：重置 `next_free_block`，批量创建小文件、写入大文件，输出进度信息
- `test_crash_recovery()`：创建/提交文件，再模拟“崩溃”前写入日志但不 `end_op()`，随后重启日志系统并检查是否丢弃未提交 inode

### 4.4 文件时间测试

- `test_file_creation_time()`：验证 `record_file_creation_time()`、`compare_file_ages()`、时间持久化
- `test_batch_file_times()`：创建 5 个文件检查时间顺序与读取一致性
- `test_edge_cases()`：NULL、非法 inode、未记录时间等边界场景

## 五、实验结果与分析

示例日志（裁剪）：

```
========================================
  File System Test Suite
========================================
=== Testing filesystem integrity ===
Filesystem initialized
Created test file with 17 bytes
✓ Data integrity verified: 'Hello, filesystem!'
=== Filesystem integrity test completed ===
=== Testing Kernel-Level Concurrent Access ===
  Slot 0: 5 successes, 0 failures
✓ Kernel-level concurrent access test completed
=== Testing filesystem performance ===
DEBUG: before reset, next_free_block=123
Created large file (60KB, 15 blocks)
=== Performance test completed ===
=== Testing Crash Recovery ===
✓ Uncommitted file (inode 9) was properly rolled back
✓ Crash recovery test PASSED
========================================
  All File System Tests Completed
========================================
```

文件时间测试输出（节选）：

```
=== Testing File Creation Time Recording ===
FILE_TIME: Recorded creation time for inode 5: 123456
File age comparison result: -1
✓ File 1 is older than File 2 - CORRECT
=== Testing Batch File Creation Times ===
✓ All files created in correct time order
```

结果说明：
- 基础读写与目录/数据块管理正常；
- 并发写入通过多轮读写未出现数据损坏；
- 手动重置 `next_free_block` 后可持续写入，表明块分配器与测试脚本协同正常；
- 写前日志能够回滚未提交的 inode，恢复后无残留；
- 文件创建时间在单次运行中可正确记录、比较与展示。

## 六、思考题解答

1. **为何采用固定布局 + 顺序块分配？** 结构简单、便于调试；可通过位图/空闲链实现更高效的碎片管理。
2. **inode 缓存与引用计数的意义？** 减少磁盘访问，避免多个文件描述符并发修改时丢失更新；引用计数降至 0 才能复用缓存槽。
3. **写前日志如何确保崩溃恢复？** 所有数据块先写入日志并记录 `n`，只有提交后才覆盖正式块；`recover_from_log()` 根据 `n` 判断是否应用或丢弃。
4. **块缓存如何兼顾命中率与一致性？** `bread` 命中直接返回，未命中通过 LRU 替换；`bwrite` 标记脏块后写回内存磁盘，并与日志联动。
5. **文件时间为何放在 inode 中？** 避免额外表结构，随着 inode 写回即可持久化；若要实现更丰富的元数据，可扩展 inode 结构或引入 xattr。

## 七、实验总结

本实验实现了一个可运行的简化日志文件系统：
- 完整的磁盘布局、inode/目录/路径解析逻辑
- LRU 块缓存与写前日志保障并发一致性与崩溃恢复
- 文件描述符层与系统调用框架打通，为后续用户态程序提供接口
- 文件创建时间等扩展功能展示了如何在 inode 层添加元数据
- 测试套件覆盖功能、并发、性能与恢复，便于回归验证

后续可在此基础上继续拓展：真正的磁盘驱动、位图分配、目录层级创建、硬链接/符号链接、用户态工具链等，以逐步向完整操作系统文件子系统演进。
