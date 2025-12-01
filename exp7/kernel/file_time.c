// kernel/file_time.c - 文件创建时间记录扩展（修复版本）
#include "fs.h"
#include "printf.h"
#include "clock.h"

// static uint64_t file_creation_times[NINODE];  // 为每个可能的inode存储创建时间
// 文件创建时间记录功能
void record_file_creation_time(struct inode *ip) {
    if (!ip || ip->inum < 1) return;
    
    // 获取当前时间
    uint64_t current_time = 0;
    asm volatile("csrr %0, time" : "=r"(current_time));
    
    ip->ctime = current_time;
    iupdate(ip);  // 立即写回磁盘，确保时间持久化
    
    printf("FILE_TIME: Recorded creation time for inode %d: %lu\n", 
           ip->inum, current_time);
}

// 获取文件创建时间
uint64_t get_file_creation_time(struct inode *ip) {
    if (!ip || ip->inum < 1) return 0;
    
    return ip->ctime;
}

// 显示文件时间信息
void display_file_time_info(struct inode *ip) {
    if (!ip) return;
    
    uint64_t create_time = get_file_creation_time(ip);
    
    printf("FILE_TIME: Inode %d - ", ip->inum);
    if (create_time > 0) {
        printf("Created at: %lu cycles\n", create_time);
    } else {
        printf("Creation time not recorded\n");
    }
}

// 文件时间比较函数
int compare_file_ages(struct inode *ip1, struct inode *ip2) {
    if (!ip1 || !ip2) return 0;
    
    uint64_t time1 = get_file_creation_time(ip1);
    uint64_t time2 = get_file_creation_time(ip2);
    
    if (time1 == 0 || time2 == 0) return 0;  // 如果时间未记录，认为相等
    
    if (time1 < time2) return -1;  // ip1更老
    if (time1 > time2) return 1;   // ip1更新
    return 0;                      // 同时创建
}

// 初始化文件时间系统
void init_file_time_system(void) {
    // 目前使用 inode 内嵌的时间字段，无需额外初始化
    printf("FILE_TIME: File time system ready (timestamps stored in inodes)\n");
}