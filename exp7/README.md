# 实验七：文件系统

## 一、实验目的

1. 理解基于 RAM 磁盘的简化日志文件系统布局及其关键元数据结构
2. 掌握 inode/目录/位图之间的协作关系，能够实现文件的创建、读写、删除与遍历
3. 设计块缓存和写前日志（WAL）以保证一致性，在崩溃恢复时完整重放事务
4. 构建文件描述符与文件对象管理层，支持顺序读写与多次 `dup`
5. 使用内核侧测试套件评估文件系统的正确性、并发性、恢复能力与性能

## 二、实验环境

- 平台：RISC-V RV64，QEMU virt 单核
- 内核结构：
  - `kernel/boot/`：引导与进入 S 模式
  - `kernel/core/`：内核主函数 `main.c`、测试入口 `test.c`
  - `kernel/fs/`：`fs.c`、`bio.c`、`log.c`、`file.c` 等核心文件系统模块
  - `kernel/mm/`、`kernel/proc/`、`kernel/trap/`：提供内存、调度、trap 基础
  - `include/`：`fs.h`、`defs.h` 等公共声明
- 运行方式：`make run` → `main()` 初始化内核，创建 `run_fs_tests` 进程并运行文件系统测试

## 三、实验原理

### 3.1 磁盘布局与超级块

`kernel/fs/fs.c` 中 `fs_format()` 以 4 KiB 块初始化 RAM 磁盘，布局顺序为 `boot(0) | super(1) | log | inodes | bitmap | data`。  
`struct superblock` 记录：
- `size`：总块数 (`FSSIZE`)
- `ninodes`：inode 数目 (`NINODE`)
- `nlog/logstart`：日志区大小与起始块
- `inodestart/bmapstart/datastart`：inode 区、位图、数据区的偏移
这些元数据保证 `bmap()`、`balloc()` 能够定位磁盘区域并保持一致性。

### 3.2 inode、目录与路径解析

- `ialloc()` 在 inode 区扫描空闲条目，初始化 `type/nlink/addrs` 后返回内存 inode；`iget()/ilock()` 缓存在 `icache` 中并引用计数
- `bmap()` 为文件逻辑块号分配直接块与一级间接块（`NDIRECT` + `NINDIRECT`）
- `create()/dirlink()/dirlookup()` 维护目录项，写入“.”、“..”并支持 `namei()/nameiparent()` 路径解析；`dirent` 大小固定为 16 字节
- 删除文件时，`fs_delete_file()` 会在父目录写入空目录项并减少目标 inode 的 `nlink`

### 3.3 块缓存与 RAM 磁盘

`kernel/fs/bio.c` 使用 `struct buf` + LRU 双向链表缓存固定数量的块：
- `binit()` 初始化缓存与内存 RAM 磁盘数组 `ramdisk[FSSIZE][BSIZE]`
- `bread()` 命中返回缓存，否则调用 `ramdisk_rw()` 读出数据并统计命中/未命中
- `bwrite()` 总是写回 RAM 磁盘，并更新 `disk_write_count`
- `fs_get_cache_counters()` 暴露缓存统计供调试

### 3.4 日志系统（WAL）

`kernel/fs/log.c` 实现写前日志：
- `begin_op()/end_op()` 管理未完成事务数量，防止日志耗尽
- `log_write()` 记录脏块号；提交时 `write_log()` 将数据块写入日志区，`install_trans()` 将其复制回真实位置
- `recover_from_log()` 在启动时回放未完成事务；`fs_force_recovery()` 暴露给测试用于模拟崩溃

### 3.5 文件对象与系统调用

`kernel/fs/file.c` 维护 `struct file` 数组：
- `filealloc()/filedup()/fileclose()` 负责引用计数
- `fileread()/filewrite()` 在 `FD_INODE` 上调用 `readi()/writei()`，并使用 `begin_op()/end_op()` 保证日志事务原子性
同时，`fs.c` 提供 `fs_write_file()/fs_read_file()/fs_delete_file()` 等简化接口，供测试套件直接调用。

### 3.6 测试与调试支撑

`include/fs.h` 扩展了 `fs_usage_stats`、`fs_inode_usage`、`fs_cache_counters` 等结构；`fs_get_usage_stats()/fs_collect_inode_usage()` 可以统计空闲块、inode 以及缓存命中率。  
`kernel/core/test.c` 使用这些接口构建了多阶段测试（完整性、并发、恢复、性能），并提供调试命令打印文件系统状态。

## 四、实验内容与实现

1. **初始化流程**（`kernel/core/main.c`）：串联 `kinit()` → `kvminit()` → `fileinit()/fs_init()` → `procinit()/timer_init()` → `create_process("fs-tests", run_fs_tests)`
2. **块缓存**（`kernel/fs/bio.c`）：在 RAM 磁盘上模拟块设备，记录命中/未命中与磁盘读写次数
3. **日志系统**（`kernel/fs/log.c`）：实现 WAL 提交、并发控制与重放；`log_force_recover()` 用于注入崩溃
4. **文件系统核心**（`kernel/fs/fs.c`）：`create()`、`bmap()`、`ialloc()`、`fs_write_file()`、`fs_delete_file()` 等接口支撑用户层文件操作
5. **文件描述符层**（`kernel/fs/file.c`）：提供 `fileread/filewrite`，保证写入按 `MAXOPBLOCKS` 分批提交
6. **测试套件**（`kernel/core/test.c`）：`run_fs_tests()` 依次调用 `test_filesystem_integrity()`、`test_concurrent_access()`、`test_crash_recovery()`、`test_filesystem_performance()` 并输出统计信息

## 五、实验结果与分析

一次完整运行的关键信息：

```
[SUITE] running filesystem tests
============ filesystem integrity ============
[INFO] create/read/unlink OK
============ filesystem recovery ============
[INFO] wrote "journal-entry" to /fs_recovery, triggering recovery
[INFO] after recovery read "journal-entry" ...
[PASS] crash recovery
============ filesystem performance ============
[INFO] small files (16 x 4B): 13562 cycles
[INFO] large file (65536B): 28041 cycles
============ filesystem state ============
[INFO] blocks total=2000 data=... free=...
[INFO] cache hits=120 misses=34
[SUITE] filesystem tests finished
```

- **完整性测试**：创建文件、写入、读取、删除，验证 `dirlookup` 与 `writei/readi`
- **并发测试**：多个进程并行写入不同文件，检查 inode 锁、日志与块缓存无竞态
- **崩溃恢复**：通过 `fs_force_recovery()` 模拟重播，确保未提交日志被重新安装或清理
- **性能与统计**：记录创建大量小文件与单个大文件的耗时，并输出缓存命中率、磁盘 I/O 计数、空闲块/ inode 等调试信息

## 六、思考题

1. **为何采用写前日志而非直接写盘？** WAL 确保崩溃后事务要么全部生效、要么完全丢弃，避免目录/位图半更新导致的不一致。
2. **inode 缓存如何避免泄漏？** `iget()/ilock()` 与 `iput()/iunlockput()` 配对，引用计数降为 0 且 `nlink==0` 时回收；`icache.lock` 串行化分配。
3. **如何平衡块缓存大小与命中率？** `bget()` 使用 LRU；缓存命中不足可通过 `fs_get_cache_counters()` 观察并调整 `NBUF`。
4. **日志区满了怎么办？** `begin_op()` 在日志剩余空间不足时阻塞后续事务，等待 `end_op()` 提交释放空间。
5. **大文件如何扩展？** `bmap()` 在 `NDIRECT` 用尽后自动分配一级间接块；若需要更大文件，可扩展 `addrs` 支持双重间接。

## 七、实验总结

本实验在 RAM 磁盘上实现了一个自洽的日志文件系统：自底向上包括块缓存、超级块、inode、目录、日志与文件描述符层；顶层借助 `fs_write_file` 等简化接口和测试框架验证了功能、并发与恢复。通过分析运行日志和调试统计信息，掌握了文件系统从布局、缓存、日志到一致性的完整链路，为后续支持真实块设备或更复杂的数据结构打下基础。***
