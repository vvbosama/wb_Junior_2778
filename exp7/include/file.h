// kernel/file.h - 文件描述符管理头文件
#ifndef _FILE_H_
#define _FILE_H_

#include "fs.h"

// 文件操作函数声明
struct file* filealloc(void);
void fileclose(struct file *f);
struct file* filedup(struct file *f);
int fileread(struct file *f, uint64_t addr, int n);
int filewrite(struct file *f, uint64_t addr, int n);
int filestat(struct file *f, uint64_t addr);

#endif // _FILE_H_

