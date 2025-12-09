// include/file_time.h - 文件时间功能头文件
#ifndef _FILE_TIME_H_
#define _FILE_TIME_H_

#include "types.h"

struct inode;

// 函数声明
void init_file_time_system(void);
void record_file_creation_time(struct inode *ip);
uint64_t get_file_creation_time(struct inode *ip);
void display_file_time_info(struct inode *ip);
int compare_file_ages(struct inode *ip1, struct inode *ip2);

#endif // _FILE_TIME_H_