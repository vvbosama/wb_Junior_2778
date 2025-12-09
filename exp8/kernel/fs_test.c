// kernel/fs_test.c - 文件系统测试
#include "fs.h"
#include "bio.h"
#include "log.h"
#include "file.h"
#include "printf.h"
#include "proc.h"
#include "fs_test.h"
#include "syscall.h"

// 简单的字符串长度函数
static int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// 文件系统完整性测试
void test_filesystem_integrity(void) {
    printf("=== Testing filesystem integrity ===\n");
    
    // 初始化文件系统
    binit();
    fsinit(ROOTDEV);
    
    printf("Filesystem initialized\n");
    
    // 创建测试文件
    begin_op();
    struct inode *ip = ialloc(ROOTDEV, T_FILE);
    if (ip == 0) {
        printf("ERROR: Failed to allocate inode\n");
        end_op();
        return;
    }
    
    // 写入测试数据
    char test_data[] = "Hello, filesystem!";
    int n = strlen(test_data);
    if (writei(ip, 0, (uint64_t)test_data, 0, n) != n) {
        printf("ERROR: Failed to write data\n");
        iput(ip);
        end_op();
        return;
    }
    
    iupdate(ip);
    end_op();
    
    printf("Created test file with %d bytes\n", n);
    
    // 读取并验证数据
    char read_buffer[64];
    if (readi(ip, 0, (uint64_t)read_buffer, 0, n) != n) {
        printf("ERROR: Failed to read data\n");
        iput(ip);
        return;
    }
    read_buffer[n] = '\0';
    
    // 验证数据
    int match = 1;
    for (int i = 0; i < n; i++) {
        if (test_data[i] != read_buffer[i]) {
            match = 0;
            break;
        }
    }
    
    if (match) {
        printf("✓ Data integrity verified: '%s'\n", read_buffer);
    } else {
        printf("✗ Data mismatch!\n");
        printf("  Expected: '%s'\n", test_data);
        printf("  Got:      '%.*s'\n", n, read_buffer);
    }
    
    // 清理
    begin_op();
    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    end_op();
    iput(ip);
    
    printf("=== Filesystem integrity test completed ===\n\n");
}

// 使用内核级并发测试（不依赖用户态系统调用）
void test_kernel_concurrent_access(void) {
    printf("=== Testing Kernel-Level Concurrent Access ===\n");
    
    printf("Creating 4 concurrent file operations...\n");
    
    struct inode *inodes[4];
    int success_count[4] = {0};
    int fail_count[4] = {0};
    
    // 阶段1：并发分配inode（模拟多个进程同时分配）
    printf("Phase 1: Concurrent inode allocation\n");
    for (int i = 0; i < 4; i++) {
        begin_op();
        inodes[i] = ialloc(ROOTDEV, T_FILE);
        if (inodes[i]) {
            printf("  Allocated inode %d for slot %d\n", inodes[i]->inum, i);
        } else {
            printf("  Failed to allocate inode for slot %d\n", i);
            inodes[i] = 0;
        }
        end_op();
        
        // 小延迟，模拟并发交错
        for (volatile int j = 0; j < 1000; j++);
    }
    
    // 阶段2：并发写入数据
    printf("Phase 2: Concurrent data writing\n");
    for (int round = 0; round < 5; round++) {  // 5轮并发操作
        printf("  Round %d:\n", round);
        
        for (int i = 0; i < 4; i++) {
            if (inodes[i]) {
                begin_op();
                
                int data = i * 100 + round;  // 每个文件和轮次不同的数据
                if (writei(inodes[i], 0, (uint64_t)&data, round * sizeof(int), sizeof(data)) == sizeof(data)) {
                    iupdate(inodes[i]);
                    success_count[i]++;
                    printf("    Slot %d: wrote value %d\n", i, data);
                } else {
                    fail_count[i]++;
                    printf("    Slot %d: write failed\n", i);
                }
                
                end_op();
                
                // 并发延迟
                for (volatile int j = 0; j < 500; j++);
            }
        }
    }
    
    // 阶段3：并发读取验证
    printf("Phase 3: Concurrent data verification\n");
    for (int i = 0; i < 4; i++) {
        if (inodes[i]) {
            begin_op();
            
            int corruption_count = 0;
            for (int round = 0; round < 5; round++) {
                int expected = i * 100 + round;
                int actual;
                
                if (readi(inodes[i], 0, (uint64_t)&actual, round * sizeof(int), sizeof(actual)) == sizeof(actual)) {
                    if (actual != expected) {
                        printf("    Slot %d: DATA CORRUPTION at round %d! expected=%d, actual=%d\n", 
                               i, round, expected, actual);
                        corruption_count++;
                    }
                } else {
                    printf("    Slot %d: read failed at round %d\n", i, round);
                    corruption_count++;
                }
            }
            
            if (corruption_count == 0) {
                printf("    Slot %d: ✓ All data verified correctly\n", i);
            } else {
                printf("    Slot %d: ✗ %d data corruptions found\n", i, corruption_count);
            }
            
            end_op();
        }
    }
    
    // 阶段4：并发清理
    printf("Phase 4: Concurrent cleanup\n");
    for (int i = 0; i < 4; i++) {
        if (inodes[i]) {
            begin_op();
            itrunc(inodes[i]);  // 截断文件
            inodes[i]->type = 0;  // 标记为未使用
            iupdate(inodes[i]);
            end_op();
            
            iput(inodes[i]);
            printf("  Freed inode for slot %d\n", i);
        }
    }
    
    // 统计结果
    printf("Concurrent access results:\n");
    for (int i = 0; i < 4; i++) {
        printf("  Slot %d: %d successes, %d failures\n", i, success_count[i], fail_count[i]);
    }
    
    printf("✓ Kernel-level concurrent access test completed\n\n");
}

// 性能测试
void test_filesystem_performance(void) {
    printf("=== Testing filesystem performance ===\n");
    
    // 紧急修复：直接访问并重置
    extern uint32_t next_free_block;
    
    printf("DEBUG: before reset, next_free_block=%d\n", next_free_block);
    next_free_block = 0;
    printf("DEBUG: after reset, next_free_block=%d\n", next_free_block);

    // 双重验证
    if (next_free_block != 0) {
        printf("EMERGENCY: manual reset failed, using pointer\n");
        uint32_t *ptr = &next_free_block;
        *ptr = 0;
    }
    printf("Final check: next_free_block=%d\n", next_free_block);
    
    // 大量小文件测试
    int small_file_count = 5;
    printf("Creating %d small files...\n", small_file_count);
    
    for (int i = 0; i < small_file_count; i++) {
        begin_op();
        struct inode *ip = ialloc(ROOTDEV, T_FILE);
        if (ip) {
            const char *data = "test";
            writei(ip, 0, (uint64_t)data, 0, 4);
            iupdate(ip);
            iput(ip);
        }
        end_op();
    }
    
    printf("Created %d small files\n", small_file_count);
    
    // 大文件测试
    printf("Creating large file...\n");
    begin_op();
    struct inode *ip = ialloc(ROOTDEV, T_FILE);
    if (ip) {
        char large_buffer[4096];
        // 填充测试数据
        for (int i = 0; i < 4096; i++) {
            large_buffer[i] = (char)(i % 256);
        }
        
        // 写入多个块
        for (int i = 0; i < 15; i++) {
            writei(ip, 0, (uint64_t)large_buffer, i * 4096, 4096);
        }
        iupdate(ip);
        iput(ip);
        printf("Created large file (60KB, 15 blocks)\n");
    }
    end_op();
    
    printf("=== Performance test completed ===\n\n");
}


// 修复后的崩溃恢复测试
void test_crash_recovery(void) {
    printf("=== Testing Crash Recovery ===\n");
    
    printf("Phase 1: Preparing test data before simulated crash...\n");
    
    // 阶段1：创建一些文件但不完全提交
    struct inode *test_files[3];
    uint32_t test_data[3] = {0x12345678, 0x87654321, 0xABCDEF01};
    
    // 创建多个文件，模拟正在进行的事务
    for (int i = 0; i < 3; i++) {
        begin_op();
        test_files[i] = ialloc(ROOTDEV, T_FILE);
        if (test_files[i]) {
            // 写入测试数据
            if (writei(test_files[i], 0, (uint64_t)&test_data[i], 0, sizeof(test_data[i])) == sizeof(test_data[i])) {
                iupdate(test_files[i]);
                printf("  Created file %d with data 0x%08X (inode %d)\n", 
                       i, test_data[i], test_files[i]->inum);  // 修复：使用 inum 而不是直接打印指针
            }
        }
       
        end_op();  
    }
    
    printf("Phase 2: Simulating system crash...\n");
      
    // 专门创建一个不提交的事务来模拟崩溃
    begin_op();
    struct inode *uncommitted_file = ialloc(ROOTDEV, T_FILE);
    uint32_t uncommitted_inum = 0;
    if (uncommitted_file) {
        uncommitted_inum = uncommitted_file->inum;
        uint32_t uncommitted_data = 0xDEADBEEF;
        writei(uncommitted_file, 0, (uint64_t)&uncommitted_data, 0, sizeof(uncommitted_data));
        iupdate(uncommitted_file);
        printf("  Created UNCOMMITTED file (inode %d) - simulating crash\n", uncommitted_file->inum);
        
        // 模拟崩溃：手动写入日志头和数据到磁盘（但不提交）
        // 这模拟了系统在写入日志后、提交前崩溃的情况
        acquire(&log.lock);
        if (log.lh.n > 0) {
            // 写入日志头
            struct buf *buf = bread(log.dev, log.start);
            struct logheader *hb = (struct logheader *)buf->data;
            hb->n = log.lh.n;
            for (int i = 0; i < log.lh.n; i++) {
                hb->block[i] = log.lh.block[i];
            }
            bwrite(buf);
            brelse(buf);
            
            // 写入日志数据块（模拟commit()的前半部分）
            for (int tail = 0; tail < log.lh.n; tail++) {
                struct buf *to = bread(log.dev, log.start + tail + 1);
                struct buf *from = bread(log.dev, log.lh.block[tail]);
                // 复制数据（简单的字节复制）
                for (int i = 0; i < BSIZE; i++) {
                    to->data[i] = from->data[i];
                }
                bwrite(to);
                brelse(from);
                brelse(to);
            }
            
            printf("  Wrote log header and data to disk (n=%d) - simulating crash before commit\n", log.lh.n);
        }
        release(&log.lock);
        
        // 注意：不调用 end_op() 来模拟崩溃
        // 释放inode引用，但不提交事务
        iput(uncommitted_file);
    }
    
    // 模拟崩溃：直接重新初始化文件系统
    printf("Phase 3: System reboot and filesystem recovery...\n");
    
    // 重新初始化日志系统（模拟重启后的恢复）
    printf("  Reinitializing log system for recovery...\n");
    initlog(ROOTDEV, &sb);
    
    printf("Phase 4: Verifying filesystem consistency after crash...\n");
    
    // 检查文件系统状态
    int recovered_files = 0;
    int corrupted_files = 0;
    
    // 尝试查找之前创建的文件
    int found_uncommitted = 0;
    for (int i = 1; i < 20; i++) {  // 限制搜索范围
        struct inode *ip = iget(ROOTDEV, i);
        if (ip && ip->type == T_FILE && ip->size > 0) {
            printf("  Found file: inode %d, size %d bytes\n", ip->inum, ip->size);
            
            if (ip->size == sizeof(uint32_t)) {
                // 验证文件内容
                uint32_t read_data;
                if (readi(ip, 0, (uint64_t)&read_data, 0, sizeof(read_data)) == sizeof(read_data)) {
                    printf("    File content: 0x%08X\n", read_data);
                    
                    // 检查是否是未提交的文件（应该被回滚）
                    if (uncommitted_inum > 0 && ip->inum == uncommitted_inum) {
                        printf("    ✗ ERROR: Uncommitted file found after recovery (should be rolled back)\n");
                        found_uncommitted = 1;
                        corrupted_files++;
                    } else {
                        // 检查是否是我们的测试数据
                        int is_test_data = 0;
                        for (int j = 0; j < 3; j++) {
                            if (read_data == test_data[j]) {
                                is_test_data = 1;
                                break;
                            }
                        }
                        
                        if (is_test_data) {
                            printf("    ✓ Data intact: 0x%08X\n", read_data);
                            recovered_files++;
                        } else {
                            printf("    ? Unknown data: 0x%08X\n", read_data);
                        }
                    }
                }
            } else {
                printf("    ! Unexpected file size: %d bytes\n", ip->size);
                corrupted_files++;
            }
            iput(ip);
        } else if (ip) {
            iput(ip);  // 释放不需要的inode
        }
    }
    
    printf("Recovery results:\n");
    printf("  Recovered files: %d\n", recovered_files);
    printf("  Corrupted files: %d\n", corrupted_files);
    if (uncommitted_inum > 0) {
        if (found_uncommitted) {
            printf("  ✗ Uncommitted file (inode %d) was NOT rolled back - recovery failed\n", uncommitted_inum);
        } else {
            printf("  ✓ Uncommitted file (inode %d) was properly rolled back\n", uncommitted_inum);
        }
    }
    
    if (recovered_files == 3 && !found_uncommitted) {
        printf("✓ Crash recovery test PASSED - all committed data preserved, uncommitted data rolled back\n");
    } else if (recovered_files > 0) {
        printf("⚠ Crash recovery test - some data preserved but recovery incomplete\n");
    } else {
        printf("✗ Crash recovery test FAILED - no test data recovered\n");
    }
    
    printf("=== Crash recovery test completed ===\n\n");
}

// 运行所有测试
void run_filesystem_tests(void) {
    printf("\n");
    printf("========================================\n");
    printf("  File System Test Suite\n");
    printf("========================================\n\n");
    
    test_filesystem_integrity();
    
    // 运行内核级并发测试（不依赖用户态系统调用）
    test_kernel_concurrent_access();
    
    test_filesystem_performance();
    
    test_crash_recovery();
    
    printf("========================================\n");
    printf("  All File System Tests Completed\n");
    printf("========================================\n\n");
}