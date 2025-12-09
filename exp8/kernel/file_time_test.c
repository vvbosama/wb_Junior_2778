// kernel/file_time_test.c - 文件时间功能测试（修复版本）
#include "fs.h"
#include "printf.h"
#include "file_time.h"
#include "bio.h"
#include "log.h"
#include "file.h"
#include "fs.h"

// 测试文件创建时间记录功能
void test_file_creation_time(void) {
    printf("=== Testing File Creation Time Recording ===\n");
    
    // 初始化文件系统和时间系统
    binit();
    fsinit(ROOTDEV);
    init_file_time_system();
    
    printf("1. Creating test files with time recording...\n");
    
    // 创建第一个测试文件
    begin_op();
    struct inode *file1 = ialloc(ROOTDEV, T_FILE);
    if (file1) {
        record_file_creation_time(file1);
        display_file_time_info(file1);
        
        // 写入一些测试数据
        char data1[] = "First file";
        writei(file1, 0, (uint64_t)data1, 0, sizeof(data1));
        iupdate(file1);
    }
    end_op();
    
    // 短暂延迟，确保时间不同
    for (volatile int i = 0; i < 10000; i++);
    
    // 创建第二个测试文件
    begin_op();
    struct inode *file2 = ialloc(ROOTDEV, T_FILE);
    if (file2) {
        record_file_creation_time(file2);
        display_file_time_info(file2);
        
        // 写入一些测试数据
        char data2[] = "Second file";
        writei(file2, 0, (uint64_t)data2, 0, sizeof(data2));
        iupdate(file2);
    }
    end_op();
    
    printf("2. Testing file age comparison...\n");
    
    if (file1 && file2) {
        int comparison = compare_file_ages(file1, file2);
        printf("File age comparison result: %d\n", comparison);
        
        if (comparison == -1) {
            printf("✓ File 1 is older than File 2 - CORRECT\n");
        } else if (comparison == 1) {
            printf("✓ File 2 is older than File 1\n");
        } else {
            printf("Files were created at the same time\n");
        }
        
        // 验证时间信息
        printf("3. Testing time information access...\n");
        
        uint64_t time1 = get_file_creation_time(file1);
        uint64_t time2 = get_file_creation_time(file2);
        
        printf("File 1 creation time: %lu\n", time1);
        printf("File 2 creation time: %lu\n", time2);
        
        if (time1 > 0 && time2 > 0 && time1 < time2) {
            printf("✓ Time information correctly recorded and ordered\n");
        } else {
            printf("✗ Time information issue detected\n");
        }
        
        // 清理
        begin_op();
        itrunc(file1);
        itrunc(file2);
        file1->type = 0;
        file2->type = 0;
        iupdate(file1);
        iupdate(file2);
        end_op();
        
        iput(file1);
        iput(file2);
    }
    
    printf("=== File Creation Time Test Completed ===\n\n");
}

// 批量文件创建时间测试
void test_batch_file_times(void) {
    printf("=== Testing Batch File Creation Times ===\n");
    
    struct inode *files[5];
    uint64_t creation_times[5];
    
    printf("Creating 5 files with recorded creation times...\n");
    
    // 创建多个文件并记录时间
    for (int i = 0; i < 5; i++) {
        begin_op();
        files[i] = ialloc(ROOTDEV, T_FILE);
        if (files[i]) {
            record_file_creation_time(files[i]);
            creation_times[i] = get_file_creation_time(files[i]);
            
            char data[32];
            // 简单的手动字符串构建
            data[0] = 'F'; data[1] = 'i'; data[2] = 'l'; data[3] = 'e';
            data[4] = '0' + i; data[5] = '\0';
            
            writei(files[i], 0, (uint64_t)data, 0, 6);
            iupdate(files[i]);
            
            printf("  Created file %d (inode %d): time=%lu\n", 
                   i, files[i]->inum, creation_times[i]);
        }
        end_op();
        
        // 小延迟确保不同时间
        for (volatile int j = 0; j < 1000; j++);
    }
    
    printf("Verifying creation time order...\n");
    
    // 验证时间顺序（应该递增）
    int correct_order = 1;
    for (int i = 1; i < 5; i++) {
        if (files[i-1] && files[i]) {
            if (creation_times[i-1] >= creation_times[i]) {
                printf("✗ Time order incorrect: file%d=%lu, file%d=%lu\n", 
                       i-1, creation_times[i-1], i, creation_times[i]);
                correct_order = 0;
            }
        }
    }
    
    if (correct_order) {
        printf("✓ All files created in correct time order\n");
    }
    
    // 测试时间信息持久性（在同一个运行会话中）
    printf("Testing time information persistence within session...\n");
    int persistence_ok = 1;
    for (int i = 0; i < 5; i++) {
        if (files[i]) {
            uint64_t reloaded_time = get_file_creation_time(files[i]);
            if (reloaded_time != creation_times[i]) {
                printf("✗ Time persistence failed for file %d\n", i);
                persistence_ok = 0;
            }
        }
    }
    
    if (persistence_ok) {
        printf("✓ Time information persisted correctly within session\n");
    }
    
    // 清理
    for (int i = 0; i < 5; i++) {
        if (files[i]) {
            begin_op();
            itrunc(files[i]);
            files[i]->type = 0;
            iupdate(files[i]);
            end_op();
            iput(files[i]);
        }
    }
    
    printf("=== Batch File Times Test Completed ===\n\n");
}

// 测试边界情况
void test_edge_cases(void) {
    printf("=== Testing Edge Cases ===\n");
    
    init_file_time_system();
    
    printf("1. Testing NULL pointer handling...\n");
    record_file_creation_time(NULL);
    // uint64_t time = get_file_creation_time(NULL);
    display_file_time_info(NULL);
    int comparison = compare_file_ages(NULL, NULL);
    printf("NULL comparison result: %d (expected 0)\n", comparison);
    
    printf("2. Testing invalid inode numbers...\n");
    struct inode fake_inode;
    fake_inode.inum = NINODE + 100;  // 无效的inode号
    record_file_creation_time(&fake_inode);
    
    printf("3. Testing unrecorded file...\n");
    binit();
    fsinit(ROOTDEV);
    
    begin_op();
    struct inode *file = ialloc(ROOTDEV, T_FILE);
    if (file) {
        // 不调用 record_file_creation_time
        uint64_t unrecorded_time = get_file_creation_time(file);
        printf("Unrecorded file time: %lu (expected 0)\n", unrecorded_time);
        display_file_time_info(file);
        
        // 清理
        itrunc(file);
        file->type = 0;
        iupdate(file);
        iput(file);
    }
    end_op();
    
    printf("=== Edge Cases Test Completed ===\n\n");
}

// 运行所有文件时间测试
void run_file_time_tests(void) {
    printf("\n");
    printf("========================================\n");
    printf("  File Creation Time Extension Tests\n");
    printf("========================================\n\n");
    
    test_file_creation_time();
    test_batch_file_times();
    test_edge_cases();
    
    printf("========================================\n");
    printf("  All File Time Tests Completed\n");
    printf("========================================\n\n");
}